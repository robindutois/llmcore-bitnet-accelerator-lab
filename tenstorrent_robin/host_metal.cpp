#include <iostream>
#include <vector>
#include <numeric>
#include <memory>

#include "tt-metalium/host_api.hpp"
#include "tt-metalium/mesh_device.hpp"
#include "tt-metalium/mesh_command_queue.hpp"
#include "tt-metalium/mesh_buffer.hpp"
#include "tt-metalium/mesh_workload.hpp"

using namespace tt;
using namespace tt::tt_metal;

int main(int argc, char **argv) {
    int device_id = 0;
    
    auto mesh_device = distributed::MeshDevice::create_unit_mesh(device_id);
    distributed::MeshCommandQueue& cq = mesh_device->mesh_command_queue();
    
    Program program = CreateProgram();
    CoreCoord compute_core = {0, 0}; 
    uint32_t num_tiles = 1;

    uint32_t act_tile_size = 1024;    
    uint32_t weight_tile_size = 256;  
    uint32_t out_tile_size = 4096;    

    // ALLOCATION EN DRAM
    distributed::DeviceLocalBufferConfig act_local_config{.page_size = act_tile_size, .buffer_type = BufferType::DRAM};
    distributed::ReplicatedBufferConfig act_repl_config{.size = act_tile_size * num_tiles};
    auto act_dram_buffer = distributed::MeshBuffer::create(act_repl_config, act_local_config, mesh_device.get());

    distributed::DeviceLocalBufferConfig weight_local_config{.page_size = weight_tile_size, .buffer_type = BufferType::DRAM};
    distributed::ReplicatedBufferConfig weight_repl_config{.size = weight_tile_size * num_tiles};
    auto weight_dram_buffer = distributed::MeshBuffer::create(weight_repl_config, weight_local_config, mesh_device.get());

    distributed::DeviceLocalBufferConfig out_local_config{.page_size = out_tile_size, .buffer_type = BufferType::DRAM};
    distributed::ReplicatedBufferConfig out_repl_config{.size = out_tile_size * num_tiles};
    auto out_dram_buffer = distributed::MeshBuffer::create(out_repl_config, out_local_config, mesh_device.get());

    // CONFIGURATION DES CIRCULAR BUFFERS (L1) -> FORCAGE DU FORMAT FLOAT32 POUR MODE RAW DMA
    CircularBufferConfig cb_act_config = CircularBufferConfig(act_tile_size * 2, {{tt::CB::c_in0, tt::DataFormat::Float32}}).set_page_size(tt::CB::c_in0, act_tile_size);
    CBHandle cb_act = CreateCircularBuffer(program, compute_core, cb_act_config);

    CircularBufferConfig cb_weight_config = CircularBufferConfig(weight_tile_size * 2, {{tt::CB::c_in1, tt::DataFormat::Float32}}).set_page_size(tt::CB::c_in1, weight_tile_size);
    CBHandle cb_weight = CreateCircularBuffer(program, compute_core, cb_weight_config);

    CircularBufferConfig cb_out_config = CircularBufferConfig(out_tile_size, {{tt::CB::c_out0, tt::DataFormat::Float32}}).set_page_size(tt::CB::c_out0, out_tile_size);
    CBHandle cb_out = CreateCircularBuffer(program, compute_core, cb_out_config);

    // INJECTION DES NOYAUX
    KernelHandle reader_kernel_id = CreateKernel(
        program, "kernels/reader.cpp", compute_core,
        DataMovementConfig{.processor = DataMovementProcessor::RISCV_0, .noc = NOC::RISCV_0_default}
    );

    KernelHandle writer_kernel_id = CreateKernel(
        program, "kernels/writer.cpp", compute_core,
        DataMovementConfig{.processor = DataMovementProcessor::RISCV_1, .noc = NOC::RISCV_1_default}
    );

    KernelHandle compute_kernel_id = CreateKernel(
        program, "kernels/compute_bitlinear.cpp", compute_core, ComputeConfig{}
    );

    SetRuntimeArgs(program, reader_kernel_id, compute_core, {act_dram_buffer->address(), weight_dram_buffer->address(), num_tiles});
    SetRuntimeArgs(program, writer_kernel_id, compute_core, {out_dram_buffer->address(), num_tiles});

    // PRÉPARATION ET TRANSFERT
    std::vector<int8_t> host_activations(act_tile_size * num_tiles, 1); 
    std::vector<uint8_t> host_weights(weight_tile_size * num_tiles, 0x55); 

    std::cout << "[EdgeBox-TT] Écriture des données via MeshBuffer..." << std::endl;
    cq.enqueue_write_mesh_buffer(act_dram_buffer, host_activations.data(), false);
    cq.enqueue_write_mesh_buffer(weight_dram_buffer, host_weights.data(), false);

    // EXÉCUTION DU WORKLOAD MESH
    std::cout << "[EdgeBox-TT] Lancement de la topologie MeshWorkload..." << std::endl;
    distributed::MeshWorkload workload;
    distributed::MeshCoordinateRange device_range = distributed::MeshCoordinateRange(mesh_device->shape());
    workload.add_program(device_range, std::move(program));
    
    distributed::EnqueueMeshWorkload(cq, workload, false); 
    cq.finish();

    // LECTURE DES RÉSULTATS
    std::vector<int32_t> host_outputs(out_tile_size * num_tiles / sizeof(int32_t), 0);
    std::cout << "[EdgeBox-TT] Rapatriement des résultats de la DRAM..." << std::endl;
    
    cq.enqueue_read_mesh_buffer(host_outputs.data(), out_dram_buffer, true);

    std::cout << "\n=== DÉBOGAGE DES 16 PREMIERS RÉSULTATS ===" << std::endl;
    for (uint32_t m = 0; m < 16; ++m) {
        std::cout << "Index " << m << " : " << host_outputs[m] << std::endl;
    }

    std::cout << "\n=== RÉSULTATS DU TEST MATÉRIEL ===" << std::endl;
    bool success = true;
    for (uint32_t m = 0; m < 32; ++m) {
        if (host_outputs[m] != 32) success = false;
    }

    if (success) {
        std::cout << "\n[Test PASS] Calcul BitLinear matériel validé !" << std::endl;
    } else {
        std::cout << "\n[Test FAIL] Écart détecté." << std::endl;
    }

    mesh_device->close();
    return 0;
}