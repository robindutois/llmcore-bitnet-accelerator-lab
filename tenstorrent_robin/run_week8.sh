#!/bin/bash
echo "========================================================="
echo "[*] Démarrage de la compilation (Semaine 8)"
echo "========================================================="

export TT_METAL_RUNTIME_ROOT=/home/robin/tt-metal
DEST_DIR="$TT_METAL_RUNTIME_ROOT/build_Release/include/spdlog/fmt"

echo "[*] Étape 1 : Réparation de l'environnement Tenstorrent..."
# Si le dossier manquant n'est pas à sa place, on le cherche et on le copie
if [ ! -d "$DEST_DIR/bundled" ]; then
    echo " -> Le dossier 'bundled' est manquant. Recherche sur le disque..."
    # Recherche du dossier caché dans les sources third_party
    SRC_DIR=$(find "$TT_METAL_RUNTIME_ROOT" -type d -name "bundled" | grep "fmt/bundled" | head -n 1)
    
    if [ ! -z "$SRC_DIR" ]; then
        echo " -> Dossier trouvé ! Copie en cours..."
        cp -r "$SRC_DIR" "$DEST_DIR/"
        echo " -> Réparation terminée."
    else
        echo " [!] Impossible de trouver le dossier source sur le serveur."
    fi
else
    echo " -> Le dossier est déjà à sa place."
fi

echo "---------------------------------------------------------"
echo "[*] Étape 2 : Nettoyage et Compilation..."
rm -rf build
mkdir -p build
cd build
cmake ..
make -j4
cd ..

echo "---------------------------------------------------------"
echo "[*] Étape 3 : Vérification finale..."
if [ -f "build/libbitlinear.so" ]; then
    echo "[*] SUCCÈS : libbitlinear.so généré !"
    echo "[*] Copie vers le serveur API..."
    cp build/libbitlinear.so inference_server/
else
    echo "[!] ERREUR : La compilation a échoué."
    exit 1
fi
