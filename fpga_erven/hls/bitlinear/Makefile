# =============================================================
# Makefile — BitLinear HLS standalone build and test
# Run from: fpga_erven/hls/bitlinear/
# =============================================================

CXX      = g++
CXXFLAGS = -std=c++14 -I. -I../packing
PACKING  = ../packing/pack_ternary_2bit.cpp ../packing/unpack_ternary_2bit.cpp
TV_PATH  = $(abspath ../../../reference/test_vectors)

.PHONY: all test clean

all: testbench

testbench: testbench.cpp bitlinear_hls.cpp $(PACKING)
	$(CXX) $(CXXFLAGS) -DTV_BASE_PATH='"$(TV_PATH)"' -o $@ $^

test: testbench
	@echo "--- HLS kernel testbench ---"
	./testbench

clean:
	rm -f testbench
