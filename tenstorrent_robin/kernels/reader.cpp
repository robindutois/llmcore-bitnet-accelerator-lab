#include <stdint.h>
#include "api/dataflow/dataflow_api.h"

void kernel_main() {
    uint32_t src0_addr  = get_arg_val<uint32_t>(0); // Adresse DRAM des activations
    uint32_t src1_addr  = get_arg_val<uint32_t>(1); // Adresse DRAM des poids
    uint32_t num_tiles  = get_arg_val<uint32_t>(2); 
    
    constexpr uint32_t cb_id_in0 = tt::CB::c_in0;
    constexpr uint32_t cb_id_in1 = tt::CB::c_in1;

    uint32_t tile_size_in0 = get_tile_size(cb_id_in0);
    uint32_t tile_size_in1 = get_tile_size(cb_id_in1);

    uint64_t src0_noc_addr = get_noc_addr(src0_addr);
    uint64_t src1_noc_addr = get_noc_addr(src1_addr);

    for (uint32_t i = 0; i < num_tiles; i++) {
        // Réservation de l'espace dans le Circular Buffer en L1
        cb_reserve_back(cb_id_in0, 1);
        cb_reserve_back(cb_id_in1, 1);

        uint32_t l1_write_addr_in0 = get_write_ptr(cb_id_in0);
        uint32_t l1_write_addr_in1 = get_write_ptr(cb_id_in1);

        // Amorce des lectures asynchrones sans blocage immédiat
        noc_async_read(src0_noc_addr, l1_write_addr_in0, tile_size_in0);
        noc_async_read(src1_noc_addr, l1_write_addr_in1, tile_size_in1);

        // Synchro matérielle : On s'assure que les données de CETTE itération sont en cache L1
        noc_async_read_barrier();

        // Notification au cœur compute qu'un bloc est disponible
        cb_push_back(cb_id_in0, 1);
        cb_push_back(cb_id_in1, 1);

        // Incrémentation des pointeurs DRAM pour le cycle réseau suivant
        src0_noc_addr += tile_size_in0;
        src1_noc_addr += tile_size_in1;
    }
}
