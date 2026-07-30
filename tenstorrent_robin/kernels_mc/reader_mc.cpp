// SPDX-License-Identifier: Apache-2.0
// Reader multi-core officiel tt-metal (reader_mm_output_tiles_partitioned.cpp), inchangé.
// Chaque coeur lit les tuiles A/B nécessaires pour SES tuiles de sortie assignées.
#include <stdint.h>
#include <cstdint>
#include "api/dataflow/dataflow_api.h"

void kernel_main() {
    uint32_t src0_addr = get_arg_val<uint32_t>(0);
    uint32_t src1_addr = get_arg_val<uint32_t>(1);
    uint32_t Mt = get_arg_val<uint32_t>(2);
    uint32_t Kt = get_arg_val<uint32_t>(3);
    uint32_t Nt = get_arg_val<uint32_t>(4);
    uint32_t output_tile_start_id = get_arg_val<uint32_t>(5);
    uint32_t num_output_tiles = get_arg_val<uint32_t>(6);

    constexpr uint32_t cb_id_in0 = tt::CBIndex::c_0;
    constexpr uint32_t cb_id_in1 = tt::CBIndex::c_1;

    constexpr auto a_args = TensorAccessorArgs<0>();
    const auto a = TensorAccessor(a_args, src0_addr);
    constexpr auto b_args = TensorAccessorArgs<a_args.next_compile_time_args_offset()>();
    const auto b = TensorAccessor(b_args, src1_addr);

    for (uint32_t output_tile = 0; output_tile < num_output_tiles; output_tile++) {
        uint32_t current_tile_id = output_tile_start_id + output_tile;
        uint32_t out_row = current_tile_id / Nt;
        uint32_t out_col = current_tile_id % Nt;

        for (uint32_t k = 0; k < Kt; k++) {
            uint32_t tile_A = out_row * Kt + k;
            {
                cb_reserve_back(cb_id_in0, 1);
                uint32_t l1_write_addr_in0 = get_write_ptr(cb_id_in0);
                noc_async_read_page(tile_A, a, l1_write_addr_in0);
                noc_async_read_barrier();
                cb_push_back(cb_id_in0, 1);
            }
            uint32_t tile_B = k * Nt + out_col;
            {
                cb_reserve_back(cb_id_in1, 1);
                uint32_t l1_write_addr_in1 = get_write_ptr(cb_id_in1);
                noc_async_read_page(tile_B, b, l1_write_addr_in1);
                noc_async_read_barrier();
                cb_push_back(cb_id_in1, 1);
            }
        }
    }
}
