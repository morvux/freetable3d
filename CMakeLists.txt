cmake_minimum_required(VERSION 3.16)
project(CaoLibre VERSION 0.3.0 LANGUAGES CXX)

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
    TKGeomAlgo TKTopAlgo TKPrim TKMesh TKBO TKShHealing
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

add_executable(cao_libre
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
    src/Modelisation.h
    src/Modelisation.cpp
    src/GrilleAxes.h
    src/GrilleAxes.cpp)

target_include_directories(cao_libre PRIVATE ${OCCT_INCLUDE_DIR})
target_link_libraries(cao_libre PRIVATE Qt6::Widgets ${OCCT_LIBRARIES})
