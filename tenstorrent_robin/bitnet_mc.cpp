// BitLinear ternaire MULTI-COEUR sur Blackhole.
// Même calcul que bitnet_mm.cpp (matmul unité matricielle, entrées bf16, sortie fp32),
// mais les tuiles de sortie sont réparties sur plusieurs coeurs Tensix via split_work_to_cores.
// Kernels réutilisés tels quels : matmul_multi_core officiel tt-metal (kernels_mc/).
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <memory>

#include <tt-metalium/host_api.hpp>
#include <tt-metalium/constants.hpp>
#include <tt-metalium/bfloat16.hpp>
#include <tt-metalium/tilize_utils.hpp>
#include <tt-metalium/distributed.hpp>
#include <tt-metalium/work_split.hpp>
#include <tt-metalium/tensor_accessor_args.hpp>

using namespace tt;
using namespace tt::tt_metal;
using namespace tt::constants;

static const int8_t decode_table[4] = {0, 1, -1, 0};
static std::vector<uint8_t> read_file(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) { std::cerr << "[MC] Impossible d'ouvrir : " << p << std::endl; std::exit(2); }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
static uint32_t round_up(uint32_t v, uint32_t a) { return ((v + a - 1) / a) * a; }

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "Usage: run_bitnet_mc <test_vector_dir> [M] [K]" << std::endl; return 2; }
    std::string dir = argv[1];
    uint32_t M = (argc > 2) ? std::stoul(argv[2]) : 32;
    uint32_t K = (argc > 3) ? std::stoul(argv[3]) : 64;

    auto act_b = read_file(dir + "/activation_int8.bin");
    auto w_b   = read_file(dir + "/weight_ternary_packed_2bit.bin");
    auto exp_b = read_file(dir + "/expected_output_int32.bin");
    uint32_t exp_packed = (M * K + 3) / 4;
    if (act_b.size() < K || w_b.size() < exp_packed || exp_b.size() < M * 4) {
        std::cerr << "[MC] Tailles de fichiers incoherentes." << std::endl; return 2;
    }
    const int8_t*  act = reinterpret_cast<const int8_t*>(act_b.data());
    const int32_t* expected = reinterpret_cast<const int32_t*>(exp_b.data());

    uint32_t Mp = round_up(M, TILE_HEIGHT), Kp = round_up(K, TILE_WIDTH), N = TILE_WIDTH;
    uint32_t Mt = Mp / TILE_HEIGHT, Kt = Kp / TILE_WIDTH, Nt = N / TILE_WIDTH;

    std::vector<bfloat16> A_rm(Mp * Kp, bfloat16(0.0f));
    for (uint32_t m = 0; m < M; ++m)
        for (uint32_t k = 0; k < K; ++k) {
            uint32_t idx = m * K + k;
            uint8_t tb = (w_b[idx >> 2] >> ((idx & 3) * 2)) & 0x03;
            A_rm[m * Kp + k] = bfloat16(static_cast<float>(decode_table[tb]));
        }
    std::vector<bfloat16> B_rm(Kp * N, bfloat16(0.0f));
    for (uint32_t k = 0; k < K; ++k) B_rm[k * N + 0] = bfloat16(static_cast<float>(act[k]));

    std::vector<bfloat16> A = tilize_nfaces(A_rm, Mp, Kp);
    std::vector<bfloat16> B = tilize_nfaces(B_rm, Kp, N);

    auto mesh_device = distributed::MeshDevice::create_unit_mesh(0);
    distributed::MeshCommandQueue& cq = mesh_device->mesh_command_queue();
    distributed::MeshWorkload workload;
    distributed::MeshCoordinateRange device_range(mesh_device->shape());
    Program program{};

    auto core_grid = mesh_device->compute_with_storage_grid_size();
    uint32_t num_output_tiles = Mt * Nt;
    auto [num_cores, all_cores, cg1, cg2, wpc1, wpc2] = split_work_to_cores(core_grid, num_output_tiles);

    uint32_t in_tile  = sizeof(bfloat16) * TILE_HW;   // 2048
    uint32_t out_tile = sizeof(float)    * TILE_HW;   // 4096

    distributed::DeviceLocalBufferConfig in_dram{.page_size = in_tile, .buffer_type = BufferType::DRAM};
    distributed::DeviceLocalBufferConfig out_dram{.page_size = out_tile, .buffer_type = BufferType::DRAM};
    auto A_buf = distributed::MeshBuffer::create(distributed::ReplicatedBufferConfig{.size = in_tile * Mt * Kt}, in_dram, mesh_device.get());
    auto B_buf = distributed::MeshBuffer::create(distributed::ReplicatedBufferConfig{.size = in_tile * Kt * Nt}, in_dram, mesh_device.get());
    auto C_buf = distributed::MeshBuffer::create(distributed::ReplicatedBufferConfig{.size = out_tile * Mt * Nt}, out_dram, mesh_device.get());

    CreateCircularBuffer(program, all_cores, CircularBufferConfig(2 * in_tile,  {{CBIndex::c_0,  tt::DataFormat::Float16_b}}).set_page_size(CBIndex::c_0, in_tile));
    CreateCircularBuffer(program, all_cores, CircularBufferConfig(2 * in_tile,  {{CBIndex::c_1,  tt::DataFormat::Float16_b}}).set_page_size(CBIndex::c_1, in_tile));
    CreateCircularBuffer(program, all_cores, CircularBufferConfig(2 * out_tile, {{CBIndex::c_16, tt::DataFormat::Float32}}).set_page_size(CBIndex::c_16, out_tile));

    std::vector<uint32_t> rct; TensorAccessorArgs(*A_buf).append_to(rct); TensorAccessorArgs(*B_buf).append_to(rct);
    auto reader = CreateKernel(program, "kernels_mc/reader_mc.cpp", all_cores,
        DataMovementConfig{.processor = DataMovementProcessor::RISCV_1, .noc = NOC::RISCV_1_default, .compile_args = rct});
    std::vector<uint32_t> wct; TensorAccessorArgs(*C_buf).append_to(wct);
    auto writer = CreateKernel(program, "kernels_mc/writer_mc.cpp", all_cores,
        DataMovementConfig{.processor = DataMovementProcessor::RISCV_0, .noc = NOC::RISCV_0_default, .compile_args = wct});
    auto compute = CreateKernel(program, "kernels_mc/mm_mc.cpp", all_cores,
        ComputeConfig{.math_fidelity = MathFidelity::HiFi4, .compile_args = {}});

    uint32_t off = 0;
    auto groups = {std::make_pair(cg1, wpc1), std::make_pair(cg2, wpc2)};
    for (const auto& [ranges, wpc] : groups)
        for (const auto& range : ranges.ranges())
            for (const auto& core : range) {
                SetRuntimeArgs(program, reader, core, {A_buf->address(), B_buf->address(), Mt, Kt, Nt, off, wpc});
                SetRuntimeArgs(program, writer, core, {C_buf->address(), wpc, off});
                SetRuntimeArgs(program, compute, core, {wpc, Kt});
                off += wpc;
            }

    std::cout << "[MC] Test : " << dir << "  (M=" << M << " K=" << K
              << ")  tuiles sortie=" << num_output_tiles << "  coeurs utilises=" << num_cores << std::endl;

    distributed::EnqueueWriteMeshBuffer(cq, A_buf, A, false);
    distributed::EnqueueWriteMeshBuffer(cq, B_buf, B, false);
    workload.add_program(device_range, std::move(program));
    distributed::EnqueueMeshWorkload(cq, workload, false);

    std::vector<float> C(Mt * Nt * TILE_HW, 0.0f);
    distributed::EnqueueReadMeshBuffer(cq, C, C_buf, true);
    std::vector<float> C_rm = untilize_nfaces(C, Mp, N);

    uint32_t mism = 0; int32_t max_abs = 0;
    double sx=0,sy=0,sxx=0,syy=0,sxy=0;
    for (uint32_t m = 0; m < M; ++m) {
        int32_t got = static_cast<int32_t>(std::lround(C_rm[m * N + 0]));
        int32_t exp = expected[m];
        if (got != exp) { if (mism < 12) std::cout << "  MISMATCH m=" << m << " got=" << got << " exp=" << exp << std::endl; mism++; }
        int32_t d = std::abs(got - exp); if (d > max_abs) max_abs = d;
        double a = got, b = exp; sx += a; sy += b; sxx += a*a; syy += b*b; sxy += a*b;
    }
    double n = M, den = std::sqrt((n*sxx - sx*sx) * (n*syy - sy*sy));
    double pcc = (den > 0) ? (n*sxy - sx*sy) / den : 1.0;

    std::cout << "\n=== RESULTAT (multi-coeur, " << num_cores << " coeurs) ===" << std::endl;
    std::cout << "  PCC=" << pcc << "  ecart_abs_max=" << max_abs << "  exact=" << (M - mism) << "/" << M << std::endl;
    bool ok = (mism == 0) || (pcc >= 0.999);
    std::cout << (ok ? "[Test PASS] " : "[Test FAIL] ") << dir
              << (mism == 0 ? " : bit-exact." : (ok ? " : PCC>=0.999 (bf16)." : " : ecart trop grand.")) << std::endl;

    mesh_device->close();
    return ok ? 0 : 1;
}
