#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

// Déclaration de ta fonction CPU
extern "C" void bitlinear_cpu_packed(const int8_t* x, const uint8_t* W_packed, int32_t* y, int M, int K);

// Fonction pour lire les Golden Vectors d'Erven
template<typename T>
std::vector<T> read_binary_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Impossible d'ouvrir : " + filename);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<T> buffer(size / sizeof(T));
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) return buffer;
    throw std::runtime_error("Erreur de lecture : " + filename);
}

int main() {
    // La liste exacte des 10 tests de la Semaine 1
    std::vector<std::string> tests = {
        "test01_random", "test02_W_zero", "test03_W_plus1", "test04_W_minus1",
        "test05_sparse", "test06_dense", "test07_xmax", "test08_xmin",
        "test09_manual", "test10_large"
    };

    bool all_passed = true;
    std::cout << "[*] Démarrage du banc de test CPU avec les Golden Vectors...\n\n";

    for (const auto& test : tests) {
        std::string path = "../reference/test_vectors/" + test + "/";
        
        try {

            auto x = read_binary_file<int8_t>(path + "activation_int8.bin");
            
            // LECTURE DU FICHIER COMPRESSÉ (en uint8_t)
            auto W_packed = read_binary_file<uint8_t>(path + "weight_ternary_packed_2bit.bin");
            
            auto expected_y = read_binary_file<int32_t>(path + "expected_output_int32.bin");

            int K = x.size();
            int M = expected_y.size();
            std::vector<int32_t> actual_y(M, 0);

            // On injecte les poids compressés dans notre nouvelle fonction
            bitlinear_cpu_packed(x.data(), W_packed.data(), actual_y.data(), M, K);
            
            bool passed = true;
            for (int i = 0; i < M; ++i) {
                if (actual_y[i] != expected_y[i]) {
                    passed = false;
                    break;
                }
            }

            if (passed) std::cout << "[PASS] " << test << "\n";
            else {
                std::cout << "[FAIL] " << test << " (Erreur mathématique détectée)\n";
                all_passed = false;
            }
        } catch (const std::exception& e) {
            std::cout << "[ERR]  " << test << " -> " << e.what() << "\n";
            all_passed = false;
        }
    }

    std::cout << "\n=======================================================\n";
    if (all_passed) std::cout << "✅ RESULTAT : PASS (Logique CPU validée !)\n";
    else std::cout << "❌ RESULTAT : FAIL\n";
    std::cout << "=======================================================\n";

    return 0;
}
