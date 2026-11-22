#pragma once

// Description paramétrique d'une esquisse et de l'extrusion qui en est tirée.
// Fait partie du noyau : aucune dépendance à Qt.

#include <vector>

#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

enum class TypeProfil
{
    Rectangle, // v = {u, v, largeur, hauteur} : coin « bas-gauche » et dimensions
    Cercle,    // v = {u, v, rayon}            : centre et rayon
    Polygone   // v = {u0, v0, u1, v1, ...}    : sommets successifs (fermé)
};

struct Profil
{
    TypeProfil          type = TypeProfil::Rectangle;
    std::vector<double> v;
};

// Coordonnées (u, v) dans le plan d'esquisse : origine + u * axeU + v * axeV,
// avec axeV = normale ^ axeU.
struct Esquisse
{
    gp_Pnt              origine{0.0, 0.0, 0.0};
    gp_Dir              normale{0.0, 0.0, 1.0};
    gp_Dir              axeU{1.0, 0.0, 0.0};
    std::vector<Profil> profils;
};

// Une pièce issue d'une extrusion : on peut modifier l'esquisse ou la hauteur.
struct Parametrique
{
    Esquisse esquisse;
    double   hauteur = 10.0; // positive = dans le sens de la normale
};
