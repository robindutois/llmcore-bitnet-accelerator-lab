import ctypes
import numpy as np
from fastapi import FastAPI
from pydantic import BaseModel

# 1. Chargement de ta bibliothèque C++ compilée
lib = ctypes.CDLL('./libbitlinear.so')

# 2. Définition stricte des types d'entrée/sortie pour le C++
# Signature : void bitlinear_cpu_packed(int8_t* x, uint8_t* W_packed, int32_t* y, int M, int K)
lib.bitlinear_cpu_packed.argtypes = [
    np.ctypeslib.ndpointer(dtype=np.int8, ndim=1, flags='C_CONTIGUOUS'),
    np.ctypeslib.ndpointer(dtype=np.uint8, ndim=1, flags='C_CONTIGUOUS'),
    np.ctypeslib.ndpointer(dtype=np.int32, ndim=1, flags='C_CONTIGUOUS'),
    ctypes.c_int,
    ctypes.c_int
]

# 3. Création de l'API web
app = FastAPI(title="LLMCore BitNet API", description="Inference Engine for Tenstorrent")

class InferenceRequest(BaseModel):
    M: int
    K: int
    # En situation réelle, on recevrait les vecteurs ici. 
    # Pour le test, on va générer des matrices aléatoires côté serveur.

@app.post("/predict")
def predict(req: InferenceRequest):
    # Simulation des données entrantes (Activations et Poids compressés)
    x = np.random.randint(-1, 2, size=req.K, dtype=np.int8)
    
    # K_packed : on divise par 4 car 4 poids par octet
    k_packed = (req.K + 3) // 4 
    w_packed = np.random.randint(0, 256, size=(req.M * k_packed), dtype=np.uint8)
    
    # Vecteur de sortie vide
    y = np.zeros(req.M, dtype=np.int32)

    # L'appel magique : Python exécute directement ton code C++ !
    lib.bitlinear_cpu_packed(x, w_packed, y, req.M, req.K)

    return {
        "status": "success",
        "message": f"Calcul effectué en C++ sur une matrice {req.M}x{req.K}",
        "y_preview": y[:5].tolist() # On renvoie les 5 premières valeurs
    }