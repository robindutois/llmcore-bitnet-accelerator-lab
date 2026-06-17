#include "tt-metalium/host_api.hpp"
#include "tt-metalium/device.hpp"

using namespace tt;
using namespace tt::tt_metal;

int main(int argc, char **argv) {
    int device_id = 0;
    IDevice *device = CreateDevice(device_id);
    Program program = CreateProgram();

    // On cible toujours le premier petit coeur de la puce pour ce test
    CoreCoord compute_core = {0, 0}; 
    uint32_t num_tiles = 1; // On calcule une seule tuile (32x32) pour commencer

    // =========================================================================
    // 1. ALLOCATION EN DRAM (Memoire globale de la carte)
    // =========================================================================
    uint32_t act_tile_size = 1024;    // Tuile d'activation (int8) = 32*32 * 1 octet
    uint32_t weight_tile_size = 256;  // Tuile de poids compresses (2-bits) = 1024 / 4
    uint32_t out_tile_size = 4096;    // Tuile de resultat (int32) = 32*32 * 4 octets

    InterleavedBufferConfig act_config{device, act_tile_size * num_tiles, act_tile_size, BufferType::DRAM};
    std::shared_ptr<Buffer> act_dram_buffer = CreateBuffer(act_config);
    InterleavedBufferConfig weight_config{device, weight_tile_size * num_tiles, weight_tile_size, BufferType::DRAM};
    std::shared_ptr<Buffer> weight_dram_buffer = CreateBuffer(weight_config);
    InterleavedBufferConfig out_config{device, out_tile_size * num_tiles, out_tile_size, BufferType::DRAM};
    std::shared_ptr<Buffer> out_dram_buffer = CreateBuffer(out_config);

    // =========================================================================
    // 2. CONFIGURATION DES CIRCULAR BUFFERS (Memoire SRAM du coeur)
    // C'est ici qu'on fait le lien avec les tt::CB::c_in0 de tes fichiers RISC-V
    // =========================================================================
    CircularBufferConfig cb_act_config = CircularBufferConfig(act_tile_size, {{tt::CB::c_in0, tt::DataFormat::Int8}}).set_page_size(tt::CB::c_in0, act_tile_size);
    CBHandle cb_act = CreateCircularBuffer(program, compute_core, cb_act_config);

    CircularBufferConfig cb_weight_config = CircularBufferConfig(weight_tile_size, {{tt::CB::c_in1, tt::DataFormat::UInt8}}).set_page_size(tt::CB::c_in1, weight_tile_size);
    CBHandle cb_weight = CreateCircularBuffer(program, compute_core, cb_weight_config);

    CircularBufferConfig cb_out_config = CircularBufferConfig(out_tile_size, {{tt::CB::c_out0, tt::DataFormat::Int32}}).set_page_size(tt::CB::c_out0, out_tile_size);
    CBHandle cb_out = CreateCircularBuffer(program, compute_core, cb_out_config);

    // =========================================================================
    // 3. COMPILATION ET INJECTION DES NOYAUX RISC-V
    // =========================================================================
    // Le Reader va s'executer sur le micro-controleur reseau 0 (NOC_RISCV_0)
    KernelHandle reader_kernel_id = CreateKernel(
        program,
        "kernels/reader.cpp",
        compute_core,
        DataMovementConfig{.processor = DataMovementProcessor::RISCV_0, .noc = NOC::RISCV_0_default}
    );

    // Le Writer va s'executer sur le micro-controleur reseau 1 (NOC_RISCV_1)
    KernelHandle writer_kernel_id = CreateKernel(
        program,
        "kernels/writer.cpp",
        compute_core,
        DataMovementConfig{.processor = DataMovementProcessor::RISCV_1, .noc = NOC::RISCV_1_default}
    );

    // Le moteur mathematique s'execute sur l'unite de calcul principale (Compute)
    KernelHandle compute_kernel_id = CreateKernel(
        program,
        "kernels/compute_bitlinear.cpp",
        compute_core,
        ComputeConfig{}
    );

    // =========================================================================
    // 4. PASSAGE DES ARGUMENTS D'EXECUTION (Runtime Args)
    // On donne aux noyaux les adresses physiques en DRAM
    // =========================================================================
    SetRuntimeArgs(
        program,
        reader_kernel_id,
        compute_core,
        {act_dram_buffer->address(), weight_dram_buffer->address(), num_tiles}
    );

    SetRuntimeArgs(
        program,
        writer_kernel_id,
        compute_core,
        {out_dram_buffer->address(), num_tiles}
    );

    // =========================================================================
    // 5. EXECUTION ET SYNCHRONISATION
    // =========================================================================
    // (Dans la vraie vie, on ferait un EnqueueWriteBuffer ici pour copier
    // tes donnees Python depuis le CPU vers act_dram_buffer et weight_dram_buffer)

    // Lancement du programme sur la carte Tenstorrent
    
    // On attend que la carte ait fini de calculer

    // (Dans la vraie vie, on ferait un EnqueueReadBuffer ici pour recuperer le resultat)

    CloseDevice(device);
    return 0;
}