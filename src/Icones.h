#pragma once

// Icônes de la barre d'outils, dessinées par code (aucun fichier image à installer).

#include <QIcon>
#include <QString>

namespace icones
{

// Noms : cube, cylindre, sphere, annuler, refaire, piece, face, arete, rotation, plateau,
// faceaplat, conge, chanfrein, trou, union, soustraire, intersection, esquisse, rectangle,
// cercle, polygone, effacer, extruder, quitter, supprimer, dessus, devant, cote, iso,
// recadrer, grille. Retourne une icône vide pour un nom inconnu.
QIcon creer(const QString& nom);

} // namespace icones
