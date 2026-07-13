#include <stdint.h>
#include "api/dataflow/dataflow_api.h"

void kernel_main() {
    uint32_t dst_addr  = get_arg_val<uint32_t>(0); 
    uint32_t num_tiles = get_arg_val<uint32_t>(1);

    constexpr uint32_t cb_id_out0 = 16; 

    const InterleavedAddrGenFast<true> s = {
        .bank_base_address = dst_addr,
        .page_size = 4096, 
        .data_format = DataFormat::Float32, 
    };

    for (uint32_t i = 0; i < num_tiles; i++) {
        // 1. On attend que le cœur mathématique ait terminé sa tuile
        cb_wait_front(cb_id_out0, 1);
        
        // 2. SEULEMENT MAINTENANT on récupère l'adresse L1 valide
        uint32_t l1_read_addr = get_read_ptr(cb_id_out0);
        
        // 3. On envoie les données vers la DRAM
        noc_async_write_page(i, s, l1_read_addr);
        noc_async_write_barrier();
        
        // 4. On libère la place dans le buffer
        cb_pop_front(cb_id_out0, 1);
        
        // Note : Plus besoin d'incrémenter manuellement, get_read_ptr gèrera la page suivante.
    }
}