#include <stdint.h>
#include "dataflow_api.h"

void kernel_main() {
    uint32_t src_addr  = get_arg_val<uint32_t>(0); // Adresse de la DRAM
    uint32_t num_tiles = get_arg_val<uint32_t>(1); // Nombre de tuiles à lire

    constexpr uint32_t cb_id_in0 = 0; // Identifiant du Circular Buffer d'entrée
    uint32_t l1_write_addr = get_write_ptr(cb_id_in0);

    const InterleavedAddrGenFast<true> s = {
        .bank_base_address = src_addr,
        .page_size = 2048, // Taille standard d'une tuile
        .data_format = DataFormat::Float16_b
    };

    // On boucle pour lire chaque tuile depuis la DRAM vers le coeur
    for (uint32_t i = 0; i < num_tiles; i++) {
        cb_reserve_back(cb_id_in0, 1);
        noc_async_read_tile(i, s, l1_write_addr);
        noc_async_read_barrier();
        cb_push_back(cb_id_in0, 1);
        l1_write_addr += 2048;
    }
}
