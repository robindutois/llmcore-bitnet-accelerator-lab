# Install Log — Tenstorrent Blackhole bring-up

Steps taken to get the Tenstorrent Blackhole card usable for the BitNet BitLinear work
(tt-metal / bare-metal kernels). Commands marked `<...>` depend on your exact versions —
adjust to what is installed. This is a record of the working setup, not a from-scratch script.

## 1. Hardware

- Tenstorrent **Blackhole** accelerator card installed in the lab server (`dllabgpu`).
- Connected on a **PCIe x4** slot (see `environment_check.md`). Confirm the card is seen:
  ```bash
  lspci | grep -i tenstorrent          # -> 0000:10:00.0 ... Tenstorrent
  ```

## 2. Kernel-mode driver (tt-kmd)

- KMD (kernel module) version **2.8.0** is loaded (reported by tt-metal at startup).
  ```bash
  lsmod | grep -i tenstorrent          # tenstorrent kernel module loaded
  dmesg | grep -i tenstorrent | tail   # driver / device messages
  ```
- Source / install: <https://github.com/tenstorrent/tt-kmd> (`<installed via apt / dkms — to confirm>`).

## 3. System management tools (tt-smi)

- `tt-smi` is installed (Python tool) and used to inspect and **reset** the card:
  ```bash
  tt-smi                # device status / telemetry
  tt-smi -r 0           # warm reset device 0 (recover after an aborted run)
  ```
- If `tt-smi` is not on PATH, it lives in a Python venv; install with `pip install tt-smi`.

## 4. Firmware

- Firmware bundle **19.6.0** is flashed on the board (reported by tt-metal). Managed with
  `tt-flash` if an update is needed (<https://github.com/tenstorrent/tt-flash>). No update was
  required for this work.

## 5. TT-Metalium (tt-metal)

- Version **v0.73.0-dev20260614-9-ge76a209ca56**. Cloned and built at
  `TT_METAL_HOME=/home/robin/tt-metal`, Release build:
  ```bash
  export TT_METAL_HOME=/home/robin/tt-metal
  cd $TT_METAL_HOME
  ./build_metal.sh                     # produces build_Release/ (lib + includes)
  ```
- Our project links against `build_Release/lib` and includes `tt_metal/api` (see
  `../CMakeLists.txt`). Kernels are JIT-compiled by tt-metal at runtime.
- Compute kernels use the SFPI toolchain shipped with tt-metal (`runtime/sfpi`).

## 6. Python / build environment

- Python **3.12.12** in a project venv for tooling (`tt-smi` 5.3.0, benchmark scripts).
- C++ build: **CMake 4.0.2** + g++ (host) ; kernel build handled by tt-metal's JIT
  (SFPI riscv-tt-elf-g++).

## 7. Build & smoke test of this project

```bash
cd tenstorrent_robin
cmake -B build
cmake --build build                    # run_bitnet, run_bench, run_bitnet_mm/mc, run_bench_mm/mc
tt-smi -r 0                            # optional clean reset
./run_reference_tests.sh              # -> 9/9 bit-exact on the board
```

## Notes / gotchas learned during bring-up

- After a crashed/`Ctrl-C`'d run the card can hang; **`tt-smi -r 0`** clears it. Kill any stray
  `run_*` process first (`pkill -9 -f run_bitnet`).
- Run executables from `tenstorrent_robin/` so the relative `kernels*/…` paths resolve.
- On the compute (TRISC) side, `get_read_ptr`/`get_write_ptr` are **not** available — those L1
  address helpers exist only in the data-movement (dataflow) context. This shaped the design
  (see `../REPORT.md`).
- The host motherboard (`X870E AORUS PRO`) is unknown to tt-metal's board table → a harmless
  warning at startup (`falling back to bus_id as tray_id`); no functional impact.
