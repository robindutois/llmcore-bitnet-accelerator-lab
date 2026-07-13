#include <stdint.h>
#include "api/dataflow/dataflow_api.h"

void kernel_main() {
    uint32_t src0_addr  = get_arg_val<uint32_t>(0); 
    uint32_t src1_addr  = get_arg_val<uint32_t>(1); 
    uint32_t num_tiles  = get_arg_val<uint32_t>(2); 
    
    constexpr uint32_t cb_id_in0 = tt::CB::c_in0;
    constexpr uint32_t cb_id_in1 = tt::CB::c_in1;

    uint32_t tile_size_in0 = get_tile_size(cb_id_in0); // 1024 octets (Int8)
    uint32_t tile_size_in1 = get_tile_size(cb_id_in1); // 256 octets (2-bit packed via UInt8)

    // On force le format Float32 pour contourner le packetizer et faire une copie binaire brute
    const InterleavedAddrGenFast<true> s0 = {
        .bank_base_address = src0_addr,
        .page_size = tile_size_in0, // Reste 1024
        .data_format = DataFormat::Float32 
    };
    
    const InterleavedAddrGenFast<true> s1 = {
        .bank_base_address = src1_addr,
        .page_size = tile_size_in1, // Reste 256
        .data_format = DataFormat::Float32 
    };

    for (uint32_t i = 0; i < num_tiles; i++) {
        cb_reserve_back(cb_id_in0, 1);
        cb_reserve_back(cb_id_in1, 1);

        uint32_t l1_write_addr_in0 = get_write_ptr(cb_id_in0);
        uint32_t l1_write_addr_in1 = get_write_ptr(cb_id_in1);

        // Remplacer noc_async_read_tile par noc_async_read_page
        noc_async_read_page(i, s0, l1_write_addr_in0);
        noc_async_read_page(i, s1, l1_write_addr_in1);

        noc_async_read_barrier();

        cb_push_back(cb_id_in0, 1);
        cb_push_back(cb_id_in1, 1);
    }
}