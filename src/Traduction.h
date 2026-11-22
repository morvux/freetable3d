#pragma once

// Choix de la langue de l'interface (français ou anglais).
// Le français est la langue d'origine du code (les textes sont écrits en français dans tr()) ;
// l'anglais vient d'une table intégrée au programme (TraductionEn.cpp). Aucun fichier à installer.

#include <QString>

class QCoreApplication;

namespace traduction
{

struct Entree
{
    const char* francais;
    const char* anglais;
};
extern const Entree kTableAnglais[];
extern const int    kNombreAnglais;

// Langue mémorisée (« fr » ou « en »). Sans réglage, celle du système (français si le système est
// en français, sinon anglais).
QString langueChoisie();
void    enregistrerLangue(const QString& code);

// À appeler au démarrage, après avoir créé l'application et fixé les noms d'organisation.
void installer(QCoreApplication& application);

} // namespace traduction
