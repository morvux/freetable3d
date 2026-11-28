#include "AncragesMesure.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepExtrema_SupportType.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomAbs_CurveType.hxx>
#include <Geom_Curve.hxx>
#include <Geom_Surface.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Circ.hxx>

namespace ancrage
{

namespace
{

TopTools_IndexedMapOfShape carte(const TopoDS_Shape& forme, TopAbs_ShapeEnum type)
{
    TopTools_IndexedMapOfShape resultat;
    TopExp::MapShapes(forme, type, resultat);
    return resultat;
}

gp_Pnt milieuArete(const TopoDS_Edge& arete)
{
    BRepAdaptor_Curve courbe(arete);
    return courbe.Value(0.5 * (courbe.FirstParameter() + courbe.LastParameter()));
}

// Centre d'une arête circulaire ou d'un arc de cercle.
bool centreCercle(const TopoDS_Edge& arete, gp_Pnt& centre)
{
    BRepAdaptor_Curve courbe(arete);
    if (courbe.GetType() != GeomAbs_Circle)
        return false;
    centre = courbe.Circle().Location();
    return true;
}

gp_Pnt centreFace(const TopoDS_Face& face)
{
    GProp_GProps proprietes;
    BRepGProp::SurfaceProperties(face, proprietes);
    return proprietes.CentreOfMass();
}

Ancrage creer(Genre genre, int idPiece, int indice, const gp_Pnt& position)
{
    Ancrage a;
    a.genre    = genre;
    a.idPiece  = idPiece;
    a.indice   = indice;
    a.position = position;
    return a;
}

// Le mouvement est « rigide » si la pièce a la même structure et les mêmes distances entre ses
// sommets : on peut alors se fier aux numéros d'éléments.
bool rigide(const TopoDS_Shape& ancienne, const TopoDS_Shape& nouvelle)
{
    const TopTools_IndexedMapOfShape va = carte(ancienne, TopAbs_VERTEX);
    const TopTools_IndexedMapOfShape vn = carte(nouvelle, TopAbs_VERTEX);
    if (va.Extent() != vn.Extent() || va.Extent() == 0)
        return false;
    if (carte(ancienne, TopAbs_EDGE).Extent() != carte(nouvelle, TopAbs_EDGE).Extent())
        return false;
    if (carte(ancienne, TopAbs_FACE).Extent() != carte(nouvelle, TopAbs_FACE).Extent())
        return false;

    const int n = std::min(va.Extent(), 12);
    for (int i = 1; i <= n; ++i)
    {
        const gp_Pnt ai = BRep_Tool::Pnt(TopoDS::Vertex(va(i)));
        const gp_Pnt ni = BRep_Tool::Pnt(TopoDS::Vertex(vn(i)));
        for (int j = i + 1; j <= n; ++j)
        {
            const double da = ai.Distance(BRep_Tool::Pnt(TopoDS::Vertex(va(j))));
            const double dn = ni.Distance(BRep_Tool::Pnt(TopoDS::Vertex(vn(j))));
            if (std::abs(da - dn) > 1e-6 * (1.0 + da))
                return false;
        }
    }
    return true;
}

// Paramètres d'un point sur une face (projection sur sa surface).
bool parametresSurFace(const TopoDS_Face& face, const gp_Pnt& p, double& u, double& v)
{
    const Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
    if (surface.IsNull())
        return false;
    GeomAPI_ProjectPointOnSurf projection(p, surface);
    if (projection.NbPoints() < 1)
        return false;
    projection.LowerDistanceParameters(u, v);
    return true;
}

bool parametreSurArete(const TopoDS_Edge& arete, const gp_Pnt& p, double& t)
{
    double premier = 0.0, dernier = 0.0;
    const Handle(Geom_Curve) courbe = BRep_Tool::Curve(arete, premier, dernier);
    if (courbe.IsNull())
        return false;
    GeomAPI_ProjectPointOnCurve projection(p, courbe, premier, dernier);
    if (projection.NbPoints() < 1)
        return false;
    t = projection.LowerDistanceParameter();
    return true;
}

// Élément le plus proche de « cible » parmi ceux du genre demandé.
bool plusProche(Ancrage& a, const TopoDS_Shape& piece, const gp_Pnt& cible)
{
    double meilleur = 1e100;
    int    indice   = 0;
    gp_Pnt trouve;

    auto essayer = [&](int i, const gp_Pnt& p) {
        const double d = p.SquareDistance(cible);
        if (d < meilleur)
        {
            meilleur = d;
            indice   = i;
            trouve   = p;
        }
    };

    switch (a.genre)
    {
    case Genre::Sommet:
    {
        const TopTools_IndexedMapOfShape m = carte(piece, TopAbs_VERTEX);
        for (int i = 1; i <= m.Extent(); ++i)
            essayer(i, BRep_Tool::Pnt(TopoDS::Vertex(m(i))));
        break;
    }
    case Genre::MilieuArete:
    {
        const TopTools_IndexedMapOfShape m = carte(piece, TopAbs_EDGE);
        for (int i = 1; i <= m.Extent(); ++i)
            essayer(i, milieuArete(TopoDS::Edge(m(i))));
        break;
    }
    case Genre::CentreCercle:
    {
        const TopTools_IndexedMapOfShape m = carte(piece, TopAbs_EDGE);
        for (int i = 1; i <= m.Extent(); ++i)
        {
            gp_Pnt centre;
            if (centreCercle(TopoDS::Edge(m(i)), centre))
                essayer(i, centre);
        }
        break;
    }
    case Genre::CentreFace:
    {
        const TopTools_IndexedMapOfShape m = carte(piece, TopAbs_FACE);
        for (int i = 1; i <= m.Extent(); ++i)
            essayer(i, centreFace(TopoDS::Face(m(i))));
        break;
    }
    case Genre::SurFace:
    case Genre::SurArete:
    {
        BRepExtrema_DistShapeShape distance(BRepBuilderAPI_MakeVertex(cible).Vertex(), piece);
        if (!distance.IsDone() || distance.NbSolution() < 1)
            return false;
        const gp_Pnt p = distance.PointOnShape2(1);
        if (distance.SupportTypeShape2(1) == BRepExtrema_IsInFace)
        {
            const TopoDS_Face face = TopoDS::Face(distance.SupportOnShape2(1));
            a.genre  = Genre::SurFace;
            a.indice = carte(piece, TopAbs_FACE).FindIndex(face);
            distance.ParOnFaceS2(1, a.u, a.v);
        }
        else if (distance.SupportTypeShape2(1) == BRepExtrema_IsOnEdge)
        {
            const TopoDS_Edge arete = TopoDS::Edge(distance.SupportOnShape2(1));
            a.genre  = Genre::SurArete;
            a.indice = carte(piece, TopAbs_EDGE).FindIndex(arete);
            distance.ParOnEdgeS2(1, a.u);
        }
        else
        {
            // Tombé sur un sommet : on s'y attache
            a.genre  = Genre::Sommet;
            a.indice = carte(piece, TopAbs_VERTEX).FindIndex(distance.SupportOnShape2(1));
        }
        a.position = p;
        return a.indice > 0;
    }
    default:
        return false;
    }

    if (indice == 0)
        return false;
    a.indice   = indice;
    a.position = trouve;
    return true;
}


using Prediction = std::function<gp_Pnt(const gp_Pnt&)>;

struct Boite
{
    bool   ok = false;
    double mn[3] = {0.0, 0.0, 0.0};
    double mx[3] = {0.0, 0.0, 0.0};
};

Boite boiteDe(const TopoDS_Shape& forme)
{
    Boite b;
    try
    {
        Bnd_Box boite;
        BRepBndLib::AddOptimal(forme, boite, false, false);
        if (boite.IsVoid())
            return b;
        boite.Get(b.mn[0], b.mn[1], b.mn[2], b.mx[0], b.mx[1], b.mx[2]);
        b.ok = true;
    }
    catch (const Standard_Failure&)
    {
    }
    return b;
}

// Hypothèse « faces extrêmes » : un point posé sur une face de la boîte englobante la suit quand
// la boîte change ; les autres points ne bougent pas (cas d'une face étirée ou déplacée).
Prediction predictionFacesExtremes(const Boite& a, const Boite& n)
{
    return [a, n](const gp_Pnt& p) {
        double c[3] = {p.X(), p.Y(), p.Z()};
        for (int i = 0; i < 3; ++i)
        {
            const double eps = 1e-4 * (a.mx[i] - a.mn[i]) + 1e-6;
            if (std::abs(c[i] - a.mx[i]) <= eps)
                c[i] = n.mx[i];
            else if (std::abs(c[i] - a.mn[i]) <= eps)
                c[i] = n.mn[i];
        }
        return gp_Pnt(c[0], c[1], c[2]);
    };
}

// Hypothèse « mise à l'échelle » : chaque point garde sa position relative dans la boîte.
Prediction predictionProportionnelle(const Boite& a, const Boite& n)
{
    return [a, n](const gp_Pnt& p) {
        double c[3] = {p.X(), p.Y(), p.Z()};
        for (int i = 0; i < 3; ++i)
        {
            const double ta = a.mx[i] - a.mn[i];
            const double tn = n.mx[i] - n.mn[i];
            c[i] = ta > 1e-9 ? n.mn[i] + (c[i] - a.mn[i]) / ta * tn : c[i] + (n.mn[i] - a.mn[i]);
        }
        return gp_Pnt(c[0], c[1], c[2]);
    };
}

std::vector<gp_Pnt> sommetsDe(const TopoDS_Shape& forme, std::size_t maximum = 400)
{
    std::vector<gp_Pnt>              points;
    const TopTools_IndexedMapOfShape m = carte(forme, TopAbs_VERTEX);
    for (int i = 1; i <= m.Extent() && points.size() < maximum; ++i)
        points.push_back(BRep_Tool::Pnt(TopoDS::Vertex(m(i))));
    return points;
}

// Distance moyenne de chaque point de « a » au point le plus proche de « b ».
double ecartMoyen(const std::vector<gp_Pnt>& a, const std::vector<gp_Pnt>& b)
{
    if (a.empty() || b.empty())
        return 1e100;
    double total = 0.0;
    for (const gp_Pnt& p : a)
    {
        double plus = 1e100;
        for (const gp_Pnt& q : b)
            plus = std::min(plus, p.SquareDistance(q));
        total += std::sqrt(plus);
    }
    return total / static_cast<double>(a.size());
}

std::vector<gp_Pnt> transformes(const std::vector<gp_Pnt>& points, const Prediction& prediction)
{
    std::vector<gp_Pnt> resultat;
    resultat.reserve(points.size());
    for (const gp_Pnt& p : points)
        resultat.push_back(prediction(p));
    return resultat;
}

// Quand on ne sait pas comment la pièce a été modifiée, on essaie trois hypothèses (rien ne bouge,
// faces extrêmes, mise à l'échelle) et on garde celle qui fait le mieux coïncider les sommets.
Prediction meilleureHypothese(const TopoDS_Shape& ancienne, const TopoDS_Shape& nouvelle)
{
    const Boite a = boiteDe(ancienne);
    const Boite n = boiteDe(nouvelle);
    if (!a.ok || !n.ok)
        return nullptr;

    const std::vector<gp_Pnt> va = sommetsDe(ancienne);
    const std::vector<gp_Pnt> vn = sommetsDe(nouvelle);
    if (va.empty() || vn.empty())
        return nullptr;

    const std::vector<Prediction> hypotheses = {
        [](const gp_Pnt& p) { return p; },
        predictionFacesExtremes(a, n),
        predictionProportionnelle(a, n)};

    double     meilleur = 1e100;
    Prediction retenue;
    for (const Prediction& h : hypotheses)
    {
        const std::vector<gp_Pnt> vp = transformes(va, h);
        const double score = ecartMoyen(vp, vn) + ecartMoyen(vn, vp);
        if (score < meilleur - 1e-9)
        {
            meilleur = score;
            retenue  = h;
        }
    }
    return retenue;
}

// Élément (sommet, arête ou face) de la pièce modifiée qui correspond à l'élément « indiceAncien »
// de l'ancienne pièce : on compare les sommets attendus avec ceux de chaque candidat.
int elementCorrespondant(TopAbs_ShapeEnum type, int indiceAncien, const TopoDS_Shape& ancienne,
                         const TopoDS_Shape& nouvelle, const Prediction& prediction, double diagonale)
{
    const TopTools_IndexedMapOfShape anciens = carte(ancienne, type);
    if (indiceAncien < 1 || indiceAncien > anciens.Extent())
        return 0;
    const std::vector<gp_Pnt> attendus = transformes(sommetsDe(anciens(indiceAncien)), prediction);
    if (attendus.empty())
        return 0;

    const TopTools_IndexedMapOfShape nouveaux = carte(nouvelle, type);
    if (nouveaux.Extent() > 5000)
        return 0;

    double meilleur = 1e100;
    int    trouve   = 0;
    for (int i = 1; i <= nouveaux.Extent(); ++i)
    {
        const std::vector<gp_Pnt> voisins = sommetsDe(nouveaux(i));
        if (voisins.empty())
            continue;
        const double score = ecartMoyen(attendus, voisins) + ecartMoyen(voisins, attendus);
        if (score < meilleur)
        {
            meilleur = score;
            trouve   = i;
        }
    }
    if (trouve == 0 || meilleur > 0.25 * diagonale)
        return 0;
    return trouve;
}

// Retrouve l'ancrage dans la pièce modifiée en suivant l'élément auquel il est attaché.
bool recalerParElement(Ancrage& a, const TopoDS_Shape& ancienne, const TopoDS_Shape& nouvelle,
                       const Prediction& prediction)
{
    TopAbs_ShapeEnum type;
    switch (a.genre)
    {
    case Genre::Sommet:
        type = TopAbs_VERTEX;
        break;
    case Genre::MilieuArete:
    case Genre::CentreCercle:
    case Genre::SurArete:
        type = TopAbs_EDGE;
        break;
    case Genre::CentreFace:
    case Genre::SurFace:
        type = TopAbs_FACE;
        break;
    default:
        return false;
    }

    const Boite bn = boiteDe(nouvelle);
    double      diagonale = 1e100;
    if (bn.ok)
    {
        const double dx = bn.mx[0] - bn.mn[0], dy = bn.mx[1] - bn.mn[1], dz = bn.mx[2] - bn.mn[2];
        diagonale = std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    const int indice = elementCorrespondant(type, a.indice, ancienne, nouvelle, prediction, diagonale);
    if (indice == 0)
        return false;

    Ancrage b = a;
    b.indice  = indice;

    if (a.genre == Genre::SurFace)
    {
        const TopTools_IndexedMapOfShape faces = carte(nouvelle, TopAbs_FACE);
        if (!parametresSurFace(TopoDS::Face(faces(indice)), prediction(a.position), b.u, b.v))
            return false;
    }
    else if (a.genre == Genre::SurArete)
    {
        const TopTools_IndexedMapOfShape aretes = carte(nouvelle, TopAbs_EDGE);
        if (!parametreSurArete(TopoDS::Edge(aretes(indice)), prediction(a.position), b.u))
            return false;
    }

    gp_Pnt position;
    if (!positionDepuisIndice(b, nouvelle, position))
        return false;
    b.position = position;
    a          = b;
    return true;
}

} // namespace

gp_Pnt centreDePiece(const TopoDS_Shape& piece)
{
    try
    {
        GProp_GProps proprietes;
        BRepGProp::VolumeProperties(piece, proprietes);
        if (std::abs(proprietes.Mass()) > 1e-9)
            return proprietes.CentreOfMass();

        Bnd_Box boite;
        BRepBndLib::Add(piece, boite, false);
        if (!boite.IsVoid())
        {
            double x0, y0, z0, x1, y1, z1;
            boite.Get(x0, y0, z0, x1, y1, z1);
            return gp_Pnt((x0 + x1) / 2.0, (y0 + y1) / 2.0, (z0 + z1) / 2.0);
        }
    }
    catch (const Standard_Failure&)
    {
    }
    return gp_Pnt(0.0, 0.0, 0.0);
}

bool positionDepuisIndice(const Ancrage& a, const TopoDS_Shape& piece, gp_Pnt& position)
{
    if (piece.IsNull())
        return false;

    try
    {
        switch (a.genre)
        {
        case Genre::Libre:
            position = a.position;
            return true;
        case Genre::CentrePiece:
            position = centreDePiece(piece);
            return true;
        case Genre::Sommet:
        {
            const TopTools_IndexedMapOfShape m = carte(piece, TopAbs_VERTEX);
            if (a.indice < 1 || a.indice > m.Extent())
                return false;
            position = BRep_Tool::Pnt(TopoDS::Vertex(m(a.indice)));
            return true;
        }
        case Genre::MilieuArete:
        case Genre::CentreCercle:
        case Genre::SurArete:
        {
            const TopTools_IndexedMapOfShape m = carte(piece, TopAbs_EDGE);
            if (a.indice < 1 || a.indice > m.Extent())
                return false;
            const TopoDS_Edge arete = TopoDS::Edge(m(a.indice));
            if (a.genre == Genre::SurArete)
            {
                double premier = 0.0, dernier = 0.0;
                const Handle(Geom_Curve) courbe = BRep_Tool::Curve(arete, premier, dernier);
                if (courbe.IsNull())
                    return false;
                position = courbe->Value(a.u);
                return true;
            }
            if (a.genre == Genre::CentreCercle && centreCercle(arete, position))
                return true;
            position = milieuArete(arete);
            return true;
        }
        case Genre::CentreFace:
        case Genre::SurFace:
        {
            const TopTools_IndexedMapOfShape m = carte(piece, TopAbs_FACE);
            if (a.indice < 1 || a.indice > m.Extent())
                return false;
            const TopoDS_Face face = TopoDS::Face(m(a.indice));
            if (a.genre == Genre::CentreFace)
            {
                position = centreFace(face);
                return true;
            }
            const Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
            if (surface.IsNull())
                return false;
            position = surface->Value(a.u, a.v);
            return true;
        }
        }
    }
    catch (const Standard_Failure&)
    {
    }
    return false;
}

std::vector<Ancrage> candidats(const TopoDS_Shape& piece, const TopoDS_Shape& element, int idPiece,
                               const gp_Pnt* centrePiece)
{
    std::vector<Ancrage> resultat;
    if (piece.IsNull() || element.IsNull())
        return resultat;

    try
    {
        const TopTools_IndexedMapOfShape sommets = carte(piece, TopAbs_VERTEX);
        const TopTools_IndexedMapOfShape aretes  = carte(piece, TopAbs_EDGE);
        const TopTools_IndexedMapOfShape faces   = carte(piece, TopAbs_FACE);

        std::set<std::pair<int, int>> deja; // (genre, indice) déjà proposés

        auto ajouter = [&](Genre genre, int indice, const gp_Pnt& p) {
            if (indice < 1)
                return;
            if (!deja.insert({static_cast<int>(genre), indice}).second)
                return;
            resultat.push_back(creer(genre, idPiece, indice, p));
        };
        auto sommet = [&](const TopoDS_Shape& s) {
            ajouter(Genre::Sommet, sommets.FindIndex(s), BRep_Tool::Pnt(TopoDS::Vertex(s)));
        };

        // Sommets d'abord (les plus précis), puis milieux et centres.
        if (element.ShapeType() == TopAbs_VERTEX)
        {
            sommet(element);
        }
        else if (element.ShapeType() == TopAbs_EDGE || element.ShapeType() == TopAbs_FACE)
        {
            for (TopExp_Explorer ex(element, TopAbs_VERTEX); ex.More(); ex.Next())
                sommet(ex.Current());

            for (TopExp_Explorer ex(element, TopAbs_EDGE); ex.More(); ex.Next())
            {
                const TopoDS_Edge arete = TopoDS::Edge(ex.Current());
                const int         i     = aretes.FindIndex(arete);
                gp_Pnt            centre;
                if (centreCercle(arete, centre))
                    ajouter(Genre::CentreCercle, i, centre);
                ajouter(Genre::MilieuArete, i, milieuArete(arete));
            }

            if (element.ShapeType() == TopAbs_FACE)
                ajouter(Genre::CentreFace, faces.FindIndex(element), centreFace(TopoDS::Face(element)));
        }

        // Centre de la pièce : toujours proposé.
        Ancrage c = creer(Genre::CentrePiece, idPiece, 1, centrePiece ? *centrePiece : centreDePiece(piece));
        resultat.push_back(c);
    }
    catch (const Standard_Failure&)
    {
    }
    return resultat;
}

bool surElement(const TopoDS_Shape& piece, const TopoDS_Shape& element, int idPiece,
                const gp_Pnt& point, Ancrage& resultat)
{
    if (piece.IsNull() || element.IsNull())
        return false;

    try
    {
        if (element.ShapeType() == TopAbs_FACE)
        {
            double u = 0.0, v = 0.0;
            if (!parametresSurFace(TopoDS::Face(element), point, u, v))
                return false;
            resultat          = creer(Genre::SurFace, idPiece, carte(piece, TopAbs_FACE).FindIndex(element), point);
            resultat.u        = u;
            resultat.v        = v;
            return resultat.indice > 0;
        }
        if (element.ShapeType() == TopAbs_EDGE)
        {
            double t = 0.0;
            if (!parametreSurArete(TopoDS::Edge(element), point, t))
                return false;
            resultat   = creer(Genre::SurArete, idPiece, carte(piece, TopAbs_EDGE).FindIndex(element), point);
            resultat.u = t;
            return resultat.indice > 0;
        }
    }
    catch (const Standard_Failure&)
    {
    }
    return false;
}

bool recaler(Ancrage& a, const TopoDS_Shape& ancienne, const TopoDS_Shape& nouvelle,
             const std::function<gp_Pnt(const gp_Pnt&)>& prediction)
{
    if (a.genre == Genre::Libre)
        return true;

    auto perdu = [&a]() {
        a.genre   = Genre::Libre;
        a.idPiece = 0;
        return false;
    };

    if (nouvelle.IsNull())
        return perdu();

    try
    {
        // Le centre de la pièce se recalcule toujours directement.
        if (a.genre == Genre::CentrePiece)
        {
            a.position = centreDePiece(nouvelle);
            return true;
        }

        // 1. Déplacement ou rotation de la pièce entière : les numéros d'éléments restent valables.
        if (!ancienne.IsNull() && rigide(ancienne, nouvelle))
        {
            gp_Pnt p;
            if (positionDepuisIndice(a, nouvelle, p))
            {
                a.position = p;
                return true;
            }
        }

        // 2. Sinon (étirement, redimensionnement, nouvelle forme) : on suit l'élément auquel le point
        //    est attaché, d'après ses sommets. Sans indication, on devine le mouvement des sommets.
        if (!ancienne.IsNull())
        {
            const Prediction mouvement = prediction ? prediction : meilleureHypothese(ancienne, nouvelle);
            if (mouvement && recalerParElement(a, ancienne, nouvelle, mouvement))
                return true;
        }

        // 3. Dernier recours : on cherche l'élément du même genre
        //    le plus proche de l'endroit où on l'attend.
        gp_Pnt prevu = prediction ? prediction(a.position) : a.position;

        if (a.genre == Genre::MilieuArete && prediction && !ancienne.IsNull())
        {
            const TopTools_IndexedMapOfShape aretes = carte(ancienne, TopAbs_EDGE);
            if (a.indice >= 1 && a.indice <= aretes.Extent())
            {
                TopoDS_Vertex v1, v2;
                TopExp::Vertices(TopoDS::Edge(aretes(a.indice)), v1, v2);
                if (!v1.IsNull() && !v2.IsNull())
                {
                    const gp_Pnt p1 = prediction(BRep_Tool::Pnt(v1));
                    const gp_Pnt p2 = prediction(BRep_Tool::Pnt(v2));
                    prevu = gp_Pnt((p1.X() + p2.X()) / 2.0, (p1.Y() + p2.Y()) / 2.0, (p1.Z() + p2.Z()) / 2.0);
                }
            }
        }

        if (plusProche(a, nouvelle, prevu))
            return true;
    }
    catch (const Standard_Failure&)
    {
    }
    return perdu();
}

} // namespace ancrage
