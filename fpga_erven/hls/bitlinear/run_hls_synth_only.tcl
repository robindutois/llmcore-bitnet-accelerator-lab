set project_name "bitlinear_hls_project"
set top_function  "bitlinear_hls"
set part          "xczu7ev-ffvc1156-2-e"
set clock_period  "10"

open_project -reset $project_name
set_top $top_function

# Kernel only — no external packing files needed
add_files bitlinear_hls.cpp
add_files bitlinear_hls.h

open_solution -reset "solution1"
set_part $part
create_clock -period $clock_period -name default

puts ">>> Running HLS Synthesis..."
csynth_design

puts ">>> Done. Check: $project_name/solution1/syn/report/"
