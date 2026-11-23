#include "CotesEsquisse.h"

#include <Aspect_TypeOfLine.hxx>
#include <Graphic3d_ArrayOfSegments.hxx>
#include <Graphic3d_AspectLine3d.hxx>
#include <Graphic3d_Group.hxx>
#include <Graphic3d_HorizontalTextAlignment.hxx>
#include <Graphic3d_VerticalTextAlignment.hxx>
#include <Prs3d_Text.hxx>
#include <Prs3d_TextAspect.hxx>
#include <Quantity_Color.hxx>
#include <TCollection_ExtendedString.hxx>

CotesEsquisse::CotesEsquisse(std::vector<Cote> cotes)
    : m_cotes(std::move(cotes))
{
}

void CotesEsquisse::Compute(const Handle(PrsMgr_PresentationManager)&,
                            const Handle(Prs3d_Presentation)& presentation,
                            const Standard_Integer)
{
    if (m_cotes.empty())
        return;

    const Quantity_Color couleur(1.0, 0.85, 0.3, Quantity_TOC_RGB);

    // Traits : attaches + trait de cote
    Handle(Graphic3d_ArrayOfSegments) segments = new Graphic3d_ArrayOfSegments(static_cast<int>(m_cotes.size()) * 6);
    for (const Cote& cote : m_cotes)
    {
        const gp_Pnt a = cote.a.Translated(cote.decalage);
        const gp_Pnt b = cote.b.Translated(cote.decalage);
        if (cote.attaches)
        {
            segments->AddVertex(cote.a);
            segments->AddVertex(a);
            segments->AddVertex(cote.b);
            segments->AddVertex(b);
        }
        segments->AddVertex(a);
        segments->AddVertex(b);
    }
    Handle(Graphic3d_Group) groupeTraits = presentation->NewGroup();
    groupeTraits->SetGroupPrimitivesAspect(new Graphic3d_AspectLine3d(couleur, Aspect_TOL_SOLID, 1.0));
    groupeTraits->AddPrimitiveArray(segments);

    // Valeurs (texte de taille fixe à l'écran)
    Handle(Prs3d_TextAspect) aspect = new Prs3d_TextAspect();
    aspect->SetColor(couleur);
    aspect->SetHeight(13.0);
    aspect->SetHorizontalJustification(Graphic3d_HTA_CENTER);
    aspect->SetVerticalJustification(Graphic3d_VTA_CENTER);

    Handle(Graphic3d_Group) groupeTextes = presentation->NewGroup();
    groupeTextes->SetGroupPrimitivesAspect(aspect->Aspect());
    for (const Cote& cote : m_cotes)
    {
        const gp_Pnt milieu((cote.a.X() + cote.b.X()) / 2.0 + cote.decalage.X() + cote.decalageTexte.X(),
                            (cote.a.Y() + cote.b.Y()) / 2.0 + cote.decalage.Y() + cote.decalageTexte.Y(),
                            (cote.a.Z() + cote.b.Z()) / 2.0 + cote.decalage.Z() + cote.decalageTexte.Z());
        Prs3d_Text::Draw(groupeTextes, aspect, TCollection_ExtendedString(cote.texte.c_str(), true), milieu);
    }
}

void CotesEsquisse::ComputeSelection(const Handle(SelectMgr_Selection)&, const Standard_Integer)
{
}
