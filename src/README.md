# FreeTable3D

Logiciel libre de CAO pour l'impression 3D : un établi numérique pour dessiner des esquisses, créer des
volumes, les assembler et les exporter en STL. Écrit en C++ avec OpenCascade et Qt 6.

## Fonctions

- Pièces de base (cube, cylindre, sphère), import STEP, export STL (sélection ou tout, finesse réglable)
- Sélection de pièces, faces et arêtes ; déplacement, rotation et étirement à la souris
- Panneau de propriétés : dimensions, position, orientation, diamètre modifiables au clavier
- Opérations booléennes, congés, chanfreins, trous, suppression de face
- Esquisses 2D (rectangle, cercle, polygone) avec cotes au clavier et extrusion ; pièces paramétriques
- Plateau d'imprimante affiché à l'échelle, vérification avant impression
- Projets enregistrés au format `.ft3d`

## Compilation (Debian / MX Linux)

    cmake -B build && cmake --build build -j
    ./build/freetable3d

## Licence

Copyright (C) 2026 les auteurs de FreeTable3D.

Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier selon les termes de la
Licence Publique Générale GNU (GNU GPL) telle que publiée par la Free Software Foundation, version 3 de la
licence. Voir le fichier LICENSE.
