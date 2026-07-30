// SPDX-License-Identifier: Apache-2.0
// Writer multi-core officiel tt-metal (writer_unary_interleaved_start_id.cpp), inchangé.
// Chaque coeur écrit ses num_tiles tuiles de sortie à partir de start_id.
// La TensorAccessor reprend la page_size du buffer C (fp32 = 4096 o ici) → sortie fp32 gérée.
#include "api/dataflow/dataflow_api.h"

void kernel_main() {
    uint32_t dst_addr = get_arg_val<uint32_t>(0);
    uint32_t num_tiles = get_arg_val<uint32_t>(1);
    uint32_t start_id = get_arg_val<uint32_t>(2);

    constexpr uint32_t cb_id_out = tt::CBIndex::c_16;
    constexpr uint32_t onetile = 1;

    constexpr auto c_args = TensorAccessorArgs<0>();
    const auto c = TensorAccessor(c_args, dst_addr);

    uint32_t end_id = start_id + num_tiles;
    for (uint32_t i = start_id; i < end_id; ++i) {
        cb_wait_front(cb_id_out, onetile);
        uint32_t l1_read_addr = get_read_ptr(cb_id_out);
        noc_async_write_page(i, c, l1_read_addr);
        noc_async_write_barrier();
        cb_pop_front(cb_id_out, onetile);
    }
}
