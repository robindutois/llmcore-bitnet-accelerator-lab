cmake_minimum_required(VERSION 3.10)
project(EdgeBoxTT)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TT_METAL_HOME $ENV{TT_METAL_HOME})

if(NOT TT_METAL_HOME)
    message(FATAL_ERROR "La variable TT_METAL_HOME n'est pas définie.")
endif()

# Inclusion des répertoires API (ceux fournis dans ton archive)
include_directories(${TT_METAL_HOME})
include_directories(${TT_METAL_HOME}/tt_metal/api)
include_directories(${TT_METAL_HOME}/build_Release/include)

add_executable(run_bitnet host_metal.cpp)

# Ajout du chemin des bibliothèques
link_directories(${TT_METAL_HOME}/build_Release/lib)

# FIX: Linkage explicite de la librairie principale ET de ses dépendances système/matériel
# Ajoute ici les librairies nécessaires que l'éditeur de liens réclamait
# 5. Édition de liens avec toutes les dépendances transitives
target_link_libraries(run_bitnet 
    ${TT_METAL_HOME}/build_Release/lib/libtt_metal.so
    ${TT_METAL_HOME}/build_Release/lib/libtt_stl.so
    ${TT_METAL_HOME}/build_Release/lib/libtt-umd.so.0
    ${TT_METAL_HOME}/build_Release/lib/libfmt.so
    pthread
    dl
)