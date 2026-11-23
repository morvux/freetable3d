#pragma once

// Cotes (dimensions) affichées sur les profils d'une esquisse : trait de cote décalé avec ses
// deux traits d'attache, et la valeur en mm au milieu. Objet d'affichage seulement, non sélectionnable.

#include <string>
#include <vector>

#include <AIS_InteractiveObject.hxx>
#include <Prs3d_Presentation.hxx>
#include <PrsMgr_PresentationManager.hxx>
#include <SelectMgr_Selection.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

struct Cote
{
    gp_Pnt      a;
    gp_Pnt      b;
    gp_Vec      decalage;      // décalage du trait de cote par rapport à [a, b] (mm)
    gp_Vec      decalageTexte; // décalage supplémentaire du texte (mm)
    std::string texte;         // texte UTF-8 déjà formaté, ex. « 40.00 »
    bool        attaches = true; // false : seulement le trait (pour un diamètre)
};

class CotesEsquisse : public AIS_InteractiveObject
{
public:
    DEFINE_STANDARD_RTTI_INLINE(CotesEsquisse, AIS_InteractiveObject)

    explicit CotesEsquisse(std::vector<Cote> cotes);

    void Compute(const Handle(PrsMgr_PresentationManager)& gestionnaire,
                 const Handle(Prs3d_Presentation)& presentation,
                 const Standard_Integer mode) override;

    void ComputeSelection(const Handle(SelectMgr_Selection)& selection,
                          const Standard_Integer mode) override;

private:
    std::vector<Cote> m_cotes;
};
