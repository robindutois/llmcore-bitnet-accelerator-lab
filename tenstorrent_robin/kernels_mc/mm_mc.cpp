// SPDX-License-Identifier: Apache-2.0
// Compute multi-core officiel tt-metal (matmul_multi_core/kernels/compute/mm.cpp), inchangé.
// Chaque coeur produit num_output_tiles tuiles de sortie via matmul_tiles sur l'unité matricielle.
#include <cstdint>
#include "api/compute/tile_move_copy.h"
#include "api/compute/matmul.h"

using std::uint32_t;

void kernel_main() {
    uint32_t num_output_tiles = get_arg_val<uint32_t>(0);
    uint32_t Kt = get_arg_val<uint32_t>(1);

    constexpr tt::CBIndex cb_in0 = tt::CBIndex::c_0;
    constexpr tt::CBIndex cb_in1 = tt::CBIndex::c_1;
    constexpr tt::CBIndex cb_out = tt::CBIndex::c_16;

    mm_init(cb_in0, cb_in1, cb_out);

    for (uint32_t i = 0; i < num_output_tiles; ++i) {
        tile_regs_acquire();
        for (uint32_t kt = 0; kt < Kt; kt++) {
            cb_wait_front(cb_in0, 1);
            cb_wait_front(cb_in1, 1);
            matmul_tiles(cb_in0, cb_in1, 0, 0, 0);
            cb_pop_front(cb_in0, 1);
            cb_pop_front(cb_in1, 1);
        }
        tile_regs_commit();
        tile_regs_wait();
        cb_reserve_back(cb_out, 1);
        pack_tile(0, cb_out);
        cb_push_back(cb_out, 1);
        tile_regs_release();
    }
}
