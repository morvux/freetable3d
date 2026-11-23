#pragma once

// Noyau de modélisation : fonctions géométriques OpenCascade, sans aucune
// dépendance à Qt. Toute la logique « métier » vit ici, pour pouvoir être
// testée et réutilisée indépendamment de l'interface.

#include <string>
#include <vector>

#include <vector>

#include <TopAbs_ShapeEnum.hxx>
#include <gp_Trsf.hxx>

#include "Esquisse.h"
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Quaternion.hxx>
#include <gp_Vec.hxx>

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

// Si la forme est une face plane, renvoie true et donne un point de la face
// (son centre) et la normale unitaire dirigée vers l'extérieur de la pièce.
bool reperFacePlane(const TopoDS_Shape& forme, gp_Pnt& point, gp_Vec& normaleSortante);
bool estFacePlane(const TopoDS_Shape& forme);

// « Presser-tirer » : déplace une face plane le long de sa normale.
// distance > 0 : la pièce s'allonge vers l'extérieur ; distance < 0 : la face rentre
// dans la pièce. Retourne la forme inchangée si l'opération échoue.
// La face doit appartenir à la forme.
TopoDS_Shape etirerFace(const TopoDS_Shape& forme, const TopoDS_Shape& face, double distance);

// Descend ou remonte la forme pour que son point le plus bas soit exactement à Z = 0
// (sur le plateau d'impression).
TopoDS_Shape poserSurPlateau(const TopoDS_Shape& forme);

// Fait pivoter la forme pour que la face plane donnée (qui doit lui appartenir) soit
// dirigée vers le bas, puis la pose sur le plateau. Inchangée si la face n'est pas plane.
TopoDS_Shape poserFaceAPlat(const TopoDS_Shape& forme, const TopoDS_Shape& face);

// Regroupe plusieurs formes en une seule (sans fusion booléenne).
TopoDS_Shape assembler(const std::vector<TopoDS_Shape>& formes);

// Échange de fichiers. Retournent false en cas d'échec.
bool exporterSTL(const TopoDS_Shape& forme, const std::string& chemin,
                 double deflexion = 0.1);
bool importerSTEP(const std::string& chemin, TopoDS_Shape& forme);
// Export STEP (format d'échange CAO lu par FreeCAD, etc.) ; chaque forme devient un objet distinct.
bool exporterSTEP(const std::vector<TopoDS_Shape>& formes, const std::string& chemin);



// Mesures d'une pièce, d'une face ou d'une arête, pour l'affichage des propriétés.
struct Mesures
{
    TopAbs_ShapeEnum type = TopAbs_SHAPE;

    // Boîte englobante alignée sur les axes du monde (mm)
    bool   boiteValide = false;
    double xmin = 0.0, ymin = 0.0, zmin = 0.0;
    double xmax = 0.0, ymax = 0.0, zmax = 0.0;

    double aire     = 0.0; // face, ou surface totale d'une pièce (mm²)
    double volume   = 0.0; // pièce (mm³)
    double longueur = 0.0; // arête (mm)

    // Diamètres (mm) des cylindres, sphères et cercles, sans doublon
    std::vector<double> diametres;

    // Normale sortante (face plane) ou direction (arête droite), de longueur 1
    bool   aDirection = false;
    gp_Vec direction;
};

Mesures mesurer(const TopoDS_Shape& forme);

// Opérations booléennes : la base est combinée successivement avec chaque outil.
enum class OperationBooleenne { Union, Soustraction, Intersection };
bool booleen(const TopoDS_Shape& base, const std::vector<TopoDS_Shape>& outils,
             OperationBooleenne operation, TopoDS_Shape& resultat);

// Arêtes à traiter à partir d'éléments sélectionnés : une arête compte pour elle-même,
// une face pour toutes ses arêtes. Sans doublon.
std::vector<TopoDS_Shape> aretesDe(const std::vector<TopoDS_Shape>& elements);

