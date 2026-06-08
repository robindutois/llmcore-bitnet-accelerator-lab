#include <cstdint>

// Table de décodage (Lookup Table)
static const int8_t decode_table[4] = {0, 1, -1, 0};

extern "C" void bitlinear_cpu_packed(const int8_t* x, const uint8_t* W_packed, int32_t* y, int M, int K) {
    for (int i = 0; i < M; ++i) {
        int32_t accumulator = 0;
        
        for (int j = 0; j < K; ++j) {
            // 1. Calculer l'index global du poids comme si la matrice était aplatie en 1D
            int global_idx = i * K + j;
            
            // 2. Trouver le bon octet et la bonne paire de bits
            int byte_idx = global_idx / 4;
            int bit_pos = global_idx % 4;
            
            // 3. Extraction instantanée de la valeur via masque et table
            uint8_t packed_byte = W_packed[byte_idx];
            uint8_t bits = (packed_byte >> (bit_pos * 2)) & 0x03;
            int8_t weight = decode_table[bits];
            
            // 4. Multiplication et accumulation classique
            accumulator += static_cast<int32_t>(weight) * static_cast<int32_t>(x[j]);
        }
        y[i] = accumulator;
    }
}