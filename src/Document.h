#pragma once

// Document : la liste des pièces du projet, avec annuler / rétablir.
// Fait partie du noyau : aucune dépendance à Qt.
//
// L'historique fonctionne par instantanés : avant chaque modification, l'état
// complet est mémorisé. Comme une TopoDS_Shape partage sa géométrie interne
// (copier une forme ne copie pas ses données), cela reste très léger.

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <utility>

#include <TopoDS_Shape.hxx>
#include <gp_Quaternion.hxx>
#include <gp_Trsf.hxx>

#include "Esquisse.h"

struct Piece
{
    int           id = 0;
    std::string   nom;
    TopoDS_Shape  forme;
    gp_Quaternion orientation; // rotation cumulée depuis la création (identité au départ)

    // Pièce paramétrique (issue d'une extrusion) : définition + placement actuel.
    // Toute autre modification de la forme (congé, trou...) supprime la définition.
    std::shared_ptr<const Parametrique> parametres;
    gp_Trsf                             placement;
};

class Document
{
public:
    // Ajoute une pièce nommée « <nomBase> <id> » et retourne son identifiant.
    int ajouter(const TopoDS_Shape& forme, const std::string& nomBase,
                std::shared_ptr<const Parametrique> parametres = nullptr);

    // Ajoute plusieurs pièces d'un coup avec leur nom exact (import de fichier), en une seule
    // étape d'annulation. Un nom vide devient « <nomParDefaut> <id> ». Retourne les identifiants.
    std::vector<int> ajouterPieces(const std::vector<std::pair<TopoDS_Shape, std::string>>& pieces,
                                   const std::string& nomParDefaut);

    // Supprime les pièces indiquées (une seule étape d'annulation).
    void supprimer(const std::vector<int>& ids);

    // Remplace la forme des pièces indiquées par le résultat de l'opération
    // (déplacement, arrondi, découpe...). Une seule étape d'annulation.
    void modifier(const std::vector<int>& ids,
                  const std::function<TopoDS_Shape(const TopoDS_Shape&)>& operation);

    // Ajoute une rotation à l'orientation mémorisée des pièces indiquées. À appeler juste
    // après modifier() : c'est la même étape d'annulation, il n'y a pas de nouvel instantané.
    // Déplace / tourne des pièces : « calcul » donne la transformation de chaque pièce.
    // Contrairement à modifier(), la définition paramétrique est conservée (placement mis à jour).
    void transformer(const std::vector<int>& ids,
                     const std::function<gp_Trsf(const TopoDS_Shape&)>& calcul);

    // Nouvelle définition d'une pièce paramétrique : « forme » est la forme déjà placée.
    void redefinirParametres(int id, const TopoDS_Shape& forme,
                             std::shared_ptr<const Parametrique> parametres);

    void composerOrientation(const std::vector<int>& ids, const gp_Quaternion& rotation);

    // Remplace la forme de la pièce « idConservee » et supprime les pièces « idsSupprimees »
    // (une seule étape d'annulation). Sert aux opérations booléennes.
    void remplacer(int idConservee, const TopoDS_Shape& forme, const std::vector<int>& idsSupprimees);

    // Enregistrement / ouverture d'un projet (fichier .ft3d (les anciens .cao restent lisibles) : formes OpenCascade au format BREP).
    // charger() remplace tout le contenu et efface l'historique ; en cas d'échec le document
    // reste inchangé.
    bool enregistrer(const std::string& chemin) const;
    bool charger(const std::string& chemin);

    // Copie les pièces indiquées, une copie par transformation (symétrie, décalage...).
    // Une seule étape d'annulation. Retourne les identifiants des copies.
    // Avec conserverParametres = false (symétrie), les copies perdent leur définition paramétrique.
    std::vector<int> dupliquer(const std::vector<int>& ids,
                               const std::vector<gp_Trsf>& transformations,
                               bool conserverParametres);

    // Colle des pièces mémorisées (copiées depuis ce document ou un état précédent) :
    // chaque modèle est transformé par « t » et reçoit un nouvel identifiant.
    // Une seule étape d'annulation. Retourne les identifiants des nouvelles pièces.
    std::vector<int> coller(const std::vector<Piece>& modeles, const gp_Trsf& t);

    void vider();

    const std::vector<Piece>& pieces() const { return m_pieces; }
    const Piece*              trouver(int id) const;
    bool                      vide() const { return m_pieces.empty(); }

    bool peutAnnuler() const { return !m_pileAnnuler.empty(); }
    bool peutRefaire() const { return !m_pileRefaire.empty(); }
    void annuler();
    void refaire();

private:
    // Mémorise l'état actuel avant une modification.
    void memoriser();

    std::vector<Piece>              m_pieces;
    int                             m_prochainId = 1;
    std::vector<std::vector<Piece>> m_pileAnnuler;
    std::vector<std::vector<Piece>> m_pileRefaire;
};
