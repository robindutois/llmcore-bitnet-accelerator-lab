#include <cstdint>
#include "compute_kernel_api/common.h"
#include "compute_kernel_api/cb_api.h"

// La meme table de decodage, mais cette fois stockee dans la SRAM rapide de la puce
static const int8_t decode_table[4] = {0, 1, -1, 0};

namespace NAMESPACE {
void MAIN {
    // 1. Definition des canaux de communication (Circular Buffers)
    // Ces identifiants doivent correspondre a ce qui est configure dans le host_metal.cpp
    constexpr uint32_t cb_activations = tt::CB::c_in0;
    constexpr uint32_t cb_weights_packed = tt::CB::c_in1;
    constexpr uint32_t cb_output = tt::CB::c_out0;

    // 2. Synchronisation : On attend que le "Data Movement Kernel" 
    // ait fini de rapatrier une "Tuile" (Tile de donnees) depuis la DRAM vers la SRAM
    cb_wait_front(cb_activations, 1);
    cb_wait_front(cb_weights_packed, 1);

    // 3. On reserve une case dans la SRAM de sortie pour ecrire notre resultat
    cb_reserve_back(cb_output, 1);

    // 4. Recuperation des pointeurs physiques directs vers la memoire SRAM du coeur
    int8_t* x = (int8_t*) get_read_ptr(cb_activations);
    uint8_t* W_packed = (uint8_t*) get_read_ptr(cb_weights_packed);
    int32_t* y = (int32_t*) get_write_ptr(cb_output);

    // Sur un NPU, on ne traite jamais 4096 valeurs d'un coup.
    // On decoupe toujours le travail par "Tuiles" (Tiles), classiquement de 32x32.
    int M = 32;
    int K = 32;

    // =========================================================================
    // LE MOTEUR MATHEMATIQUE (Directement porte depuis ton CPU)
    // =========================================================================
    for (int i = 0; i < M; ++i) {
        int32_t accumulator = 0;
        
        for (int j = 0; j < K; ++j) {
            int global_idx = i * K + j;
            int byte_idx = global_idx / 4;
            int bit_pos = global_idx % 4;
            
            // Extraction instantanee via la Lookup Table
            uint8_t packed_byte = W_packed[byte_idx];
            uint8_t bits = (packed_byte >> (bit_pos * 2)) & 0x03;
            int8_t weight = decode_table[bits];
            
            accumulator += static_cast<int32_t>(weight) * static_cast<int32_t>(x[j]);
        }
        y[i] = accumulator;
    }

    // 5. Cloture : Le calcul de cette tuile est termine.
    // On signale au materiel que le resultat est pret a etre envoye.
    cb_push_back(cb_output, 1);
    
    // 6. On libere la memoire SRAM des entrees pour faire de la place 
    // a la tuile suivante.
    cb_pop_front(cb_activations, 1);
    cb_pop_front(cb_weights_packed, 1);
}
}