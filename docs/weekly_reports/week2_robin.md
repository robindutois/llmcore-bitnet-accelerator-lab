# Weekly Report - Week 2
**Author:** Robin Dutois
**Date:** June 15, 2026
**Project:** LLMCore BitNet Accelerator (Tenstorrent)

## 🎯 Objectives for the Week
* Implement the 2-bit memory compression algorithm (packing) on the CPU to drastically reduce the RAM footprint.
* Establish the software bridge between the high-level Python API and the low-level C++ math engine.
* Deploy a functional, end-to-end inference server prototype (Software Mock).

## ✅ Achievements
### 1. 2-Bit Ternary Packing (C++ Implementation)
* **Algorithm:** Developed a highly optimized C++ unpacking kernel using a Lookup Table (LUT) to instantly decode 4 weights per byte without slow branching (`if/else`).
* **Edge Case Resolution:** Identified and resolved a memory alignment issue (global indexing) caused by matrices with non-multiple-of-4 dimensions, ensuring perfect synchronization with the Python reference model.
* **Validation:** Achieved 100% bit-accurate validation against all Golden Vectors, confirming zero precision loss during decompression.

### 2. Inference Server & C++ Binding
* **Shared Library:** Compiled the C++ mathematical engine into a dynamic shared object (`libbitlinear.so`).
* **Python Wrapper (ctypes):** Engineered a direct memory bridge to pass raw pointers from Python to C++, bypassing the Python Global Interpreter Lock (GIL) for heavy calculations.
* **FastAPI Deployment:** Built and successfully tested a REST API (`POST /predict`). The system seamlessly receives web requests, processes the inference in native C++, and returns the computed tensors.

## 🚧 Next Steps (Week 3)
* Pending hardware availability (awaiting Manager Kim's approval for the workstation upgrade), transition the CPU mock logic to the actual Tenstorrent Blackhole p150a RISC-V cores.
* Implement real-world LLM checkpoint loading (parsing actual BitNet weights instead of randomly generated matrices).
