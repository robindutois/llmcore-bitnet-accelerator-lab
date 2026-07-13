#include <cstdint>
#include "api/compute/compute_kernel_api.h"

// Calcul déplacé dans writer.cpp (contexte dataflow, get_read_ptr/get_write_ptr fiables).
// Kernel compute volontairement vide : il ne consomme aucun CB pour ne pas entrer
// en concurrence avec le writer sur cb_in0/cb_in1.
void kernel_main() {
}