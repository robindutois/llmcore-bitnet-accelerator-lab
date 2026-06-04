# EdgeBox-TT Alpha — Tenstorrent Track

**Tenstorrent Blackhole-based BitNet-oriented LLM inference SKU prototype**
**Student:** Robin DUTOIS — Seoul National University / LLM Core AI

---

> **Note:** This track is not included in the current archive.
> The `tenstorrent_robin/` directory is reserved for the Tenstorrent implementation.
> See the project directive document for the full scope and 10-week plan.

---

## Planned deliverables

| Layer | Target |
|-------|--------|
| setup/ | Tenstorrent driver + TT-Metalium installation docs |
| inference_server/ | LLM inference server (prompt → generated text) |
| api_demo/ | REST API wrapper (`POST /generate`) |
| benchmarks/ | Latency / tokens-per-second CSV |
| tt_metal_kernels/ | vector_add, matvec_int8, bitlinear_ternary |

## Investor message

EdgeBox-TT Alpha will prove that LLM Core AI can build a near-term
non-NVIDIA LLM inference appliance on Tenstorrent Blackhole hardware.
