# Environment Check — Tenstorrent Blackhole

Snapshot of the working environment used to bring up and validate the BitNet BitLinear
operator on the Tenstorrent Blackhole card. Values marked `<to fill>` are host-specific —
run the command shown to capture the exact string on your machine.

## Host / OS

| Item | Value | How to get it |
|---|---|---|
| Host machine | `dllabgpu` | `hostname` |
| OS version | **Ubuntu 24.04.3 LTS** (noble) | `lsb_release -a` |
| Kernel | **6.17.0-35-generic** | `uname -r` |
| Motherboard | X870E AORUS PRO | `sudo dmidecode -t baseboard` |
| CPU / RAM | `<to fill>` | `lscpu` / `free -h` |

## Tenstorrent device

| Item | Value | Source |
|---|---|---|
| Architecture | **Blackhole** | tt-metal topology discovery |
| Device count | 1 local chip (PCIe id 0) | tt-metal / `tt-smi` |
| PCIe address | `0000:10:00.0` (bus 0x10) | `lspci | grep -i tenstorrent` |
| **PCIe link width** | **x4** (per tt-metal discovery) | `sudo lspci -vv -s 0000:10:00.0 | grep -i lnksta` (needs sudo) |
| Firmware bundle | **19.6.0** | tt-metal log (topology_discovery) |
| KMD (kernel driver) | **2.8.0** | tt-metal log / `tt-smi` |
| IOMMU | disabled | tt-metal log (cluster.cpp) |

> Note: firmware 19.6.0 is newer than the latest "fully tested" 19.5.0 for Blackhole in this
> tt-metal build (a warning is printed at startup); no functional issue observed.

## Software stack

| Item | Value | How to get it |
|---|---|---|
| tt-metal (TT-Metalium) home | `/home/robin/tt-metal` | `echo $TT_METAL_HOME` |
| tt-metal version / commit | **v0.73.0-dev20260614-9-ge76a209ca56** | `cd $TT_METAL_HOME && git describe --tags --always` |
| tt-metal build | `build_Release/` present | — |
| tt-smi | **5.3.0** (reset functional) | `tt-smi --version` |
| Python | **3.12.12** (project venv) | `python --version` |
| CMake | **4.0.2** | `cmake --version` |
| g++ | `<to fill>` | `g++ --version` |

## Device detection result

tt-metal topology discovery detects one Blackhole chip on PCIe id 0 and opens it successfully
(user-mode driver + KMD 2.8.0). Startup log excerpt:

```
Creating TopologyDiscovery for architecture: blackhole
Established firmware bundle version: 19.6.0
Opening local chip ids/PCIe ids: {0}/[0] and remote chip ids {}
IOMMU: disabled    KMD version: 2.8.0
Starting devices in cluster completed.
```

Card reset (used to recover after an aborted run) is functional:

```
tt-smi -r 0        # "Starting reset on devices ... completed warm reset"
```

## Simple test result

The BitLinear operator runs and is validated on the board against the reference vectors:

```
cd tenstorrent_robin
cmake -B build && cmake --build build
./run_reference_tests.sh        # scalar  -> 9/9 bit-exact
./run_reference_tests_mm.sh     # matrix engine -> 10/10, PCC ~= 1
./run_reference_tests_mc.sh     # multi-core    -> 10/10, PCC ~= 1
```

Result: all reference vectors pass (scalar bit-exact; matrix-engine and multi-core at PCC ≈ 1).
See `../REPORT.md` for full results and benchmarks.
