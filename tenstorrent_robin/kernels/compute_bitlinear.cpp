#include <cstdint>
#include "api/compute/compute_kernel_api.h"
#include "api/compute/cb_api.h"
#include "api/compute/eltwise_binary.h"
#include "api/compute/matmul.h"
#include "api/compute/tile_move_copy.h"

// Helper pour la mémoire physique Tenstorrent (Tile Layout = 4 faces 16x16)
inline uint32_t get_tile_idx(uint32_t m, uint32_t n) {
    uint32_t face_m = m >> 4; // m / 16
    uint32_t face_n = n >> 4; // n / 16
    uint32_t face_idx = (face_m << 1) + face_n; // Donne la face 0, 1, 2, ou 3
    uint32_t local_m = m & 15; // m % 16
    uint32_t local_n = n & 15; // n % 16
    return (face_idx << 8) + (local_m << 4) + local_n; // face_idx * 256 + local_m * 16 + local_n
}

// Table de décodage ternaire fixée en SRAM L1 
static const int8_t decode_table[4] = {0, 1, -1, 0};

void kernel_main() {
    // === TRISC 0 : UNPACK ===
    #if defined(UCK_CHLKC_UNPACK)
    constexpr uint32_t TILE_WIDTH = 32;
    constexpr uint32_t TILE_HEIGHT = 32;
    
    constexpr uint32_t cb_in0 = tt::CB::c_in0;
    constexpr uint32_t cb_in1 = tt::CB::c_in1;
    constexpr uint32_t cb_out0 = tt::CB::c_out0;

    uint32_t num_tiles = 1; 

    for(uint32_t t = 0; t < num_tiles; ++t) {
        cb_wait_front(cb_in0, 1);
        cb_wait_front(cb_in1, 1);
        cb_reserve_back(cb_out0, 1);

        // Retrait de la multiplication par 16 (les pointeurs sont déjà des adresses absolues)
        int8_t* ptr_act = (int8_t*)get_local_cb_interface(cb_in0).fifo_rd_ptr;
        uint8_t* ptr_w_packed = (uint8_t*)get_local_cb_interface(cb_in1).fifo_rd_ptr;
        int32_t* ptr_out = (int32_t*)get_local_cb_interface(cb_out0).fifo_wr_ptr;

        for (uint32_t m = 0; m < TILE_HEIGHT; ++m) {
            for (uint32_t n = 0; n < TILE_WIDTH; ++n) {
                int32_t accumulator = 0;
                
                for (uint32_t k = 0; k < TILE_WIDTH; ++k) {
                    
                    // CORRECTION 2 : Lecture au format physique (Faces)
                    uint32_t act_idx = get_tile_idx(m, k);
                    int8_t x_val = ptr_act[act_idx];

                    uint32_t w_idx = get_tile_idx(k, n);
                    uint32_t byte_idx = w_idx >> 2; 
                    uint32_t bit_pos  = w_idx & 3;  

                    uint8_t packed_byte = ptr_w_packed[byte_idx];
                    uint8_t two_bit_val = (packed_byte >> (bit_pos * 2)) & 0x03;

                    int8_t w_val = decode_table[two_bit_val];

                    // MAC BitLinear 
                    if (w_val == 1) {
                        accumulator += static_cast<int32_t>(x_val);
                    } else if (w_val == -1) {
                        accumulator -= static_cast<int32_t>(x_val);
                    }
                }
                
                // Écriture au format physique (Faces)
                uint32_t out_idx = get_tile_idx(m, n);
                ptr_out[out_idx] = accumulator;
            }
        }

        cb_pop_front(cb_in0, 1);
        cb_pop_front(cb_in1, 1);
        cb_push_back(cb_out0, 1);
    }

    // === TRISC 1 : MATH ===
    #elif defined(UCK_CHLKC_MATH)
    
    // === TRISC 2 : PACK ===
    #elif defined(UCK_CHLKC_PACK)
    
    #endif
}