# FreeTable3D

Logiciel libre de CAO pour l'impression 3D, en C++ avec OpenCascade et Qt 6.

Dépôt : https://github.com/morvux/freetable3d

## Installation

Téléchargez le fichier correspondant à votre système depuis la page des
[versions](https://github.com/morvux/freetable3d/releases).

**Linux** : un paquet `.deb` par distribution (un `.deb` ne s'installe que sur la version de Debian
pour laquelle il a été construit) :

- `freetable3d_0.1.3_debian12_amd64.deb` : Debian 12 / MX Linux 23
- `freetable3d_0.1.3_debian13_amd64.deb` : Debian 13

Puis, par exemple :

    sudo apt install ./freetable3d_0.1.3_debian12_amd64.deb

**Windows 11** : `FreeTable3D-0.1.3-windows-setup.exe`. Windows peut afficher un avertissement
(SmartScreen), car l'installateur n'est pas signé : « Informations complémentaires » puis « Exécuter quand même ».

## Compilation (Debian / MX Linux)

Dépendances :

    sudo apt install build-essential cmake qt6-base-dev libocct-foundation-dev \
        libocct-modeling-data-dev libocct-modeling-algorithms-dev \
        libocct-visualization-dev libocct-data-exchange-dev libgl1-mesa-dev

Compilation et lancement :

    cmake -B build && cmake --build build -j
    ./build/freetable3d

## Licence

Copyright (C) 2026 KREUDER olivier, FreeTable3D.
Contact : okpcbrumath@orange.fr

Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier selon les termes de la
Licence Publique Générale GNU (GNU GPL) telle que publiée par la Free Software Foundation, version 3 de la
licence. Voir le fichier LICENSE.

Ce programme est distribué dans l'espoir qu'il sera utile, mais SANS AUCUNE GARANTIE, sans même la garantie
implicite de COMMERCIALISATION ou D'ADÉQUATION À UN USAGE PARTICULIER.
