#pragma once

// Document : la liste des pièces du projet, avec annuler / rétablir.
// Fait partie du noyau : aucune dépendance à Qt.
//
// L'historique fonctionne par instantanés : avant chaque modification, l'état
// complet est mémorisé. Comme une TopoDS_Shape partage sa géométrie interne
// (copier une forme ne copie pas ses données), cela reste très léger.

#include <functional>
#include <string>
#include <vector>

#include <TopoDS_Shape.hxx>

struct Piece
{
    int          id = 0;
    std::string  nom;
    TopoDS_Shape forme;
};

class Document
{
public:
    // Ajoute une pièce nommée « <nomBase> <id> » et retourne son identifiant.
    int ajouter(const TopoDS_Shape& forme, const std::string& nomBase);

    // Supprime les pièces indiquées (une seule étape d'annulation).
    void supprimer(const std::vector<int>& ids);

    // Remplace la forme des pièces indiquées par le résultat de l'opération
    // (déplacement, arrondi, découpe...). Une seule étape d'annulation.
    void modifier(const std::vector<int>& ids,
                  const std::function<TopoDS_Shape(const TopoDS_Shape&)>& operation);

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
