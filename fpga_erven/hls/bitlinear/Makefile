# =============================================================
# Makefile — BitLinear HLS standalone build and test
# Run from: fpga_erven/hls/bitlinear/
# =============================================================

CXX      = g++
CXXFLAGS = -std=c++14 -I. -I../packing
PACKING  = ../packing/pack_ternary_2bit.cpp ../packing/unpack_ternary_2bit.cpp
VECTORS  = ../../../../reference/test_vectors

.PHONY: all test clean

all: testbench testbench_week2_vectors

# Standard testbench (inline random tests)
testbench: testbench.cpp bitlinear_hls.cpp $(PACKING)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Cross-validation against Week 2 binary test vectors
testbench_week2_vectors: testbench_week2_vectors.cpp bitlinear_hls.cpp $(PACKING)
	$(CXX) $(CXXFLAGS) -o $@ $^

test: testbench testbench_week2_vectors
	@echo "--- Inline testbench ---"
	./testbench
	@echo ""
	@echo "--- Week 2 cross-validation ---"
	./testbench_week2_vectors $(VECTORS)

clean:
	rm -f testbench testbench_week2_vectors
