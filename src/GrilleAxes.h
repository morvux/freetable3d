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

    // Grille adaptative : quand on zoome, des lignes au 1/10 de mm apparaissent dans
    // une fenêtre carrée centrée sur (cx, cy) de demi-côté « demi » (mm entiers).
    //  - niveau 0 : rien de plus ; 1 : graduations au mm ; 2 : lignes au 0,1 mm + graduations au 0,1 mm
    // Renvoie true si l'état a changé (il faut alors réafficher la grille).
    bool definirDetail(int niveau, int cx, int cy, int demi);
    int niveauDetail() const { return m_niveau; }
    int centreX() const { return m_cx; }
    int centreY() const { return m_cy; }
    int demiFenetre() const { return m_demi; }

private:
    int m_niveau = 0;
    int m_cx = 0;
    int m_cy = 0;
    int m_demi = 0;
    int m_demiTaille;
    int m_pasMineur;
    int m_pasMajeur;
};
