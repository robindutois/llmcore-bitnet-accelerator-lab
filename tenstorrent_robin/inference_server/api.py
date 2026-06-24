# ============================================================================
# EdgeBox-TT Alpha: FastAPI Server (Couche 3 - Inference Engine)
# ============================================================================

from fastapi import FastAPI
from pydantic import BaseModel
import uvicorn
import time
import os
import ctypes
import numpy as np

app = FastAPI(title="EdgeBox-TT Alpha API")

# ------------------------------------------------------------------------
# Chargement du moteur matériel C++ (libbitlinear.so) et configuration
# ------------------------------------------------------------------------
lib_path = os.path.join(os.path.dirname(__file__), "libbitlinear.so")
bitnet_engine = None
hardware_mode = False

if os.path.exists(lib_path):
    try:
        bitnet_engine = ctypes.CDLL(lib_path)
        
        # Définition stricte de la signature de la fonction C++
        # void compute_bitlinear_c(int8_t* act, uint8_t* weights, int8_t* out, int M, int K, int N)
        bitnet_engine.compute_bitlinear_c.argtypes = [
            ctypes.POINTER(ctypes.c_int8),   # Pointeurs vers les activations
            ctypes.POINTER(ctypes.c_uint8),  # Pointeurs vers les poids compressés
            ctypes.POINTER(ctypes.c_int8),   # Pointeurs vers le buffer de sortie
            ctypes.c_int,                    # Dimension M (Tokens)
            ctypes.c_int,                    # Dimension K (In_features)
            ctypes.c_int                     # Dimension N (Out_features)
        ]
        bitnet_engine.compute_bitlinear_c.restype = None
        
        hardware_mode = True
        print(f"[*] Succès: Moteur matériel Tenstorrent C++ chargé depuis {lib_path}")
    except Exception as e:
        print(f"[ERREUR] Échec de la configuration ctypes : {e}")
else:
    print(f"[*] Note : {lib_path} non trouvé. Mode simulation pur actif.")

# ------------------------------------------------------------------------
# Modèles de données
# ------------------------------------------------------------------------
class GenerateRequest(BaseModel):
    prompt: str
    max_tokens: int = 50
    temperature: float = 0.7

class GenerateResponse(BaseModel):
    response: str
    compute_time_ms: float
    total_latency_ms: float
    backend_used: str
    tokens_generated: int
    tokens_per_second: float

# ------------------------------------------------------------------------
# Endpoint principal
# ------------------------------------------------------------------------
@app.post("/generate", response_model=GenerateResponse)
async def generate_text(request: GenerateRequest):
    start_total = time.time()
    backend_info = "Simulation (Texte)"
    
    # 1. Préparation des dimensions (Exemple pour une couche du réseau)
    M, K, N = 1, 1024, 1024
    
    if hardware_mode and bitnet_engine:
        # 2. Allocation mémoire pour l'inférence matérielle
        # En production, np_act contiendrait les vraies données du prompt
        np_act = np.random.randint(-128, 127, size=(M, K), dtype=np.int8)
        np_weights = np.random.randint(0, 255, size=(K, N // 4), dtype=np.uint8)
        np_out = np.zeros((M, N), dtype=np.int8)

        # 3. Conversion en pointeurs C compatibles
        act_ptr = np_act.ctypes.data_as(ctypes.POINTER(ctypes.c_int8))
        weights_ptr = np_weights.ctypes.data_as(ctypes.POINTER(ctypes.c_uint8))
        out_ptr = np_out.ctypes.data_as(ctypes.POINTER(ctypes.c_int8))

        # 4. Exécution sur la carte Tenstorrent
        compute_time_start = time.time()
        bitnet_engine.compute_bitlinear_c(act_ptr, weights_ptr, out_ptr, M, K, N)
        compute_time_end = time.time()
        
        backend_info = "Tenstorrent Blackhole (C++)"
    else:
        # Fallback si la carte n'est pas dispo
        compute_time_start = time.time()
        time.sleep(0.01)
        compute_time_end = time.time()

    # --- FORMATAGE DE LA RÉPONSE ---
    mock_response = f"EdgeBox-TT hardware response to: '{request.prompt[:20]}...'"
    tokens_gen = len(mock_response.split())
    
    total_latency = (time.time() - start_total) * 1000.0
    compute_time = (compute_time_end - compute_time_start) * 1000.0
    
    tps = tokens_gen / ((compute_time_end - compute_time_start) + 1e-9)

    return GenerateResponse(
        response=mock_response,
        compute_time_ms=compute_time,
        total_latency_ms=total_latency,
        backend_used=backend_info,
        tokens_generated=tokens_gen,
        tokens_per_second=tps
    )

if __name__ == "__main__":
    print("[*] Lancement du serveur uvicorn sur le port 8000...")
    uvicorn.run(app, host="127.0.0.1", port=8000, log_level="info")
