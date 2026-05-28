CXX      = g++
CXXFLAGS = -std=c++14 -I..

.PHONY: all test clean

all: test_packing

test_packing: test_packing.cpp ../pack_ternary_2bit.cpp ../unpack_ternary_2bit.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

test: test_packing
	./test_packing

clean:
	rm -f test_packing
