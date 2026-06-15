#include <stdint.h>
#include "dataflow_api.h"

void kernel_main() {
    // 1. Recuperation de l'adresse de destination en DRAM
    uint32_t dst_addr   = get_arg_val<uint32_t>(0);
    uint32_t num_tiles  = get_arg_val<uint32_t>(1);

    constexpr uint32_t cb_output = tt::CB::c_out0;
    
    // Taille d'une tuile de sortie (int32_t) : 32x32 * 4 octets = 4096 octets
    uint32_t tile_size_output = 4096;

    for (uint32_t i = 0; i < num_tiles; i++) {
        // On attend que le Compute Kernel nous livre une tuile terminee
        cb_wait_front(cb_output, 1);

        uint32_t l1_read_addr = get_read_ptr(cb_output);
        uint64_t dst_noc_addr = get_noc_addr(i, dst_addr);

        // Transfert asynchrone : SRAM -> DRAM
        noc_async_write(l1_read_addr, dst_noc_addr, tile_size_output);
        noc_async_write_barrier();

        // On libere la SRAM pour que le Compute Kernel puisse ecrire le prochain resultat
        cb_pop_front(cb_output, 1);
    }
}