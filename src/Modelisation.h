#pragma once

// Noyau de modélisation : fonctions géométriques OpenCascade, sans aucune
// dépendance à Qt. Toute la logique « métier » vit ici, pour pouvoir être
// testée et réutilisée indépendamment de l'interface.

#include <string>
#include <vector>

#include <TopoDS_Shape.hxx>

namespace modelisation
{

// Primitives. Dimensions et positions en millimètres.
// (x, y, z) est le coin du cube, le centre de la base du cylindre
// ou le centre de la sphère.
TopoDS_Shape creerCube(double longueurX, double largeurY, double hauteurZ,
                       double x, double y, double z);
TopoDS_Shape creerCylindre(double rayon, double hauteur,
                           double x, double y, double z);
TopoDS_Shape creerSphere(double rayon, double x, double y, double z);

// Retourne une copie de la forme tournée (degrés, autour des axes X, Y puis Z
// passant par le centre de la forme) puis déplacée (millimètres).
TopoDS_Shape transformer(const TopoDS_Shape& forme,
                         double dx, double dy, double dz,
                         double rotationX, double rotationY, double rotationZ);

// Regroupe plusieurs formes en une seule (sans fusion booléenne).
TopoDS_Shape assembler(const std::vector<TopoDS_Shape>& formes);

// Échange de fichiers. Retournent false en cas d'échec.
bool exporterSTL(const TopoDS_Shape& forme, const std::string& chemin,
                 double deflexion = 0.1);
bool importerSTEP(const std::string& chemin, TopoDS_Shape& forme);

} // namespace modelisation
