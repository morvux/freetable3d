#!/bin/bash
# Construit un AppImage de FreeTable3D (un seul fichier exécutable, utilisable sur la plupart des
# distributions Linux récentes). À lancer depuis n'importe où : bash packaging/appimage/construire.sh
#
# Pour une compatibilité maximale, construire sur la distribution la plus ancienne visée
# (Debian 12 par exemple), car l'AppImage dépend de la version de la bibliothèque C du système de
# construction.
set -e

RACINE="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$RACINE"

VERSION="$(grep -m1 -oP 'project\(FreeTable3D VERSION \K[0-9.]+' CMakeLists.txt)"
OUTILS="$RACINE/outils"
mkdir -p "$OUTILS"

echo "== Compilation de FreeTable3D $VERSION =="
cmake -B build-appimage -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-appimage -j

echo "== Préparation du dossier AppDir =="
rm -rf AppDir
DESTDIR="$RACINE/AppDir" cmake --install build-appimage

echo "== Outils linuxdeploy =="
if [ ! -x "$OUTILS/linuxdeploy-x86_64.AppImage" ]; then
    wget -O "$OUTILS/linuxdeploy-x86_64.AppImage" \
        https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
    chmod +x "$OUTILS/linuxdeploy-x86_64.AppImage"
fi
if [ ! -x "$OUTILS/linuxdeploy-plugin-qt-x86_64.AppImage" ]; then
    wget -O "$OUTILS/linuxdeploy-plugin-qt-x86_64.AppImage" \
        https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
    chmod +x "$OUTILS/linuxdeploy-plugin-qt-x86_64.AppImage"
fi

echo "== Assemblage de l'AppImage =="
export PATH="$OUTILS:$PATH"
export QMAKE="$(command -v qmake6 || echo /usr/lib/qt6/bin/qmake6)"
export NO_STRIP=1
export VERSION
export APPIMAGE_EXTRACT_AND_RUN=1   # fonctionne aussi sans FUSE (conteneur)
export LINUXDEPLOY_OUTPUT_VERSION="$VERSION"

"$OUTILS/linuxdeploy-x86_64.AppImage" \
    --appdir AppDir \
    --plugin qt \
    --output appimage

FICHIER="$(ls -1 FreeTable3D*.AppImage 2>/dev/null | head -n1)"
if [ -z "$FICHIER" ]; then
    echo "Erreur : aucun AppImage produit." >&2
    exit 1
fi
mv "$FICHIER" "FreeTable3D-$VERSION-x86_64.AppImage"
echo "== Terminé : $RACINE/FreeTable3D-$VERSION-x86_64.AppImage =="
