#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <random>
#include <chrono>
#include <fstream>
#include <memory>

#include "tt-metalium/host_api.hpp"
#include "tt-metalium/mesh_device.hpp"
#include "tt-metalium/mesh_command_queue.hpp"
#include "tt-metalium/mesh_buffer.hpp"
#include "tt-metalium/mesh_workload.hpp"

using namespace tt;
using namespace tt::tt_metal;
using clk = std::chrono::high_resolution_clock;

static const int8_t decode_table[4] = {0, 1, -1, 0};

// Référence CPU : out[m] = sum_k decode(W[m][k]) * x[k]  (packé 2-bit, row-major)
static void bitlinear_cpu(const int8_t* x, const uint8_t* w, int32_t* y, uint32_t M, uint32_t K) {
    for (uint32_t m = 0; m < M; ++m) {
        int32_t acc = 0;
        for (uint32_t k = 0; k < K; ++k) {
            uint32_t idx = m * K + k;
            uint8_t two_bit = (w[idx >> 2] >> ((idx & 3) * 2)) & 0x03;
            acc += static_cast<int32_t>(decode_table[two_bit]) * static_cast<int32_t>(x[k]);
        }
        y[m] = acc;
    }
}

static uint32_t round_up(uint32_t v, uint32_t a) { return ((v + a - 1) / a) * a; }

