#include <stdint.h>
#include "dataflow_api.h"

void kernel_main() {
    uint32_t dst_addr  = get_arg_val<uint32_t>(0); // Adresse de destination en DRAM
    uint32_t num_tiles = get_arg_val<uint32_t>(1);

    constexpr uint32_t cb_id_out0 = 16; // Identifiant standard du Circular Buffer de sortie
    uint32_t l1_read_addr = get_read_ptr(cb_id_out0);

    const InterleavedAddrGenFast<true> s = {
        .bank_base_address = dst_addr,
        .page_size = 2048,
        .data_format = DataFormat::Float16_b
    };

    // On boucle pour envoyer les résultats calculés vers la DRAM
    for (uint32_t i = 0; i < num_tiles; i++) {
        cb_wait_front(cb_id_out0, 1);
        noc_async_write_tile(i, s, l1_read_addr);
        noc_async_write_barrier();
        cb_pop_front(cb_id_out0, 1);
        l1_read_addr += 2048;
    }
}
