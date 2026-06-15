#include <stdint.h>
#include "dataflow_api.h"

void kernel_main() {
    // 1. Recuperation des arguments envoyes par le Host Code (Ryzen)
    uint32_t src_activations_addr  = get_arg_val<uint32_t>(0);
    uint32_t src_weights_addr      = get_arg_val<uint32_t>(1);
    uint32_t num_tiles             = get_arg_val<uint32_t>(2);

    // 2. Identification des tampons de destination dans la SRAM
    constexpr uint32_t cb_activations = tt::CB::c_in0;
    constexpr uint32_t cb_weights     = tt::CB::c_in1;

    // 3. Taille d'une tuile standard en octets
    // Pour des int8, une tuile 32x32 = 1024 octets.
    // Pour tes poids compresses en 2-bits, c'est 1024 / 4 = 256 octets.
    uint32_t tile_size_activations = 1024; 
    uint32_t tile_size_weights = 256;

    // 4. Boucle de prechargement (Fetch)
    for (uint32_t i = 0; i < num_tiles; i++) {
        // Reservation de l'espace dans la memoire rapide SRAM
        cb_reserve_back(cb_activations, 1);
        cb_reserve_back(cb_weights, 1);

        // Recuperation des adresses d'ecriture SRAM
        uint32_t l1_write_addr_act = get_write_ptr(cb_activations);
        uint32_t l1_write_addr_weight = get_write_ptr(cb_weights);

        // Generation des coordonnees de lecture sur le NOC (Network On Chip)
        uint64_t act_noc_addr = get_noc_addr(i, src_activations_addr);
        uint64_t weight_noc_addr = get_noc_addr(i, src_weights_addr);

        // Lancement des transferts asynchrones (DMA) : DRAM -> SRAM
        noc_async_read(act_noc_addr, l1_write_addr_act, tile_size_activations);
        noc_async_read(weight_noc_addr, l1_write_addr_weight, tile_size_weights);

        // On s'assure que les donnees sont physiquement arrivees
        noc_async_read_barrier();

        // On signale au Compute Kernel que cette tuile est prete a etre calculee
        cb_push_back(cb_activations, 1);
        cb_push_back(cb_weights, 1);
    }
}   