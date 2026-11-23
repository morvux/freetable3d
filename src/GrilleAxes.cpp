#include "GrilleAxes.h"

#include <vector>

#include <Aspect_TypeOfLine.hxx>
#include <Graphic3d_ArrayOfSegments.hxx>
#include <Graphic3d_AspectLine3d.hxx>
#include <Graphic3d_Group.hxx>
#include <Prs3d_Text.hxx>
#include <Prs3d_TextAspect.hxx>
#include <TCollection_ExtendedString.hxx>
#include <Graphic3d_HorizontalTextAlignment.hxx>
#include <Graphic3d_VerticalTextAlignment.hxx>
#include <Quantity_Color.hxx>
#include <gp_Pnt.hxx>

namespace
{

// Lignes parallèles à X (y = p) et à Y (x = p) pour p = ±pas, ±2*pas, ...
// On saute les multiples de pasExclu (0 = on ne saute rien) : ils sont dessinés
// par un autre groupe. L'axe lui-même (p = 0) est toujours exclu.
Handle(Graphic3d_ArrayOfSegments) lignesQuadrillage(int demiTaille, int pas, int pasExclu)
{
    std::vector<int> positions;
    for (int p = pas; p <= demiTaille; p += pas)
    {
        if (pasExclu > 0 && p % pasExclu == 0)
            continue;
        positions.push_back(p);
        positions.push_back(-p);
    }
    if (positions.empty())
        return Handle(Graphic3d_ArrayOfSegments)();

    // 4 sommets par position : un segment parallèle à X et un segment parallèle à Y
    Handle(Graphic3d_ArrayOfSegments) segments =
        new Graphic3d_ArrayOfSegments(static_cast<int>(positions.size()) * 4);

    const double d = demiTaille;
    for (int p : positions)
    {
        segments->AddVertex(gp_Pnt(-d, p, 0.0));
        segments->AddVertex(gp_Pnt(d, p, 0.0));
        segments->AddVertex(gp_Pnt(p, -d, 0.0));
        segments->AddVertex(gp_Pnt(p, d, 0.0));
    }
    return segments;
}

void ajouterGroupe(const Handle(Prs3d_Presentation)& presentation,
                   const Handle(Graphic3d_ArrayOfSegments)& segments,
                   const Quantity_Color& couleur, double largeur)
{
    if (segments.IsNull())
        return;

    Handle(Graphic3d_Group) groupe = presentation->NewGroup();
    groupe->SetGroupPrimitivesAspect(
        new Graphic3d_AspectLine3d(couleur, Aspect_TOL_SOLID, largeur));
    groupe->AddPrimitiveArray(segments);
}

// Un axe : un seul segment entre deux points.
void ajouterAxe(const Handle(Prs3d_Presentation)& presentation,
                const gp_Pnt& debut, const gp_Pnt& fin, const Quantity_Color& couleur)
{
    Handle(Graphic3d_ArrayOfSegments) segment = new Graphic3d_ArrayOfSegments(2);
    segment->AddVertex(debut);
    segment->AddVertex(fin);
    ajouterGroupe(presentation, segment, couleur, 2.0);
}

// Graduations : valeur en mm tous les « pas » le long des axes X et Y (texte de taille fixe à l'écran).
void ajouterGraduations(const Handle(Prs3d_Presentation)& presentation, int demiTaille, int pas)
{
    if (pas <= 0)
        return;

    auto etiquettes = [&](const Quantity_Color& couleur, bool surX) {
        Handle(Prs3d_TextAspect) aspect = new Prs3d_TextAspect();
        aspect->SetColor(couleur);
        aspect->SetHeight(12.0);
        aspect->SetHorizontalJustification(Graphic3d_HTA_CENTER);
        aspect->SetVerticalJustification(Graphic3d_VTA_CENTER);

        Handle(Graphic3d_Group) groupe = presentation->NewGroup();
        groupe->SetGroupPrimitivesAspect(aspect->Aspect());
        for (int p = -demiTaille; p <= demiTaille; p += pas)
        {
            if (p == 0)
                continue;
            const TCollection_ExtendedString texte(p);
            // Les nombres sont décalés de l'axe pour ne pas le recouvrir
            const gp_Pnt position = surX ? gp_Pnt(p, -3.5, 0.0) : gp_Pnt(-4.5, p, 0.0);
            Prs3d_Text::Draw(groupe, aspect, texte, position);
        }
    };
    etiquettes(Quantity_Color(0.95, 0.55, 0.55, Quantity_TOC_RGB), true);
    etiquettes(Quantity_Color(0.55, 0.90, 0.55, Quantity_TOC_RGB), false);
}

} // namespace

GrilleAxes::GrilleAxes(int demiTaille, int pasMineur, int pasMajeur)
    : m_demiTaille(demiTaille)
    , m_pasMineur(pasMineur)
    , m_pasMajeur(pasMajeur)
{
}

void GrilleAxes::Compute(const Handle(PrsMgr_PresentationManager)&,
                         const Handle(Prs3d_Presentation)& presentation,
                         const Standard_Integer)
{
    // Lignes mineures (millimètre) : très discrètes pour ne pas gêner la lecture
    ajouterGroupe(presentation,
                  lignesQuadrillage(m_demiTaille, m_pasMineur, m_pasMajeur),
                  Quantity_Color(0.21, 0.24, 0.28, Quantity_TOC_RGB), 1.0);

    // Lignes majeures (tous les 10 mm)
    ajouterGroupe(presentation,
                  lignesQuadrillage(m_demiTaille, m_pasMajeur, 0),
                  Quantity_Color(0.36, 0.40, 0.46, Quantity_TOC_RGB), 1.0);

    // Graduations numérotées (mm) le long des axes X et Y
    ajouterGraduations(presentation, m_demiTaille, m_pasMajeur);

    // Axes : X rouge, Y vert, Z bleu
    const double d = m_demiTaille;
    ajouterAxe(presentation, gp_Pnt(-d, 0, 0), gp_Pnt(d, 0, 0),
               Quantity_Color(0.90, 0.25, 0.25, Quantity_TOC_RGB));
    ajouterAxe(presentation, gp_Pnt(0, -d, 0), gp_Pnt(0, d, 0),
               Quantity_Color(0.30, 0.80, 0.30, Quantity_TOC_RGB));
    ajouterAxe(presentation, gp_Pnt(0, 0, -d), gp_Pnt(0, 0, d),
               Quantity_Color(0.30, 0.50, 1.00, Quantity_TOC_RGB));
}

void GrilleAxes::ComputeSelection(const Handle(SelectMgr_Selection)&, const Standard_Integer)
{
}