int main(int argc, char** argv) {
    uint32_t M     = (argc > 1) ? static_cast<uint32_t>(std::stoul(argv[1])) : 32;
    uint32_t K     = (argc > 2) ? static_cast<uint32_t>(std::stoul(argv[2])) : 64;
    uint32_t iters = (argc > 3) ? static_cast<uint32_t>(std::stoul(argv[3])) : 1000;
    uint32_t warmup = 20;

    uint32_t exp_packed = (M * K + 3) / 4;
    // Le loader mono-tuile actuel ne charge qu'une tuile Float32 (4096 o) par entrée.
    if (exp_packed > 4096 || K > 4096 || M * 4 > 4096) {
        std::cerr << "[Bench] M*K trop grand pour le kernel mono-tuile actuel "
                     "(poids packés=" << exp_packed << " o > 4096). Choisis M,K plus petits." << std::endl;
        return 2;
    }

    // --- Données synthétiques ---
    std::mt19937 rng(1234);
    std::uniform_int_distribution<int> act_dist(-8, 8);
    std::uniform_int_distribution<int> tern_dist(0, 2); // 0->0, 1->+1, 2->-1

    std::vector<int8_t> act(K);
    for (auto& v : act) v = static_cast<int8_t>(act_dist(rng));

    // poids ternaires -> encodage 2-bit {0:00, +1:01, -1:10}
    std::vector<uint8_t> w_packed(exp_packed, 0);
    for (uint32_t idx = 0; idx < M * K; ++idx) {
        uint8_t code = static_cast<uint8_t>(tern_dist(rng)); // 0,1,2 == 00,01,10
        w_packed[idx >> 2] |= (code << ((idx & 3) * 2));
    }

    // --- Référence CPU (+ chrono CPU) ---
    std::vector<int32_t> cpu_out(M);
    auto c0 = clk::now();
    for (uint32_t r = 0; r < iters; ++r) bitlinear_cpu(act.data(), w_packed.data(), cpu_out.data(), M, K);
    auto c1 = clk::now();
    double cpu_lat_us = std::chrono::duration<double, std::micro>(c1 - c0).count() / iters;

    // --- Tailles de pages (alignées) ---
    const uint32_t ALIGN = 64;
    uint32_t act_page = round_up(K, ALIGN);
    uint32_t w_page   = round_up(exp_packed, ALIGN);
    uint32_t out_page = round_up(M * 4, ALIGN);

    // --- Device ---
    auto mesh_device = distributed::MeshDevice::create_unit_mesh(0);
    distributed::MeshCommandQueue& cq = mesh_device->mesh_command_queue();

    Program program = CreateProgram();
    CoreCoord core = {0, 0};

    distributed::DeviceLocalBufferConfig act_local{.page_size = act_page, .buffer_type = BufferType::DRAM};
    distributed::ReplicatedBufferConfig act_repl{.size = act_page};
    auto act_dram = distributed::MeshBuffer::create(act_repl, act_local, mesh_device.get());

    distributed::DeviceLocalBufferConfig w_local{.page_size = w_page, .buffer_type = BufferType::DRAM};
    distributed::ReplicatedBufferConfig w_repl{.size = w_page};
    auto w_dram = distributed::MeshBuffer::create(w_repl, w_local, mesh_device.get());

    distributed::DeviceLocalBufferConfig out_local{.page_size = out_page, .buffer_type = BufferType::DRAM};
    distributed::ReplicatedBufferConfig out_repl{.size = out_page};
    auto out_dram = distributed::MeshBuffer::create(out_repl, out_local, mesh_device.get());

    CircularBufferConfig cb_act = CircularBufferConfig(act_page * 2, {{tt::CB::c_in0, tt::DataFormat::Float32}}).set_page_size(tt::CB::c_in0, act_page);
    CreateCircularBuffer(program, core, cb_act);
    CircularBufferConfig cb_w = CircularBufferConfig(w_page * 2, {{tt::CB::c_in1, tt::DataFormat::Float32}}).set_page_size(tt::CB::c_in1, w_page);
    CreateCircularBuffer(program, core, cb_w);
    CircularBufferConfig cb_out = CircularBufferConfig(out_page, {{tt::CB::c_out0, tt::DataFormat::Float32}}).set_page_size(tt::CB::c_out0, out_page);
    CreateCircularBuffer(program, core, cb_out);

    KernelHandle reader = CreateKernel(program, "kernels/reader.cpp", core,
        DataMovementConfig{.processor = DataMovementProcessor::RISCV_0, .noc = NOC::RISCV_0_default});
    KernelHandle writer = CreateKernel(program, "kernels/writer.cpp", core,
        DataMovementConfig{.processor = DataMovementProcessor::RISCV_1, .noc = NOC::RISCV_1_default});
    CreateKernel(program, "kernels/compute_bitlinear.cpp", core, ComputeConfig{});

    SetRuntimeArgs(program, reader, core, {act_dram->address(), w_dram->address(), 1});
    SetRuntimeArgs(program, writer, core, {out_dram->address(), M, K, out_page});

    // Écriture des entrées (une fois)
    std::vector<uint8_t> host_act(act_page, 0);
    std::memcpy(host_act.data(), act.data(), K);
    std::vector<uint8_t> host_w(w_page, 0);
    std::memcpy(host_w.data(), w_packed.data(), exp_packed);
    cq.enqueue_write_mesh_buffer(act_dram, host_act.data(), false);
    cq.enqueue_write_mesh_buffer(w_dram, host_w.data(), false);

    distributed::MeshWorkload workload;
    distributed::MeshCoordinateRange range(mesh_device->shape());
    workload.add_program(range, std::move(program));

    // --- Warmup (déclenche le JIT + stabilise) ---
    for (uint32_t r = 0; r < warmup; ++r) distributed::EnqueueMeshWorkload(cq, workload, false);
    cq.finish();

    // --- Latence device seule : EnqueueMeshWorkload + finish ---
    auto d0 = clk::now();
    for (uint32_t r = 0; r < iters; ++r) distributed::EnqueueMeshWorkload(cq, workload, false);
    cq.finish();
    auto d1 = clk::now();
    double dev_lat_us = std::chrono::duration<double, std::micro>(d1 - d0).count() / iters;

    // --- Latence end-to-end : write entrées + enqueue + finish + read sortie ---
    std::vector<uint8_t> host_out(out_page, 0);
    auto e0 = clk::now();
    for (uint32_t r = 0; r < iters; ++r) {
        cq.enqueue_write_mesh_buffer(act_dram, host_act.data(), false);
        distributed::EnqueueMeshWorkload(cq, workload, false);
        cq.enqueue_read_mesh_buffer(host_out.data(), out_dram, true);
    }
    auto e1 = clk::now();
    double e2e_lat_us = std::chrono::duration<double, std::micro>(e1 - e0).count() / iters;

    // --- Vérification correction du run chronométré ---
    const int32_t* got = reinterpret_cast<const int32_t*>(host_out.data());
    uint32_t mism = 0;
    for (uint32_t m = 0; m < M; ++m) if (got[m] != cpu_out[m]) mism++;
    bool ok = (mism == 0);

    mesh_device->close();

    // --- Métriques ---
    double macs = static_cast<double>(M) * static_cast<double>(K);
    double dev_gops = (2.0 * macs) / (dev_lat_us * 1e-6) / 1e9; // 2 ops par MAC
    double dma_us   = e2e_lat_us - dev_lat_us;                  // overhead transfert (PCIe x4)
    double speedup  = cpu_lat_us / dev_lat_us;

    std::cout << "\n=== BENCHMARK BitLinear (Blackhole, 1 core, scalaire) ===" << std::endl;
    std::cout << "  M=" << M << "  K=" << K << "  MAC/appel=" << (uint64_t)macs
              << "  iters=" << iters << "  correct=" << (ok ? "OUI" : "NON") << std::endl;
    std::cout << "  Latence device   : " << dev_lat_us << " us/appel" << std::endl;
    std::cout << "  Latence e2e      : " << e2e_lat_us << " us/appel" << std::endl;
    std::cout << "  Overhead DMA/PCIe: " << dma_us << " us/appel" << std::endl;
    std::cout << "  Debit device     : " << dev_gops << " GOP/s" << std::endl;
    std::cout << "  Latence CPU (ref): " << cpu_lat_us << " us/appel  (speedup device x" << speedup << ")" << std::endl;

    // --- Sortie CSV (append) ---
    const char* csv = "benchmarks/bitlinear_hw_results.csv";
    bool exists = std::ifstream(csv).good();
    std::ofstream out(csv, std::ios::app);
    if (out) {
        if (!exists)
            out << "M,K,mac_per_call,iters,device_lat_us,e2e_lat_us,dma_us,device_gops,cpu_lat_us,speedup_vs_cpu,correct\n";
        out << M << "," << K << "," << (uint64_t)macs << "," << iters << ","
            << dev_lat_us << "," << e2e_lat_us << "," << dma_us << "," << dev_gops << ","
            << cpu_lat_us << "," << speedup << "," << (ok ? 1 : 0) << "\n";
        std::cout << "  -> ligne ajoutee a " << csv << std::endl;
    }

    return ok ? 0 : 1;
}
