// Benchmark BitLinear MULTI-COEUR (matmul reparti sur la grille Tensix).
// Meme calcul que bench_mm.cpp (batch N), mais les tuiles de sortie sont distribuees
// via split_work_to_cores -> montre le gain de la parallelisation multi-coeur.
#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <random>
#include <chrono>
#include <fstream>
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
using clk = std::chrono::high_resolution_clock;

int main(int argc, char** argv) {
    uint32_t M     = (argc > 1) ? std::stoul(argv[1]) : 512;
    uint32_t K     = (argc > 2) ? std::stoul(argv[2]) : 512;
    uint32_t N     = (argc > 3) ? std::stoul(argv[3]) : 32;
    uint32_t iters = (argc > 4) ? std::stoul(argv[4]) : 500;
    uint32_t warmup = 20;

    auto ru = [](uint32_t v, uint32_t a){ return ((v + a - 1) / a) * a; };
    uint32_t Mp = ru(M, TILE_HEIGHT), Kp = ru(K, TILE_WIDTH), Np = ru(N, TILE_WIDTH);
    uint32_t Mt = Mp / TILE_HEIGHT, Kt = Kp / TILE_WIDTH, Nt = Np / TILE_WIDTH;

    std::mt19937 rng(4321);
    std::uniform_int_distribution<int> act_d(-8, 8), tern_d(-1, 1);
    std::vector<int8_t> Wt(Mp * Kp, 0), X(Kp * Np, 0);
    std::vector<bfloat16> A_rm(Mp * Kp, bfloat16(0.0f)), B_rm(Kp * Np, bfloat16(0.0f));
    for (uint32_t m = 0; m < M; ++m) for (uint32_t k = 0; k < K; ++k) { int8_t w = tern_d(rng); Wt[m*Kp+k]=w; A_rm[m*Kp+k]=bfloat16((float)w); }
    for (uint32_t k = 0; k < K; ++k) for (uint32_t nn = 0; nn < N; ++nn) { int8_t x = act_d(rng); X[k*Np+nn]=x; B_rm[k*Np+nn]=bfloat16((float)x); }
    std::vector<double> gold(M * N, 0.0);
    for (uint32_t m = 0; m < M; ++m) for (uint32_t nn = 0; nn < N; ++nn) { long a=0; for (uint32_t k=0;k<K;++k) a += (long)Wt[m*Kp+k]*(long)X[k*Np+nn]; gold[m*N+nn]=(double)a; }

    std::vector<bfloat16> A = tilize_nfaces(A_rm, Mp, Kp);
    std::vector<bfloat16> B = tilize_nfaces(B_rm, Kp, Np);

    auto mesh_device = distributed::MeshDevice::create_unit_mesh(0);
    distributed::MeshCommandQueue& cq = mesh_device->mesh_command_queue();
    distributed::MeshWorkload workload;
    distributed::MeshCoordinateRange range(mesh_device->shape());
    Program program{};

    auto core_grid = mesh_device->compute_with_storage_grid_size();
    uint32_t num_output_tiles = Mt * Nt;
    auto [num_cores, all_cores, cg1, cg2, wpc1, wpc2] = split_work_to_cores(core_grid, num_output_tiles);

    uint32_t in_tile = sizeof(bfloat16) * TILE_HW, out_tile = sizeof(float) * TILE_HW;
    distributed::DeviceLocalBufferConfig in_dram{.page_size = in_tile, .buffer_type = BufferType::DRAM};
    distributed::DeviceLocalBufferConfig out_dram{.page_size = out_tile, .buffer_type = BufferType::DRAM};
    auto A_buf = distributed::MeshBuffer::create(distributed::ReplicatedBufferConfig{.size = in_tile * Mt * Kt}, in_dram, mesh_device.get());
    auto B_buf = distributed::MeshBuffer::create(distributed::ReplicatedBufferConfig{.size = in_tile * Kt * Nt}, in_dram, mesh_device.get());
    auto C_buf = distributed::MeshBuffer::create(distributed::ReplicatedBufferConfig{.size = out_tile * Mt * Nt}, out_dram, mesh_device.get());

    CreateCircularBuffer(program, all_cores, CircularBufferConfig(2*in_tile,  {{CBIndex::c_0,  tt::DataFormat::Float16_b}}).set_page_size(CBIndex::c_0, in_tile));
    CreateCircularBuffer(program, all_cores, CircularBufferConfig(2*in_tile,  {{CBIndex::c_1,  tt::DataFormat::Float16_b}}).set_page_size(CBIndex::c_1, in_tile));
    CreateCircularBuffer(program, all_cores, CircularBufferConfig(2*out_tile, {{CBIndex::c_16, tt::DataFormat::Float32}}).set_page_size(CBIndex::c_16, out_tile));

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
        for (const auto& r : ranges.ranges())
            for (const auto& core : r) {
                SetRuntimeArgs(program, reader, core, {A_buf->address(), B_buf->address(), Mt, Kt, Nt, off, wpc});
                SetRuntimeArgs(program, writer, core, {C_buf->address(), wpc, off});
                SetRuntimeArgs(program, compute, core, {wpc, Kt});
                off += wpc;
            }

    distributed::EnqueueWriteMeshBuffer(cq, A_buf, A, false);
    distributed::EnqueueWriteMeshBuffer(cq, B_buf, B, false);
    workload.add_program(range, std::move(program));

    for (uint32_t r = 0; r < warmup; ++r) distributed::EnqueueMeshWorkload(cq, workload, false);
    cq.finish();

    auto d0 = clk::now();
    for (uint32_t r = 0; r < iters; ++r) distributed::EnqueueMeshWorkload(cq, workload, false);
    cq.finish();
    auto d1 = clk::now();
    double dev_us = std::chrono::duration<double, std::micro>(d1 - d0).count() / iters;

    std::vector<float> C(Mt * Nt * TILE_HW, 0.0f);
    distributed::EnqueueReadMeshBuffer(cq, C, C_buf, true);
    std::vector<float> C_rm = untilize_nfaces(C, Mp, Np);
    double sx=0,sy=0,sxx=0,syy=0,sxy=0; uint32_t cnt=0;
    for (uint32_t m=0;m<M;++m) for (uint32_t nn=0;nn<N;++nn){ double a=std::lround(C_rm[m*Np+nn]), b=gold[m*N+nn]; sx+=a;sy+=b;sxx+=a*a;syy+=b*b;sxy+=a*b;cnt++; }
    double dn=cnt, de=std::sqrt((dn*sxx-sx*sx)*(dn*syy-sy*sy)); double pcc=(de>0)?(dn*sxy-sx*sy)/de:1.0;

    mesh_device->close();

    double macs = (double)M * K * N;
    double gops = (2.0 * macs) / (dev_us * 1e-6) / 1e9;

    std::cout << "\n=== BENCH multi-coeur ===" << std::endl;
    std::cout << "  M=" << M << " K=" << K << " N=" << N << "  coeurs=" << num_cores
              << "  MAC/appel=" << (uint64_t)macs << "  PCC=" << pcc << std::endl;
    std::cout << "  Latence device : " << dev_us << " us/appel" << std::endl;
    std::cout << "  Debit device   : " << gops << " GOP/s" << std::endl;

    const char* csv = "benchmarks/bitlinear_mc_results.csv";
    bool ex = std::ifstream(csv).good();
    std::ofstream out(csv, std::ios::app);
    if (out) {
        if (!ex) out << "M,K,N,cores,mac_per_call,iters,device_lat_us,device_gops,pcc\n";
        out << M << "," << K << "," << N << "," << num_cores << "," << (uint64_t)macs << ","
            << iters << "," << dev_us << "," << gops << "," << pcc << "\n";
        std::cout << "  -> ligne ajoutee a " << csv << std::endl;
    }
    return 0;
}
