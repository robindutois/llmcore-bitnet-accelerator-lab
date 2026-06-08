#include <cstdint>

// Fonction mathématique pure pour le processeur
extern "C" void bitlinear_cpu(const int8_t* x, const int8_t* W, int32_t* y, int M, int K) {
    for (int i = 0; i < M; ++i) {
        int32_t accumulator = 0;
        for (int j = 0; j < K; ++j) {
            // Multiplication matricielle : y = W * x
            accumulator += static_cast<int32_t>(W[i * K + j]) * static_cast<int32_t>(x[j]);
        }
        y[i] = accumulator;
    }
}
