#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <memory>

#include "tt-metalium/host_api.hpp"
#include "tt-metalium/mesh_device.hpp"
#include "tt-metalium/mesh_command_queue.hpp"
#include "tt-metalium/mesh_buffer.hpp"
#include "tt-metalium/mesh_workload.hpp"

using namespace tt;
using namespace tt::tt_metal;

static std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "[EdgeBox-TT] Impossible d'ouvrir : " << path << std::endl;
        std::exit(2);
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static uint32_t round_up(uint32_t v, uint32_t a) { return ((v + a - 1) / a) * a; }

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: run_bitnet <test_vector_dir> [M] [K]" << std::endl;
        std::cerr << "  ex : run_bitnet ../reference/test_vectors/test01_random 32 64" << std::endl;
        return 2;
    }
    std::string dir = argv[1];
    uint32_t M = (argc > 2) ? static_cast<uint32_t>(std::stoul(argv[2])) : 32;
    uint32_t K = (argc > 3) ? static_cast<uint32_t>(std::stoul(argv[3])) : 64;

    // --- Chargement des vecteurs de référence ---
    auto act_bytes = read_file(dir + "/activation_int8.bin");            // K octets
    auto w_bytes   = read_file(dir + "/weight_ternary_packed_2bit.bin"); // ceil(M*K/4)
    auto exp_bytes = read_file(dir + "/expected_output_int32.bin");      // M*4

    uint32_t exp_packed = (M * K + 3) / 4;
    if (act_bytes.size() < K || w_bytes.size() < exp_packed || exp_bytes.size() < M * 4) {
        std::cerr << "[EdgeBox-TT] Tailles de fichiers incohérentes avec M=" << M << " K=" << K << std::endl;
        std::cerr << "  activation=" << act_bytes.size() << " (>=" << K << ")"
                  << " weights=" << w_bytes.size() << " (>=" << exp_packed << ")"
                  << " expected=" << exp_bytes.size() << " (>=" << M * 4 << ")" << std::endl;
        return 2;
    }
    const int32_t* expected = reinterpret_cast<const int32_t*>(exp_bytes.data());

    // --- Tailles de pages (alignées DRAM) ---
    const uint32_t ALIGN = 64;
    uint32_t act_page = round_up(K, ALIGN);
    uint32_t w_page   = round_up(exp_packed, ALIGN);
    uint32_t out_page = round_up(M * 4, ALIGN);

    // --- Device ---
    int device_id = 0;
    auto mesh_device = distributed::MeshDevice::create_unit_mesh(device_id);
    distributed::MeshCommandQueue& cq = mesh_device->mesh_command_queue();

    Program program = CreateProgram();
    CoreCoord compute_core = {0, 0};

    // --- Allocation DRAM ---
    distributed::DeviceLocalBufferConfig act_local{.page_size = act_page, .buffer_type = BufferType::DRAM};
    distributed::ReplicatedBufferConfig act_repl{.size = act_page};
    auto act_dram = distributed::MeshBuffer::create(act_repl, act_local, mesh_device.get());

    distributed::DeviceLocalBufferConfig w_local{.page_size = w_page, .buffer_type = BufferType::DRAM};
    distributed::ReplicatedBufferConfig w_repl{.size = w_page};
    auto w_dram = distributed::MeshBuffer::create(w_repl, w_local, mesh_device.get());

    distributed::DeviceLocalBufferConfig out_local{.page_size = out_page, .buffer_type = BufferType::DRAM};
    distributed::ReplicatedBufferConfig out_repl{.size = out_page};
    auto out_dram = distributed::MeshBuffer::create(out_repl, out_local, mesh_device.get());

    // --- Circular buffers L1 (format Float32 forcé pour DMA brute) ---
    CircularBufferConfig cb_act_cfg = CircularBufferConfig(act_page * 2, {{tt::CB::c_in0, tt::DataFormat::Float32}}).set_page_size(tt::CB::c_in0, act_page);
    CreateCircularBuffer(program, compute_core, cb_act_cfg);

    CircularBufferConfig cb_w_cfg = CircularBufferConfig(w_page * 2, {{tt::CB::c_in1, tt::DataFormat::Float32}}).set_page_size(tt::CB::c_in1, w_page);
    CreateCircularBuffer(program, compute_core, cb_w_cfg);

    CircularBufferConfig cb_out_cfg = CircularBufferConfig(out_page, {{tt::CB::c_out0, tt::DataFormat::Float32}}).set_page_size(tt::CB::c_out0, out_page);
    CreateCircularBuffer(program, compute_core, cb_out_cfg);

    // --- Kernels ---
    KernelHandle reader_kernel = CreateKernel(
        program, "kernels/reader.cpp", compute_core,
        DataMovementConfig{.processor = DataMovementProcessor::RISCV_0, .noc = NOC::RISCV_0_default});

    KernelHandle writer_kernel = CreateKernel(
        program, "kernels/writer.cpp", compute_core,
        DataMovementConfig{.processor = DataMovementProcessor::RISCV_1, .noc = NOC::RISCV_1_default});

    // Kernel compute présent mais vide (le calcul vit dans le writer)
    CreateKernel(program, "kernels/compute_bitlinear.cpp", compute_core, ComputeConfig{});

    SetRuntimeArgs(program, reader_kernel, compute_core, {act_dram->address(), w_dram->address(), 1});
    SetRuntimeArgs(program, writer_kernel, compute_core, {out_dram->address(), M, K, out_page});

    // --- Données hôte (paddées à la page) ---
    std::vector<uint8_t> host_act(act_page, 0);
    std::copy(act_bytes.begin(), act_bytes.begin() + K, host_act.begin());
    std::vector<uint8_t> host_w(w_page, 0);
    std::copy(w_bytes.begin(), w_bytes.begin() + exp_packed, host_w.begin());

    std::cout << "[EdgeBox-TT] Test : " << dir << "  (M=" << M << ", K=" << K << ")" << std::endl;
    std::cout << "[EdgeBox-TT] Écriture des données via MeshBuffer..." << std::endl;
    cq.enqueue_write_mesh_buffer(act_dram, host_act.data(), false);
    cq.enqueue_write_mesh_buffer(w_dram, host_w.data(), false);

    std::cout << "[EdgeBox-TT] Lancement de la topologie MeshWorkload..." << std::endl;
    distributed::MeshWorkload workload;
    distributed::MeshCoordinateRange device_range(mesh_device->shape());
    workload.add_program(device_range, std::move(program));
    distributed::EnqueueMeshWorkload(cq, workload, false);
    cq.finish();

    // --- Lecture résultats ---
    std::vector<uint8_t> host_out(out_page, 0);
    std::cout << "[EdgeBox-TT] Rapatriement des résultats de la DRAM..." << std::endl;
    cq.enqueue_read_mesh_buffer(host_out.data(), out_dram, true);
    const int32_t* got = reinterpret_cast<const int32_t*>(host_out.data());

    // --- Comparaison ---
    uint32_t mismatches = 0;
    std::cout << "\n=== COMPARAISON (M=" << M << ") ===" << std::endl;
    for (uint32_t m = 0; m < M; ++m) {
        if (got[m] != expected[m]) {
            if (mismatches < 16) {
                std::cout << "  MISMATCH m=" << m << " : got=" << got[m]
                          << " attendu=" << expected[m] << std::endl;
            }
            mismatches++;
        }
    }

    std::cout << "\n=== RÉSULTAT ===" << std::endl;
    if (mismatches == 0) {
        std::cout << "[Test PASS] " << dir << " : " << M << "/" << M << " sorties correctes." << std::endl;
    } else {
        std::cout << "[Test FAIL] " << dir << " : " << mismatches << "/" << M << " sorties incorrectes." << std::endl;
    }

    mesh_device->close();
    return (mismatches == 0) ? 0 : 1;
}
