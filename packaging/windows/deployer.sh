#!/usr/bin/env bash
# Fabrique le dossier « dist » (FreeTable3D + toutes ses DLL) à partir de build/freetable3d.exe.
# À lancer dans le terminal MSYS2 UCRT64, depuis n'importe où : le script se place tout seul
# à la racine du projet.
set -euo pipefail

cd "$(dirname "$0")/../.."

if [ ! -f build/freetable3d.exe ]; then
    echo "build/freetable3d.exe est introuvable : compilez d'abord (cmake --build build)." >&2
    exit 1
fi

rm -rf dist
mkdir dist
cp build/freetable3d.exe dist/
cp LICENSE README.md packaging/windows/freetable3d.ico dist/

cd dist

# Qt : DLL, plugin d'affichage Windows (platforms/qwindows.dll) ; pas de traductions Qt (l'application a les siennes)
windeployqt6 --no-translations freetable3d.exe

# Autres bibliothèques (OpenCascade, compilateur, etc.), pour l'exécutable et les plugins Qt
for passe in 1 2; do
    # Les « || true » évitent l'arrêt du script (set -e / pipefail) quand il n'y a plus rien à copier
    # (grep sans résultat, ou cp -n qui renvoie une erreur pour un fichier déjà présent).
    find . \( -name '*.exe' -o -name '*.dll' \) -print0 \
        | { xargs -0 -n1 ldd 2>/dev/null || true; } \
        | { grep -i '/ucrt64/' || true; } \
        | awk '{print $3}' \
        | sort -u \
        | { xargs -r -I{} sh -c 'cp -n "{}" . || true'; }
done

echo
echo "Dossier prêt : $(pwd)"
echo "Taille : $(du -sh . | cut -f1)"
echo "Test : double-cliquez sur dist\\freetable3d.exe depuis l'Explorateur Windows."
