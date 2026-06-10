import os
import sys
import json
import numpy as np
from safetensors.numpy import load_file, save_file

# Import de l'algorithme de compression d'Erven
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'reference')))
import packing_utils

def compile_model(input_safetensors_path, output_safetensors_path, metadata_path):
    print(f"[*] Chargement du modele original : {input_safetensors_path}")
    
    try:
        tensors = load_file(input_safetensors_path)
    except Exception as e:
        print(f"[!] Erreur lors du chargement : {e}")
        return

    packed_tensors = {}
    metadata = {}

    print("[*] Debut de la compression (Pre-packing 2-bits)...")
    
    for layer_name, tensor in tensors.items():
        # On cible uniquement les matrices de poids 2D au format int8
        if len(tensor.shape) == 2 and tensor.dtype == np.int8:
            M, K = tensor.shape
            print(f"  -> Compression de la couche : {layer_name} ({M}x{K})")
            
            # Application de la compression
            packed_tensor = packing_utils.pack_matrix(tensor)
            packed_tensors[layer_name] = packed_tensor
            
            # Sauvegarde des dimensions originales necessaires pour le calcul C++
            metadata[layer_name] = {
                "type": "packed_2bit",
                "M": M,
                "K": K
            }
        else:
            # Si le tenseur est un biais (1D) ou d'un autre type, on le copie tel quel
            print(f"  -> Copie sans compression : {layer_name} (Shape: {tensor.shape})")
            packed_tensors[layer_name] = tensor
            metadata[layer_name] = {
                "type": "raw",
                "shape": list(tensor.shape)
            }

    print(f"[*] Sauvegarde du modele compresse : {output_safetensors_path}")
    save_file(packed_tensors, output_safetensors_path)

    print(f"[*] Sauvegarde des metadonnees : {metadata_path}")
    with open(metadata_path, 'w', encoding='utf-8') as f:
        json.dump(metadata, f, indent=4)

    print("[*] Compilation terminee avec succes.")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python compiler.py <input.safetensors> <output.safetensors>")
        sys.exit(1)
        
    input_path = sys.argv[1]
    output_path = sys.argv[2]
    meta_path = output_path.replace('.safetensors', '_meta.json')
    
    compile_model(input_path, output_path, meta_path)