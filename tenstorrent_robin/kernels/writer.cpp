#include <stdint.h>
#include "api/dataflow/dataflow_api.h"

// Table de décodage ternaire (2 bits -> {0, +1, -1, 0})
// Convention validée sur les vecteurs de référence :
//   idx = m*K + k ; packed[idx>>2] ; two_bit = (byte >> ((idx&3)*2)) & 3
static const int8_t decode_table[4] = {0, 1, -1, 0};

void kernel_main() {
    uint32_t dst_addr = get_arg_val<uint32_t>(0);
    uint32_t M        = get_arg_val<uint32_t>(1);
    uint32_t K        = get_arg_val<uint32_t>(2);
    uint32_t out_page = get_arg_val<uint32_t>(3); // taille de page DRAM (octets)

    constexpr uint32_t cb_id_in0  = tt::CB::c_in0;  // activations int8 (vecteur x, longueur K)
    constexpr uint32_t cb_id_in1  = tt::CB::c_in1;  // poids ternaires packés 2-bit (M*K)
    constexpr uint32_t cb_id_out0 = tt::CB::c_out0; // sortie int32 (longueur M) - scratch L1

    const InterleavedAddrGenFast<true> s = {
        .bank_base_address = dst_addr,
        .page_size = out_page,
        .data_format = DataFormat::Float32,
    };

    cb_wait_front(cb_id_in0, 1);
    cb_wait_front(cb_id_in1, 1);

    // get_read_ptr/get_write_ptr : adresses L1 correctes en octets (helpers dataflow).
    volatile tt_l1_ptr int8_t*  ptr_act = (volatile tt_l1_ptr int8_t*)get_read_ptr(cb_id_in0);
    volatile tt_l1_ptr uint8_t* ptr_w   = (volatile tt_l1_ptr uint8_t*)get_read_ptr(cb_id_in1);
    volatile tt_l1_ptr int32_t* ptr_out = (volatile tt_l1_ptr int32_t*)get_write_ptr(cb_id_out0);

    // Zéro sur toute la page (padding d'alignement propre)
    for (uint32_t i = 0; i < out_page / 4; ++i) ptr_out[i] = 0;

    // BitLinear matvec : out[m] = sum_k W[m][k] * x[k]  (row-major)
    for (uint32_t m = 0; m < M; ++m) {
        int32_t accumulator = 0;
        for (uint32_t k = 0; k < K; ++k) {
            int8_t x_val = ptr_act[k];
            uint32_t idx = m * K + k;
            uint8_t packed_byte = ptr_w[idx >> 2];
            uint8_t two_bit_val = (packed_byte >> ((idx & 3) * 2)) & 0x03;
            int8_t w_val = decode_table[two_bit_val];
            if (w_val == 1)       accumulator += static_cast<int32_t>(x_val);
            else if (w_val == -1) accumulator -= static_cast<int32_t>(x_val);
        }
        ptr_out[m] = accumulator;
    }

    // Rapatriement du vecteur résultat vers la DRAM
    noc_async_write_page(0, s, get_write_ptr(cb_id_out0));
    noc_async_write_barrier();

    cb_pop_front(cb_id_in0, 1);
    cb_pop_front(cb_id_in1, 1);
}