// Congé (arrondi, « distance » = rayon) ou chanfrein (« distance » = largeur) sur les arêtes.
// Retourne false si OpenCascade n'y arrive pas (distance trop grande, arêtes incompatibles).
bool congeOuChanfrein(const TopoDS_Shape& forme, const std::vector<TopoDS_Shape>& aretes,
                      double distance, bool chanfrein, TopoDS_Shape& resultat);

// Supprime des faces et referme la pièce autour (défeaturing) : perçages, bosses, congés...
// Échoue si les faces voisines ne peuvent pas se prolonger pour refermer la pièce
// (par exemple une face d'un simple cube).
bool supprimerFaces(const TopoDS_Shape& forme, const std::vector<TopoDS_Shape>& faces,
                    TopoDS_Shape& resultat);

// Extrusion de profils plans (faces) le long d'un vecteur ; les prismes sont fusionnés en un solide.
bool extruderProfils(const std::vector<TopoDS_Shape>& faces, const gp_Vec& vecteur,
                     TopoDS_Shape& resultat);

// Perce un trou cylindrique dans la pièce depuis une face plane : au centre de la face,
// décalé de (du, dv) mm dans le plan de la face. « profondeur » <= 0 : trou traversant.
bool percer(const TopoDS_Shape& forme, const TopoDS_Shape& face, double diametre,
            double profondeur, double du, double dv, TopoDS_Shape& resultat,
            std::string* raison = nullptr);

// Contrôle d'une pièce avant impression.
struct Verification
{
    bool   valide = false;   // la topologie est saine (BRepCheck)
    int    nbSolides = 0;
    double volume = 0.0;     // mm³
    double dx = 0.0, dy = 0.0, dz = 0.0; // dimensions de la boîte englobante
};
Verification verifier(const TopoDS_Shape& forme);

// Transformation (rotations puis translation) que « transformer » applique à cette forme :
// utile pour mémoriser le placement d'une pièce paramétrique.
gp_Trsf matriceTransformation(const TopoDS_Shape& forme,
                              double dx, double dy, double dz,
                              double rotationX, double rotationY, double rotationZ);

// Reconstruit la forme d'une pièce paramétrique (profils extrudés), dans le plan d'origine.
bool genererParametrique(const Parametrique& parametres, TopoDS_Shape& forme);

// Évidement : creuse la pièce en gardant une paroi de « epaisseur » mm ; les faces données
// sont ouvertes (retirées). Échoue si l'épaisseur est trop forte pour la géométrie.
bool evider(const TopoDS_Shape& forme, const std::vector<TopoDS_Shape>& facesOuvertes,
            double epaisseur, TopoDS_Shape& resultat);

// Édition numérique. Le coin bas (xmin, ymin, zmin) reste fixe.
// Un parallélépipède est reconstruit exactement ; sinon la forme est étirée (GTransform).
TopoDS_Shape redimensionner(const TopoDS_Shape& forme, double nx, double ny, double nz);

// Vrai pour une sphère seule ou un cylindre droit seul (3 faces).
bool diametreModifiable(const TopoDS_Shape& forme);
// Change le diamètre d'une sphère (centre fixe) ou d'un cylindre (axe et base fixes,
// hauteur conservée). Retourne la forme inchangée si elle n'est ni l'un ni l'autre.
TopoDS_Shape changerDiametre(const TopoDS_Shape& forme, double diametre);

// Orientation : quaternion de la rotation « autour de X, puis de Y, puis de Z »
// (angles en degrés, axes fixes du monde) et conversion inverse en angles.
gp_Quaternion quaternionRotation(double rx, double ry, double rz);
void          anglesEuler(const gp_Quaternion& rotation, double& rx, double& ry, double& rz);

// Rotation appliquée par poserFaceAPlat à une pièce dont on pose cette face (identité si aucune).
gp_Quaternion rotationPoserFaceAPlat(const TopoDS_Shape& face);

} // namespace modelisation
