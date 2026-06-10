import ctypes
import json
import numpy as np
from fastapi import FastAPI
from pydantic import BaseModel
from contextlib import asynccontextmanager
from safetensors.numpy import load_file

# --- CHARGEMENT DU MOTEUR C++ ---
lib = ctypes.CDLL('./libbitlinear.so')
lib.bitlinear_cpu_packed.argtypes = [
    np.ctypeslib.ndpointer(dtype=np.int8, ndim=1, flags='C_CONTIGUOUS'),
    np.ctypeslib.ndpointer(dtype=np.uint8, ndim=1, flags='C_CONTIGUOUS'),
    np.ctypeslib.ndpointer(dtype=np.int32, ndim=1, flags='C_CONTIGUOUS'),
    ctypes.c_int,
    ctypes.c_int
]

# --- VARIABLES GLOBALES ---
MODEL_MEMORY = {}
MODEL_META = {}

# --- CYCLE DE VIE DU SERVEUR ---
@asynccontextmanager
async def lifespan(app: FastAPI):
    print("[*] Demarrage du serveur...")
    
    print("[*] Chargement de la topologie du modele (Metadonnees)...")
    try:
        with open("model_packed_meta.json", "r", encoding="utf-8") as f:
            global MODEL_META
            MODEL_META = json.load(f)
    except FileNotFoundError:
        print("[!] Erreur : Fichier model_packed_meta.json introuvable.")
        yield
        return

    print("[*] Chargement des poids quantifies en RAM (Acces instantane)...")
    try:
        tensors = load_file("model_packed.safetensors")
        for name, tensor in tensors.items():
            MODEL_MEMORY[name] = tensor
    except Exception as e:
        print(f"[!] Erreur de lecture Safetensors : {e}")
        
    print("[*] Serveur pret et modele en ligne !")
    
    yield 
    
    print("[*] Arret du serveur, liberation de la memoire vive...")
    MODEL_MEMORY.clear()
    MODEL_META.clear()

app = FastAPI(title="LLMCore BitNet API", lifespan=lifespan)

class InferenceRequest(BaseModel):
    input_vector: list[int]

@app.post("/predict")
def predict(req: InferenceRequest):
    # Pour ce test, on cible la matrice d'attention generee precedemment
    layer_name = "layer_0.attention.weight"
    
    if layer_name not in MODEL_MEMORY or layer_name not in MODEL_META:
        return {"error": f"Couche {layer_name} introuvable en memoire."}
        
    W_packed = MODEL_MEMORY[layer_name]
    meta = MODEL_META[layer_name]
    
    # Recuperation des vraies dimensions depuis les metadonnees
    M = meta["M"]
    K = meta["K"]
    
    x = np.array(req.input_vector, dtype=np.int8)
    
    if len(x) != K:
        return {"error": f"Dimension mismatch : le modele attend {K}, recu {len(x)}"}
    
    y = np.zeros(M, dtype=np.int32)

    # Execution du calcul matriciel natif
    lib.bitlinear_cpu_packed(x, W_packed, y, M, K)

    return {
        "status": "success",
        "message": f"Inference reussie avec les poids pre-compresses ({M}x{K})",
        "y_preview": y[:10].tolist() 
    }