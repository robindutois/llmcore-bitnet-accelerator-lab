# Block Design Notes — Week 5 / Week 6
## BitLinear-FPGA Alpha

**Status:** IP exported (Week 5). Block design construction scheduled for Week 6.

## IP to import
`fpga_erven/hls/bitlinear/bitlinear_hls_project/solution1/impl/ip/llmcore_hls_bitlinear_hls_1_0.zip`

## Planned block design
```
Zynq UltraScale+ PS (ARM Cortex-A53)
  ├── GP0 (AXI Master) → AXI SmartConnect → s_axi_CTRL    (ap_start/done, M, K)
  │                                        → s_axi_control (x, W_packed, y addresses)
  └── HP0 (AXI Slave)  ← m_axi_MEM_IN    (activation read)
      HP1 (AXI Slave)  ← m_axi_MEM_W     (packed weights read)
      HP2 (AXI Slave)  ← m_axi_MEM_OUT   (output write)
```

## AXI address map (to be confirmed in Vivado address editor)
| Port | Base address | Size |
|------|-------------|------|
| s_axi_CTRL | TBD | 64 KB |
| s_axi_control | TBD | 64 KB |

To be completed during Week 6 board integration.
