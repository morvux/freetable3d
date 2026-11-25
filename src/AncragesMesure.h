#pragma once

// Points de mesure « accrochés » à la géométrie : un sommet, le milieu d'une arête, le centre
// d'une face, d'un cercle ou d'une pièce, ou un point d'une face / d'une arête.
//
// Un ancrage retient la pièce et l'élément auquel il est attaché (par son numéro), pas seulement
// ses coordonnées : quand la pièce est déplacée, tournée ou étirée, on retrouve le même élément
// et la mesure suit. Sans dépendance à Qt.

#include <functional>
#include <vector>

#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

namespace ancrage
{

enum class Genre
{
    Libre,        // point fixe de l'espace, attaché à rien
    Sommet,
    MilieuArete,
    CentreCercle, // centre d'une arête circulaire (trou, cylindre)
    CentreFace,
    CentrePiece,  // centre de gravité de la pièce
    SurFace,      // point quelconque d'une face (paramètres u, v)
    SurArete      // point quelconque d'une arête (paramètre u)
};

struct Ancrage
{
    Genre  genre = Genre::Libre;
    int    idPiece = 0;
    int    indice = 0;     // numéro (à partir de 1) du sommet, de l'arête ou de la face dans la pièce
    double u = 0.0;        // paramètres du point pour SurFace / SurArete
    double v = 0.0;
    gp_Pnt position;       // position actuelle dans l'espace
};

// Position d'un ancrage dans une pièce, d'après son numéro d'élément.
bool positionDepuisIndice(const Ancrage& a, const TopoDS_Shape& piece, gp_Pnt& position);

// Points d'accroche proposés pour l'élément « element » (sommet, arête ou face) d'une pièce.
// « centrePiece » (facultatif) évite de le recalculer à chaque survol.
std::vector<Ancrage> candidats(const TopoDS_Shape& piece, const TopoDS_Shape& element, int idPiece,
                               const gp_Pnt* centrePiece);

// Point quelconque d'une face ou d'une arête (quand aucun point d'accroche n'est assez proche).
bool surElement(const TopoDS_Shape& piece, const TopoDS_Shape& element, int idPiece,
                const gp_Pnt& point, Ancrage& resultat);

// Centre de gravité d'une pièce.
gp_Pnt centreDePiece(const TopoDS_Shape& piece);

// Retrouve l'ancrage dans la pièce modifiée. « prediction » donne, pour un point, l'endroit où on
// l'attend (étirement d'une face) ; sans elle, on suppose qu'il n'a pas bougé.
// Retourne false si l'ancrage a été perdu (il devient alors un point libre).
bool recaler(Ancrage& a, const TopoDS_Shape& ancienne, const TopoDS_Shape& nouvelle,
             const std::function<gp_Pnt(const gp_Pnt&)>& prediction = nullptr);

} // namespace ancrage
