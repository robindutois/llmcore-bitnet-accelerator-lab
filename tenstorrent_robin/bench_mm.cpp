// Benchmark de la version unite matricielle du BitLinear (matmul_tiles).
// Contrairement au scalaire (1 vecteur), on remplit les N colonnes -> batch de N
// activations traitees d'un coup, ce qui est le cas reel d'inference et exploite
// pleinement l'unite matricielle. Mesure latence device, e2e, debit, latence/vecteur.
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
#include <tt-metalium/tensor_accessor_args.hpp>

using namespace tt;
using namespace tt::tt_metal;
using namespace tt::constants;
using clk = std::chrono::high_resolution_clock;

int main(int argc, char** argv) {
    uint32_t M     = (argc > 1) ? static_cast<uint32_t>(std::stoul(argv[1])) : 128;
    uint32_t K     = (argc > 2) ? static_cast<uint32_t>(std::stoul(argv[2])) : 128;
    uint32_t N     = (argc > 3) ? static_cast<uint32_t>(std::stoul(argv[3])) : 32;   // batch
    uint32_t iters = (argc > 4) ? static_cast<uint32_t>(std::stoul(argv[4])) : 1000;
    uint32_t warmup = 20;

    // Dimensions tuilees (multiples de 32)
    auto ru = [](uint32_t v, uint32_t a){ return ((v + a - 1) / a) * a; };
    uint32_t Mp = ru(M, TILE_HEIGHT), Kp = ru(K, TILE_WIDTH), Np = ru(N, TILE_WIDTH);
    uint32_t Mt = Mp / TILE_HEIGHT, Kt = Kp / TILE_WIDTH, Nt = Np / TILE_WIDTH;

    // --- Donnees synthetiques : W ternaire (Mp x Kp), X int8 (Kp x Np) ---
    std::mt19937 rng(4321);
    std::uniform_int_distribution<int> act_dist(-8, 8), tern_dist(-1, 1);
    std::vector<int8_t>   Wt(Mp * Kp, 0);
    std::vector<bfloat16> A_rm(Mp * Kp, bfloat16(0.0f));
    std::vector<bfloat16> B_rm(Kp * Np, bfloat16(0.0f));
    for (uint32_t m = 0; m < M; ++m)
        for (uint32_t k = 0; k < K; ++k) { int8_t w = tern_dist(rng); Wt[m*Kp+k]=w; A_rm[m*Kp+k]=bfloat16((float)w); }
    std::vector<int8_t> X(Kp * Np, 0);
    for (uint32_t k = 0; k < K; ++k)
        for (uint32_t nn = 0; nn < N; ++nn) { int8_t x = act_dist(rng); X[k*Np+nn]=x; B_rm[k*Np+nn]=bfloat16((float)x); }

    // Golden CPU (pour PCC)
    std::vector<double> gold(M * N, 0.0);
    for (uint32_t m = 0; m < M; ++m)
        for (uint32_t nn = 0; nn < N; ++nn) {
            long acc = 0;
            for (uint32_t k = 0; k < K; ++k) acc += (long)Wt[m*Kp+k] * (long)X[k*Np+nn];
            gold[m*N+nn] = (double)acc;
        }

    std::vector<bfloat16> A = tilize_nfaces(A_rm, Mp, Kp);
    std::vector<bfloat16> B = tilize_nfaces(B_rm, Kp, Np);

    // --- Device ---
    auto mesh_device = distributed::MeshDevice::create_unit_mesh(0);
    distributed::MeshCommandQueue& cq = mesh_device->mesh_command_queue();
    distributed::MeshWorkload workload;
    distributed::MeshCoordinateRange range(mesh_device->shape());
    Program program{};
    CoreCoord core({0, 0});

    uint32_t in_tile  = sizeof(bfloat16) * TILE_HEIGHT * TILE_WIDTH;
    uint32_t out_tile = sizeof(float)    * TILE_HEIGHT * TILE_WIDTH;

    distributed::DeviceLocalBufferConfig in_dram{.page_size = in_tile, .buffer_type = BufferType::DRAM};
    distributed::DeviceLocalBufferConfig out_dram{.page_size = out_tile, .buffer_type = BufferType::DRAM};
    distributed::ReplicatedBufferConfig cfgA{.size = sizeof(bfloat16) * A.size()};
    distributed::ReplicatedBufferConfig cfgB{.size = sizeof(bfloat16) * B.size()};
    distributed::ReplicatedBufferConfig cfgC{.size = out_tile * (Mt * Nt)};
    auto A_buf = distributed::MeshBuffer::create(cfgA, in_dram, mesh_device.get());
    auto B_buf = distributed::MeshBuffer::create(cfgB, in_dram, mesh_device.get());
    auto C_buf = distributed::MeshBuffer::create(cfgC, out_dram, mesh_device.get());

    CircularBufferConfig cb0 = CircularBufferConfig(2*in_tile, {{CBIndex::c_0, tt::DataFormat::Float16_b}}).set_page_size(CBIndex::c_0, in_tile);
    CreateCircularBuffer(program, core, cb0);
    CircularBufferConfig cb1 = CircularBufferConfig(2*in_tile, {{CBIndex::c_1, tt::DataFormat::Float16_b}}).set_page_size(CBIndex::c_1, in_tile);
    CreateCircularBuffer(program, core, cb1);
    CircularBufferConfig cb16 = CircularBufferConfig(2*out_tile, {{CBIndex::c_16, tt::DataFormat::Float32}}).set_page_size(CBIndex::c_16, out_tile);
    CreateCircularBuffer(program, core, cb16);

    std::vector<uint32_t> rct; TensorAccessorArgs(*A_buf).append_to(rct); TensorAccessorArgs(*B_buf).append_to(rct);
    auto reader = CreateKernel(program, "kernels_mm/reader_mm.cpp", core,
        DataMovementConfig{.processor = DataMovementProcessor::RISCV_1, .noc = NOC::RISCV_1_default, .compile_args = rct});
    std::vector<uint32_t> wct; TensorAccessorArgs(*C_buf).append_to(wct);
    auto writer = CreateKernel(program, "kernels_mm/writer_mm.cpp", core,
        DataMovementConfig{.processor = DataMovementProcessor::RISCV_0, .noc = NOC::RISCV_0_default, .compile_args = wct});
    std::vector<uint32_t> cct = {Mt, Kt, Nt};
    CreateKernel(program, "kernels_mm/mm.cpp", core,
        ComputeConfig{.math_fidelity = MathFidelity::HiFi4, .compile_args = cct});

    SetRuntimeArgs(program, reader, core, {A_buf->address(), B_buf->address(), Mt, Kt, Nt});
    SetRuntimeArgs(program, writer, core, {C_buf->address(), Mt, Nt});

    distributed::EnqueueWriteMeshBuffer(cq, A_buf, A, false);
    distributed::EnqueueWriteMeshBuffer(cq, B_buf, B, false);
    workload.add_program(range, std::move(program));

    // Warmup
    for (uint32_t r = 0; r < warmup; ++r) distributed::EnqueueMeshWorkload(cq, workload, false);
    cq.finish();

    // Latence device seule
    auto d0 = clk::now();
    for (uint32_t r = 0; r < iters; ++r) distributed::EnqueueMeshWorkload(cq, workload, false);
    cq.finish();
    auto d1 = clk::now();
    double dev_us = std::chrono::duration<double, std::micro>(d1 - d0).count() / iters;

    // Latence end-to-end (write X + compute + read C)
    std::vector<float> C(Mp * Np, 0.0f);
    auto e0 = clk::now();
    for (uint32_t r = 0; r < iters; ++r) {
        distributed::EnqueueWriteMeshBuffer(cq, B_buf, B, false);
        distributed::EnqueueMeshWorkload(cq, workload, false);
        distributed::EnqueueReadMeshBuffer(cq, C, C_buf, true);
    }
    auto e1 = clk::now();
    double e2e_us = std::chrono::duration<double, std::micro>(e1 - e0).count() / iters;

    // Correction (PCC vs golden) sur la derniere lecture
    std::vector<float> C_rm = untilize_nfaces(C, Mp, Np);
    double sx=0,sy=0,sxx=0,syy=0,sxy=0; uint32_t cnt=0;
    for (uint32_t m=0;m<M;++m) for (uint32_t nn=0;nn<N;++nn){
        double a=std::lround(C_rm[m*Np+nn]), b=gold[m*N+nn];
        sx+=a; sy+=b; sxx+=a*a; syy+=b*b; sxy+=a*b; cnt++;
    }
    double dn=cnt, den=std::sqrt((dn*sxx-sx*sx)*(dn*syy-sy*sy));
    double pcc = (den>0)?(dn*sxy-sx*sy)/den:1.0;

    mesh_device->close();

    double macs = (double)M * K * N;                 // MAC utiles (batch complet)
    double gops = (2.0*macs)/(dev_us*1e-6)/1e9;
    double per_vec_us = dev_us / N;                  // latence amortie par vecteur BitLinear
    double dma_us = e2e_us - dev_us;

    std::cout << "\n=== BENCH unite matricielle (batch N=" << N << ") ===" << std::endl;
    std::cout << "  M=" << M << " K=" << K << " N=" << N << "  MAC/appel=" << (uint64_t)macs
              << "  PCC=" << pcc << std::endl;
    std::cout << "  Latence device : " << dev_us << " us/appel  (" << per_vec_us << " us/vecteur)" << std::endl;
    std::cout << "  Latence e2e    : " << e2e_us << " us/appel   DMA " << dma_us << " us" << std::endl;
    std::cout << "  Debit device   : " << gops << " GOP/s" << std::endl;

    const char* csv = "benchmarks/bitlinear_mm_results.csv";
    bool ex = std::ifstream(csv).good();
    std::ofstream out(csv, std::ios::app);
    if (out) {
        if (!ex) out << "M,K,N,mac_per_call,iters,device_lat_us,per_vec_us,e2e_lat_us,dma_us,device_gops,pcc\n";
        out << M << "," << K << "," << N << "," << (uint64_t)macs << "," << iters << ","
            << dev_us << "," << per_vec_us << "," << e2e_us << "," << dma_us << "," << gops << "," << pcc << "\n";
        std::cout << "  -> ligne ajoutee a " << csv << std::endl;
    }
    return 0;
}
