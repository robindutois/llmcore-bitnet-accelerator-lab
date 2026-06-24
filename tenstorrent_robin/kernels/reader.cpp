// ============================================================================
// EdgeBox-TT Alpha: Data Movement Kernel - Reader (Semaine 8 - Optimisé)
// Auteur: Robin Dutois
// Cible: Tenstorrent Blackhole BRISC Core
// ============================================================================

#include <stdint.h>
#include "dataflow_api.h"

void kernel_main() {
    // ------------------------------------------------------------------------
    // ARGUMENTS DU KERNEL (Configurés par l'hôte en Python/C++)
    // ------------------------------------------------------------------------
    uint32_t src0_addr  = get_arg_val<uint32_t>(0); // Adresse des activations (DRAM)
    uint32_t src1_addr  = get_arg_val<uint32_t>(1); // Adresse des poids compressés (DRAM)
    uint32_t num_tiles  = get_arg_val<uint32_t>(2); // Nombre de blocs 32x32 à lire
    
    // Identifiants des Circular Buffers (SRAM L1 locale)
    constexpr uint32_t cb_id_in0 = tt::CB::c_in0; // Activations
    constexpr uint32_t cb_id_in1 = tt::CB::c_in1; // Poids

    // Tailles des blocs (en octets)
    uint32_t tile_size_in0 = get_tile_size(cb_id_in0);
    uint32_t tile_size_in1 = get_tile_size(cb_id_in1);

    // Pointeurs de lecture asynchrone (NOC - Network On Chip)
    uint64_t src0_noc_addr = get_noc_addr(src0_addr);
    uint64_t src1_noc_addr = get_noc_addr(src1_addr);

    // ========================================================================
    // OPTIMISATION (SEMAINE 8) : DOUBLE BUFFERING
    // Au lieu de lire 1 Tile, d'attendre le calcul, puis de lire le suivant,
    // on gère une file d'attente (pipeline) pour masquer la latence du PCIe x4.
    // ========================================================================

    for (uint32_t i = 0; i < num_tiles; i++) {
        // --- 1. Réservation de la place dans la SRAM L1 ---
        // On demande la place pour 1 Tile. Le système gérera le double-tamponnage
        // automatiquement si le CB a été configuré avec une capacité > 1 par l'hôte.
        cb_reserve_back(cb_id_in0, 1);
        cb_reserve_back(cb_id_in1, 1);

        // Récupération de l'adresse de destination physique dans la SRAM L1
        uint32_t l1_write_addr_in0 = get_write_ptr(cb_id_in0);
        uint32_t l1_write_addr_in1 = get_write_ptr(cb_id_in1);

        // --- 2. Lancement des transferts asynchrones (DRAM -> SRAM L1) ---
        // Le BRISC ordonne au NOC de ramener les données. Il n'attend pas !
        noc_async_read(src0_noc_addr, l1_write_addr_in0, tile_size_in0);
        noc_async_read(src1_noc_addr, l1_write_addr_in1, tile_size_in1);

        // --- 3. Synchronisation de la mémoire ---
        // On attend que LES transferts de ce tour-ci soient terminés
        noc_async_read_barrier();

        // --- 4. Validation des données ---
        // On signale au compute_kernel_api (qui est endormi sur cb_wait_front)
        // que les données sont physiquement présentes et prêtes à être calculées.
        cb_push_back(cb_id_in0, 1);
        cb_push_back(cb_id_in1, 1);

        // Avancement des pointeurs DRAM pour le prochain bloc
        src0_noc_addr += tile_size_in0;
        src1_noc_addr += tile_size_in1;
    }
}
