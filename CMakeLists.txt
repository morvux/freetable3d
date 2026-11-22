cmake_minimum_required(VERSION 3.16)
project(FreeTable3D VERSION 0.1.0 LANGUAGES CXX)

# Sans précision, on compile en Release (optimisé) : beaucoup plus fluide.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Type de compilation" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)

# Qt 6 : interface graphique (LGPL)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# OpenCascade : on évite find_package(OpenCASCADE), dont la config Debian
# exige VTK. On localise directement les en-têtes et les bibliothèques.
find_path(OCCT_INCLUDE_DIR Standard_Version.hxx
          PATH_SUFFIXES opencascade REQUIRED)

set(OCCT_LIB_NAMES
    TKernel TKMath TKG2d TKG3d TKGeomBase TKBRep
    TKGeomAlgo TKTopAlgo TKPrim TKMesh TKBO TKFillet TKOffset TKShHealing
    TKXSBase TKService TKHLR TKV3d TKOpenGl)

set(OCCT_LIBRARIES "")
foreach(nom ${OCCT_LIB_NAMES})
    find_library(OCCT_LIB_${nom} NAMES ${nom} REQUIRED)
    list(APPEND OCCT_LIBRARIES ${OCCT_LIB_${nom}})
endforeach()

# Les bibliothèques d'échange de fichiers ont changé de nom selon la version
# d'OpenCascade (TKSTL/TKDESTL, TKSTEP/TKDESTEP).
find_library(OCCT_LIB_STL  NAMES TKSTL  TKDESTL  REQUIRED)
find_library(OCCT_LIB_STEP NAMES TKSTEP TKDESTEP REQUIRED)
list(APPEND OCCT_LIBRARIES ${OCCT_LIB_STL} ${OCCT_LIB_STEP})

add_executable(freetable3d
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/ViewerWidget.h
    src/ViewerWidget.cpp
    src/DialogueDimensions.h
    src/DialogueDimensions.cpp
    src/DialogueTransformation.h
    src/DialogueTransformation.cpp
    src/Document.h
    src/Document.cpp
    src/PanneauProprietes.h
    src/PanneauProprietes.cpp
    src/Esquisse.h
    src/Icones.h
    src/Icones.cpp
    src/Modelisation.h
    src/Modelisation.cpp
    src/GrilleAxes.h
    src/GrilleAxes.cpp)

# Windows : application graphique, sans fenêtre de console noire.
if(WIN32)
    set_target_properties(freetable3d PROPERTIES WIN32_EXECUTABLE ON)
endif()

# Numéro de version, affiché dans « À propos »
target_compile_definitions(freetable3d PRIVATE FREETABLE3D_VERSION="${PROJECT_VERSION}")

target_include_directories(freetable3d PRIVATE ${OCCT_INCLUDE_DIR})
target_link_libraries(freetable3d PRIVATE Qt6::Widgets ${OCCT_LIBRARIES})
