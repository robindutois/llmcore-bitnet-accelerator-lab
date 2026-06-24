// ============================================================================
// EdgeBox-TT Alpha: BitNet Compute Kernel (Semaine 8 - Optimisé)
// Auteur: Robin Dutois
// Cible: Tenstorrent Blackhole RISC-V Cores (TRISC)
// ============================================================================

#include <stdint.h>
#include "compute_kernel_api/common.h"
#include "compute_kernel_api/cb_api.h"
#include "compute_kernel_api/math.h"

namespace NAMESPACE {

// Table de décodage ternaire fixée en SRAM L1 (Ultra-rapide)
// Mappe les valeurs 2-bits (00, 01, 10) vers des multiplicateurs entiers (0, 1, -1)
static const int8_t decode_table[4] = {0, 1, -1, 0};

void MAIN {
    // Les constantes du bloc matériel (Tile) - Standard TT-Metalium = 32x32
    constexpr uint32_t TILE_WIDTH = 32;
    constexpr uint32_t TILE_HEIGHT = 32;
    
    // Identifiants des Circular Buffers (CB)
    // cb_in0 : Activations (int8) provenant de reader.cpp
    // cb_in1 : Poids compressés (2-bits) provenant de reader.cpp
    // cb_out0: Résultats accumulés (int32) envoyés vers writer.cpp
    constexpr uint32_t cb_in0 = tt::CB::c_in0;
    constexpr uint32_t cb_in1 =  tt::CB::c_in1;
    constexpr uint32_t cb_out0 = tt::CB::c_out0;

    // Phase d'initialisation matérielle requise par l'architecture
    binary_op_init_common(cb_in0, cb_in1, cb_out0);

    // Boucle principale (à adapter selon le nombre de tiles à traiter par cœur)
    // Pour cet exemple, on traite un seul Tile (bloc de 32x32)
    uint32_t num_tiles = 1; 

    for(uint32_t t = 0; t < num_tiles; ++t) {
        // --- 1. SYNCHRONISATION (Wait) ---
        // Le cœur s'endort jusqu'à ce que reader.cpp ait déposé les données
        cb_wait_front(cb_in0, 1);
        cb_wait_front(cb_in1, 1);
        cb_reserve_back(cb_out0, 1); // Réserve une place pour le résultat

        // --- 2. RÉCUPÉRATION DES POINTEURS SRAM ---
        // On récupère les adresses physiques directes dans le cache L1
        int8_t* ptr_act = (int8_t*)get_read_ptr(cb_in0);
        uint8_t* ptr_w_packed = (uint8_t*)get_read_ptr(cb_in1);
        int32_t* ptr_out = (int32_t*)get_write_ptr(cb_out0);

        // --- 3. EXÉCUTION DE L'ALGORITHME BITNET OPTIMISÉ ---
        // Pour chaque ligne de sortie 'm'
        for (uint32_t m = 0; m < TILE_HEIGHT; ++m) {
            int32_t accumulator = 0;
            
            // Pour chaque élément de la ligne 'k'
            for (uint32_t k = 0; k < TILE_WIDTH; ++k) {
                // Index global aplati (1D)
                uint32_t global_idx = m * TILE_WIDTH + k;

                // ========================================================
                // OPTIMISATION CRITIQUE (SEMAINE 8) : BIT-SHIFTING
                // Remplacement de (global_idx / 4) par un décalage de bits
                // Remplacement de (global_idx % 4) par un masque binaire
                // ========================================================
                uint32_t byte_idx = global_idx >> 2; 
                uint32_t bit_pos  = global_idx & 3;  

                // Extraction de l'octet compressé contenant 4 poids
                uint8_t packed_byte = ptr_w_packed[byte_idx];

                // Extraction des 2 bits spécifiques pour ce poids 'k'
                // Décalage pour amener les bits voulus à droite, puis masque avec 0x03 (00000011 en binaire)
                uint8_t two_bit_val = (packed_byte >> (bit_pos * 2)) & 0x03;

                // Résolution de la vraie valeur du poids (-1, 0, ou +1) via la SRAM L1
                int8_t w_val = decode_table[two_bit_val];

                // Lecture de l'activation
                int8_t x_val = ptr_act[global_idx];

                // Opérateur Linéaire BitNet (Sans multiplication complexe)
                accumulator += (x_val * w_val); // Le compilar C++ transformera ceci en additions/soustractions directes
            }
            
            // Stockage du résultat final de la ligne 'm'
            ptr_out[m] = accumulator;
        }

        // --- 4. SYNCHRONISATION DE SORTIE (Pop & Push) ---
        // On libère la place dans les tampons d'entrée pour le prochain tour
        cb_pop_front(cb_in0, 1);
        cb_pop_front(cb_in1, 1);
        
        // On prévient writer.cpp que le résultat est prêt à être expédié
        cb_push_back(cb_out0, 1);
    }
}
}
