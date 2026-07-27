// SPDX-License-Identifier: Apache-2.0
// Writer du matmul single-core officiel tt-metal (inchangé) — écrit les tuiles de
// sortie C (M×N) en DRAM. La TensorAccessor reprend la page_size du buffer C
// (fp32 = 4096 o dans notre cas), donc le kernel gère nativement la sortie fp32.
#include "api/dataflow/dataflow_api.h"

void kernel_main() {
    uint32_t dst_addr = get_arg_val<uint32_t>(0);
    uint32_t Mt = get_arg_val<uint32_t>(1);
    uint32_t Nt = get_arg_val<uint32_t>(2);

    constexpr uint32_t cb_id_out0 = 16;

    constexpr auto s_args = TensorAccessorArgs<0>();
    const auto s = TensorAccessor(s_args, dst_addr);

    for (uint32_t m = 0; m < Mt; ++m) {
        for (uint32_t n = 0; n < Nt; ++n) {
            cb_wait_front(cb_id_out0, 1);
            uint32_t l1_read_addr = get_read_ptr(cb_id_out0);
            noc_async_write_page(m * Nt + n, s, l1_read_addr);
            noc_async_write_barrier();
            cb_pop_front(cb_id_out0, 1);
        }
    }
}
