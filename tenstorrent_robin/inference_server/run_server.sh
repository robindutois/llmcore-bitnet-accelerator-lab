#!/bin/bash
echo "[*] Configuration de l'environnement Tenstorrent Blackhole..."
export TT_METAL_RUNTIME_ROOT=/home/robin/tt-metal

echo "[*] Lancement du serveur EdgeBox-TT Alpha..."
# Utilisation de l'environnement conda si disponible, sinon python standard
python -m uvicorn api:app --host 127.0.0.1 --port 18080
