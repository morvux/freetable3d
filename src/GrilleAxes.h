#pragma once

// Objet 3D affichable : grille millimétrique dans le plan XY (Z = 0) et axes
// X (rouge), Y (vert), Z (bleu) passant par l'origine.
//
//  - lignes mineures : tous les pasMineur mm  (très discrètes)
//  - lignes majeures : tous les pasMajeur mm  (plus visibles)
//  - la grille s'étend de -demiTaille à +demiTaille mm en X et en Y

#include <AIS_InteractiveObject.hxx>
#include <PrsMgr_PresentationManager.hxx>
#include <Prs3d_Presentation.hxx>
#include <SelectMgr_Selection.hxx>

class GrilleAxes : public AIS_InteractiveObject
{
public:
    DEFINE_STANDARD_RTTI_INLINE(GrilleAxes, AIS_InteractiveObject)

    // Valeurs en millimètres (entiers). pasMajeur doit être un multiple de pasMineur.
    GrilleAxes(int demiTaille = 100, int pasMineur = 1, int pasMajeur = 10);

    void Compute(const Handle(PrsMgr_PresentationManager)& gestionnaire,
                 const Handle(Prs3d_Presentation)& presentation,
                 const Standard_Integer mode) override;

    // La grille n'est pas sélectionnable à la souris.
    void ComputeSelection(const Handle(SelectMgr_Selection)& selection,
                          const Standard_Integer mode) override;

private:
    int m_demiTaille;
    int m_pasMineur;
    int m_pasMajeur;
};
