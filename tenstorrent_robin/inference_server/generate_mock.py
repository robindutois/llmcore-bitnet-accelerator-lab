import numpy as np
from safetensors.numpy import save_file

def generate_mock_model(output_path="mock_model.safetensors"):
    print("[*] Generation des tenseurs en memoire...")
    
    # Simulation d'une couche d'attention (Matrice carrée)
    # 4096 * 4096 octets = ~16.7 Mo
    attn_weight = np.random.randint(-1, 2, size=(4096, 4096), dtype=np.int8)
    attn_bias = np.random.randn(4096).astype(np.float32)
    
    # Simulation d'une couche FeedForward (Matrice rectangulaire très large)
    # 4096 * 14336 octets = ~58.7 Mo
    ffn_weight = np.random.randint(-1, 2, size=(4096, 14336), dtype=np.int8)
    ffn_bias = np.random.randn(14336).astype(np.float32)

    # Dictionnaire représentant le modèle complet
    tensors = {
        "layer_0.attention.weight": attn_weight,
        "layer_0.attention.bias": attn_bias,
        "layer_0.feedforward.weight": ffn_weight,
        "layer_0.feedforward.bias": ffn_bias
    }

    print(f"[*] Sauvegarde du modele brut vers {output_path}...")
    save_file(tensors, output_path)
    print("[*] Fichier genere avec succes. Taille estimee : ~75 Mo.")

if __name__ == "__main__":
    generate_mock_model()