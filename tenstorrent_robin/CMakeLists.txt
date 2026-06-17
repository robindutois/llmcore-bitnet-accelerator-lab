cmake_minimum_required(VERSION 3.20)
project(BitNet_Accelerator CXX)
set(CMAKE_CXX_STANDARD 20)
set(TT_DIR "/home/robin/tt-metal")

# L'arme anti-linker : on force fmt a s'auto-compiler dans ton code
add_compile_definitions(FMT_HEADER_ONLY)

include_directories(${TT_DIR})
include_directories(${TT_DIR}/tt_metal)
include_directories(${TT_DIR}/tt_metal/api)
include_directories(${TT_DIR}/tt_metal/api/tt-metalium)
include_directories(${TT_DIR}/tt_metal/impl/dispatch)
include_directories(${TT_DIR}/build_Release/include)
include_directories(${TT_DIR}/.cpmcache/spdlog/b1c2586bb5c35a7929362e87f62433eb68206873/include)
include_directories(${TT_DIR}/.cpmcache/fmt/69912fb6b71fcb1f7e5deca191a2bb4748c4e7b6/include)

add_executable(run_bitnet host_metal.cpp)

find_library(TT_METAL_LIB NAMES tt_metal PATHS ${TT_DIR}/build/lib REQUIRED)
find_library(TT_UMD_LIB NAMES tt-umd PATHS ${TT_DIR}/build/lib REQUIRED)

# On a supprime "fmt" de la liste de liaison finale
target_link_libraries(run_bitnet PRIVATE 
    ${TT_METAL_LIB} 
    ${TT_UMD_LIB} 
    pthread
)
