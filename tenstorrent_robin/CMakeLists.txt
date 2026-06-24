cmake_minimum_required(VERSION 3.10)
project(EdgeBox-TT-Alpha)

# Passage forcé à la norme C++20 exigée par Tenstorrent
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fPIC")

include_directories($ENV{TT_METAL_RUNTIME_ROOT}/tt_metal/api)
include_directories($ENV{TT_METAL_RUNTIME_ROOT}/build_Release/include)

add_executable(run_bitnet host_metal.cpp)

# Génération de la librairie partagée
add_library(bitlinear SHARED host_metal.cpp)
set_target_properties(bitlinear PROPERTIES OUTPUT_NAME "bitlinear")
