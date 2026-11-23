#include "Modelisation.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <set>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepOffset_Mode.hxx>
#include <GeomAbs_JoinType.hxx>
#include <TopTools_ListOfShape.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Circ.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepAlgoAPI_Defeaturing.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <Bnd_Box.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <STEPControl_Reader.hxx>
#include <StlAPI_Writer.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Circ.hxx>
#include <gp_Cylinder.hxx>
#include <gp_EulerSequence.hxx>
#include <gp_Sphere.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_GTrsf.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

namespace modelisation
{

TopoDS_Shape creerCube(double longueurX, double largeurY, double hauteurZ,
                       double x, double y, double z)
{
    BRepPrimAPI_MakeBox constructeur(gp_Pnt(x, y, z), longueurX, largeurY, hauteurZ);
    return constructeur.Shape();
}

TopoDS_Shape creerCylindre(double rayon, double hauteur,
                           double x, double y, double z)
{
    // Axe du cylindre : vertical (Z), base centrée en (x, y, z)
    const gp_Ax2 axe(gp_Pnt(x, y, z), gp_Dir(0.0, 0.0, 1.0));
    BRepPrimAPI_MakeCylinder constructeur(axe, rayon, hauteur);
    return constructeur.Shape();
}

TopoDS_Shape creerSphere(double rayon, double x, double y, double z)
{
    BRepPrimAPI_MakeSphere constructeur(gp_Pnt(x, y, z), rayon);
    return constructeur.Shape();
}

gp_Trsf matriceTransformation(const TopoDS_Shape& forme,
                              double dx, double dy, double dz,
                              double rotationX, double rotationY, double rotationZ)
{
    gp_Trsf total; // identité

    Bnd_Box boite;
    BRepBndLib::Add(forme, boite);
    if (boite.IsVoid())
        return total;

    double xmin, ymin, zmin, xmax, ymax, zmax;
    boite.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    const gp_Pnt centre((xmin + xmax) / 2.0, (ymin + ymax) / 2.0, (zmin + zmax) / 2.0);

    constexpr double pi = 3.14159265358979323846;
    auto tourner = [&](const gp_Dir& direction, double degres) {
        if (degres == 0.0)
            return;
        gp_Trsf rotation;
        rotation.SetRotation(gp_Ax1(centre, direction), degres * pi / 180.0);
        total = rotation.Multiplied(total); // la plus récente s'applique en dernier
    };
    tourner(gp_Dir(1.0, 0.0, 0.0), rotationX);
    tourner(gp_Dir(0.0, 1.0, 0.0), rotationY);
    tourner(gp_Dir(0.0, 0.0, 1.0), rotationZ);

    if (dx != 0.0 || dy != 0.0 || dz != 0.0)
    {
        gp_Trsf translation;
        translation.SetTranslation(gp_Vec(dx, dy, dz));
        total = translation.Multiplied(total);
    }
    return total;
}

bool genererParametrique(const Parametrique& parametres, TopoDS_Shape& forme)
{
    const Esquisse& e = parametres.esquisse;
    if (e.profils.empty() || std::abs(parametres.hauteur) < 1e-6)
        return false;

    try
    {
        const gp_Ax3 repere(e.origine, e.normale, e.axeU);
        auto point = [&repere](double u, double v) {
            return repere.Location().Translated(gp_Vec(repere.XDirection()) * u
                                                + gp_Vec(repere.YDirection()) * v);
        };

        std::vector<TopoDS_Shape> faces;
        for (const Profil& profil : e.profils)
        {
            TopoDS_Wire contour;
            const std::vector<double>& v = profil.v;

            if (profil.type == TypeProfil::Rectangle && v.size() >= 4 && v[2] > 1e-6 && v[3] > 1e-6)
            {
                BRepBuilderAPI_MakePolygon polygone(point(v[0], v[1]), point(v[0] + v[2], v[1]),
                                                    point(v[0] + v[2], v[1] + v[3]),
                                                    point(v[0], v[1] + v[3]), true);
                if (polygone.IsDone())
                    contour = polygone.Wire();
            }
            else if (profil.type == TypeProfil::Cercle && v.size() >= 3 && v[2] > 1e-6)
            {
                const gp_Circ cercle(gp_Ax2(point(v[0], v[1]), repere.Direction(), repere.XDirection()), v[2]);
                BRepBuilderAPI_MakeEdge arete(cercle);
                if (arete.IsDone())
                {
                    BRepBuilderAPI_MakeWire fil(arete.Edge());
                    if (fil.IsDone())
                        contour = fil.Wire();
                }
            }
            else if (profil.type == TypeProfil::Polygone && v.size() >= 6)
            {
                BRepBuilderAPI_MakePolygon polygone;
                for (std::size_t i = 0; i + 1 < v.size(); i += 2)
                    polygone.Add(point(v[i], v[i + 1]));
                polygone.Close();
                if (polygone.IsDone())
                    contour = polygone.Wire();
            }

            if (contour.IsNull())
                return false;
            BRepBuilderAPI_MakeFace face(contour, true);
            if (!face.IsDone())
                return false;
            faces.push_back(face.Face());
        }

        return extruderProfils(faces, gp_Vec(e.normale) * parametres.hauteur, forme);
    }
    catch (const Standard_Failure&)
    {
        return false;
    }
}

TopoDS_Shape transformer(const TopoDS_Shape& forme,
                         double dx, double dy, double dz,
                         double rotationX, double rotationY, double rotationZ)
{
    TopoDS_Shape resultat = forme;

    Bnd_Box boite;
    BRepBndLib::Add(forme, boite);
    if (boite.IsVoid())
        return resultat;

    double xmin, ymin, zmin, xmax, ymax, zmax;
    boite.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    const gp_Pnt centre((xmin + xmax) / 2.0, (ymin + ymax) / 2.0, (zmin + zmax) / 2.0);

    // Chaque étape produit une nouvelle copie de la forme.
    auto appliquer = [&resultat](const gp_Trsf& transformation) {
        BRepBuilderAPI_Transform operation(resultat, transformation, true);
        resultat = operation.Shape();
    };

    constexpr double pi = 3.14159265358979323846;
    auto tourner = [&](const gp_Dir& direction, double degres) {
        if (degres == 0.0)
            return;
        gp_Trsf rotation;
        rotation.SetRotation(gp_Ax1(centre, direction), degres * pi / 180.0);
        appliquer(rotation);
    };

    tourner(gp_Dir(1.0, 0.0, 0.0), rotationX);
    tourner(gp_Dir(0.0, 1.0, 0.0), rotationY);
    tourner(gp_Dir(0.0, 0.0, 1.0), rotationZ);

    if (dx != 0.0 || dy != 0.0 || dz != 0.0)
    {
        gp_Trsf translation;
        translation.SetTranslation(gp_Vec(dx, dy, dz));
        appliquer(translation);
    }

    return resultat;
}

bool reperFacePlane(const TopoDS_Shape& forme, gp_Pnt& point, gp_Vec& normaleSortante)
{
    if (forme.IsNull() || forme.ShapeType() != TopAbs_FACE)
        return false;

    const TopoDS_Face face = TopoDS::Face(forme);
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Plane)
        return false;

    // La normale du plan sous-jacent est celle de la face si elle est orientée
    // « FORWARD », et son opposé sinon : c'est ce qui donne le sens sortant.
    gp_Dir direction = surface.Plane().Axis().Direction();
    if (face.Orientation() == TopAbs_REVERSED)
        direction.Reverse();

    GProp_GProps proprietes;
    BRepGProp::SurfaceProperties(face, proprietes);

    point          = proprietes.CentreOfMass();
    normaleSortante = gp_Vec(direction);
    return true;
}

bool estFacePlane(const TopoDS_Shape& forme)
{
    gp_Pnt point;
    gp_Vec normale;
    return reperFacePlane(forme, point, normale);
}

TopoDS_Shape etirerFace(const TopoDS_Shape& forme, const TopoDS_Shape& face, double distance)
{
    if (distance == 0.0)
        return forme;

    gp_Pnt point;
    gp_Vec normale;
    if (!reperFacePlane(face, point, normale))
        return forme;

    // On balaie la face sur la distance voulue pour obtenir un volume (prisme)...
    BRepPrimAPI_MakePrism prisme(face, normale.Multiplied(distance));
    if (!prisme.IsDone())
        return forme;

    // ... puis on l'ajoute à la pièce (étirement) ou on le retire (rentrée).
    TopoDS_Shape resultat;
    if (distance > 0.0)
    {
        BRepAlgoAPI_Fuse operation(forme, prisme.Shape());
        if (!operation.IsDone())
            return forme;
        resultat = operation.Shape();
    }
    else
    {
        BRepAlgoAPI_Cut operation(forme, prisme.Shape());
        if (!operation.IsDone())
            return forme;
        resultat = operation.Shape();
    }
    if (resultat.IsNull())
        return forme;

    // Fusionne les faces coplanaires créées par l'opération : un cube étiré
    // reste un pavé à six faces.
    ShapeUpgrade_UnifySameDomain nettoyage(resultat, true, true, true);
    nettoyage.Build();
    return nettoyage.Shape();
}

TopoDS_Shape poserSurPlateau(const TopoDS_Shape& forme)
{
    // useTriangulation = false : on mesure la vraie géométrie, pas un éventuel maillage STL
    Bnd_Box boite;
    BRepBndLib::Add(forme, boite, false);
    if (boite.IsVoid())
        return forme;

    double xmin, ymin, zmin, xmax, ymax, zmax;
    boite.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    if (std::abs(zmin) < 1e-9)
        return forme;

    gp_Trsf translation;
    translation.SetTranslation(gp_Vec(0.0, 0.0, -zmin));
    BRepBuilderAPI_Transform operation(forme, translation, true);
    return operation.Shape();
}

namespace
{

// Rotation (axe, angle) qui amène la normale sur la direction « vers le bas » (0, 0, -1).
// Retourne false si la normale pointe déjà vers le bas.
bool rotationVersLeBas(const gp_Vec& normale, gp_Vec& axe, double& angle)
{
    const gp_Vec bas(0.0, 0.0, -1.0);

    const gp_Vec produit = normale.Crossed(bas);
    if (produit.Magnitude() > 1e-9)
    {
        axe   = produit;
        angle = normale.Angle(bas);
        return true;
    }

    if (normale.Dot(bas) < 0.0)
    {
        // Face dirigée vers le haut : demi-tour autour de l'axe X
        axe   = gp_Vec(1.0, 0.0, 0.0);
        angle = 3.14159265358979323846;
        return true;
    }

    return false;
}

} // namespace

TopoDS_Shape poserFaceAPlat(const TopoDS_Shape& forme, const TopoDS_Shape& face)
{
    gp_Pnt point;
    gp_Vec normale;
    if (!reperFacePlane(face, point, normale))
        return forme;

    TopoDS_Shape resultat = forme;

    gp_Vec axe;
    double angle = 0.0;
    if (rotationVersLeBas(normale, axe, angle))
    {
        gp_Trsf rotation;
        rotation.SetRotation(gp_Ax1(point, gp_Dir(axe)), angle);
        BRepBuilderAPI_Transform operation(resultat, rotation, true);
        resultat = operation.Shape();
    }

    return poserSurPlateau(resultat);
}

gp_Quaternion rotationPoserFaceAPlat(const TopoDS_Shape& face)
{
    gp_Pnt point;
    gp_Vec normale;
    if (!reperFacePlane(face, point, normale))
        return gp_Quaternion();

    gp_Vec axe;
    double angle = 0.0;
    if (!rotationVersLeBas(normale, axe, angle))
        return gp_Quaternion();

    return gp_Quaternion(axe, angle);
}

gp_Quaternion quaternionRotation(double rx, double ry, double rz)
{
    constexpr double versRadians = 3.14159265358979323846 / 180.0;
    const gp_Quaternion autourDeX(gp_Vec(1.0, 0.0, 0.0), rx * versRadians);
    const gp_Quaternion autourDeY(gp_Vec(0.0, 1.0, 0.0), ry * versRadians);
    const gp_Quaternion autourDeZ(gp_Vec(0.0, 0.0, 1.0), rz * versRadians);

    // Produit « Z * Y * X » : la rotation autour de X est appliquée en premier.
    return autourDeZ.Multiplied(autourDeY).Multiplied(autourDeX);
}

void anglesEuler(const gp_Quaternion& rotation, double& rx, double& ry, double& rz)
{
    double alpha = 0.0, beta = 0.0, gamma = 0.0;
    rotation.GetEulerAngles(gp_Extrinsic_XYZ, alpha, beta, gamma);

    constexpr double versDegres = 180.0 / 3.14159265358979323846;
    rx = alpha * versDegres;
    ry = beta * versDegres;
    rz = gamma * versDegres;
}

Mesures mesurer(const TopoDS_Shape& forme)
{
    Mesures mesures;
    if (forme.IsNull())
        return mesures;

    mesures.type = forme.ShapeType();

    // useTriangulation = false : on mesure la vraie géométrie, pas un éventuel maillage STL
    Bnd_Box boite;
    BRepBndLib::Add(forme, boite, false);
    if (!boite.IsVoid())
    {
        boite.Get(mesures.xmin, mesures.ymin, mesures.zmin,
                  mesures.xmax, mesures.ymax, mesures.zmax);
        mesures.boiteValide = true;
    }

    GProp_GProps proprietes;
    switch (mesures.type)
    {
    case TopAbs_FACE:
    {
        BRepGProp::SurfaceProperties(forme, proprietes);
        mesures.aire = proprietes.Mass();

        gp_Pnt point;
        gp_Vec normale;
        if (reperFacePlane(forme, point, normale))
        {
            mesures.aDirection = true;
            mesures.direction  = normale;
        }
        else
        {
            BRepAdaptor_Surface surface(TopoDS::Face(forme));
            if (surface.GetType() == GeomAbs_Cylinder)
                mesures.diametres.push_back(2.0 * surface.Cylinder().Radius());
            else if (surface.GetType() == GeomAbs_Sphere)
                mesures.diametres.push_back(2.0 * surface.Sphere().Radius());
        }
        break;
    }

    case TopAbs_EDGE:
    {
        BRepGProp::LinearProperties(forme, proprietes);
        mesures.longueur = proprietes.Mass();

        BRepAdaptor_Curve courbe(TopoDS::Edge(forme));
        if (courbe.GetType() == GeomAbs_Circle)
        {
            mesures.diametres.push_back(2.0 * courbe.Circle().Radius());
        }
        else if (courbe.GetType() == GeomAbs_Line)
        {
            const gp_Pnt debut = courbe.Value(courbe.FirstParameter());
            const gp_Pnt fin   = courbe.Value(courbe.LastParameter());
            const gp_Vec direction(debut, fin);
            if (direction.Magnitude() > 1e-9)
            {
                mesures.aDirection = true;
                mesures.direction  = direction.Normalized();
            }
        }
        break;
    }

    default:
    {
        BRepGProp::VolumeProperties(forme, proprietes);
        mesures.volume = proprietes.Mass();

        GProp_GProps surfaces;
        BRepGProp::SurfaceProperties(forme, surfaces);
        mesures.aire = surfaces.Mass();

        // Diamètres des surfaces cylindriques et sphériques de la pièce
        std::set<long> dejaVus;
        for (TopExp_Explorer faces(forme, TopAbs_FACE); faces.More(); faces.Next())
        {
            BRepAdaptor_Surface surface(TopoDS::Face(faces.Current()));

            double diametre = 0.0;
            if (surface.GetType() == GeomAbs_Cylinder)
                diametre = 2.0 * surface.Cylinder().Radius();
            else if (surface.GetType() == GeomAbs_Sphere)
                diametre = 2.0 * surface.Sphere().Radius();
            else
                continue;

            if (dejaVus.insert(std::lround(diametre * 100.0)).second)
                mesures.diametres.push_back(diametre);
        }
        break;
    }
    }

    return mesures;
}

TopoDS_Shape assembler(const std::vector<TopoDS_Shape>& formes)
{
    if (formes.size() == 1)
        return formes.front();

    BRep_Builder constructeur;
    TopoDS_Compound composite;
    constructeur.MakeCompound(composite);
    for (const TopoDS_Shape& forme : formes)
        constructeur.Add(composite, forme);
    return composite;
}

bool exporterSTL(const TopoDS_Shape& forme, const std::string& chemin,
                 double deflexion)
{
    if (forme.IsNull())
        return false;

    // L'export STL a besoin d'une triangulation de la surface.
    // Déflexion linéaire en mm (écart maximal entre la surface réelle et les triangles)
    // et déflexion angulaire de 0,2 rad (~11°) pour bien arrondir les courbes.
    BRepMesh_IncrementalMesh maillage(forme, deflexion, false, 0.2, true);

    StlAPI_Writer ecrivain;
    return ecrivain.Write(forme, chemin.c_str());
}

bool importerSTEP(const std::string& chemin, TopoDS_Shape& forme)
{
    STEPControl_Reader lecteur;
    if (lecteur.ReadFile(chemin.c_str()) != IFSelect_RetDone)
        return false;
    if (lecteur.TransferRoots() <= 0)
        return false;

    forme = lecteur.OneShape();
    return !forme.IsNull();
}

namespace
{

struct Rond
{
    bool   sphere = false;
    bool   cylindre = false;
    double rayon = 0.0;
    gp_Pnt centre;       // sphère : centre ; cylindre : centre de la base
    gp_Dir axe{0.0, 0.0, 1.0};
    double hauteur = 0.0;
};

// Reconnaît une sphère seule ou un cylindre droit seul.
Rond analyserRond(const TopoDS_Shape& forme)
{
    Rond r;
    int nbFaces = 0;
    int nbPlans = 0;
    bool aCylindre = false;
    bool aSphere = false;
    gp_Cylinder cylindre;
    gp_Sphere sphere;
    std::vector<TopoDS_Shape> plans;

    for (TopExp_Explorer ex(forme, TopAbs_FACE); ex.More(); ex.Next())
    {
        ++nbFaces;
        BRepAdaptor_Surface surface(TopoDS::Face(ex.Current()));
        switch (surface.GetType())
        {
        case GeomAbs_Plane:    ++nbPlans; plans.push_back(ex.Current()); break;
        case GeomAbs_Cylinder: aCylindre = true; cylindre = surface.Cylinder(); break;
        case GeomAbs_Sphere:   aSphere = true; sphere = surface.Sphere(); break;
        default: return r;
        }
    }

    if (nbFaces == 1 && aSphere)
    {
        r.sphere = true;
        r.rayon  = sphere.Radius();
        r.centre = sphere.Location();
    }
    else if (nbFaces == 3 && aCylindre && nbPlans == 2)
    {
        const gp_Ax1 ax = cylindre.Axis();
        double tmin = 1e300, tmax = -1e300;
        for (const TopoDS_Shape& plan : plans)
        {
            gp_Pnt p;
            gp_Vec n;
            if (!reperFacePlane(plan, p, n))
                return r;
            const double t = gp_Vec(ax.Location(), p).Dot(gp_Vec(ax.Direction()));
            tmin = std::min(tmin, t);
            tmax = std::max(tmax, t);
        }
        if (tmax - tmin < 1e-9)
            return r;
        r.cylindre = true;
        r.rayon    = cylindre.Radius();
        r.axe      = ax.Direction();
        r.centre   = ax.Location().Translated(gp_Vec(ax.Direction()) * tmin);
        r.hauteur  = tmax - tmin;
    }
    return r;
}

} // namespace

bool diametreModifiable(const TopoDS_Shape& forme)
{
    const Rond r = analyserRond(forme);
    return r.sphere || r.cylindre;
}

TopoDS_Shape changerDiametre(const TopoDS_Shape& forme, double diametre)
{
    if (diametre <= 1e-6)
        return forme;

    const Rond r = analyserRond(forme);
    const double nouveauRayon = diametre / 2.0;

    if (r.sphere)
    {
        gp_Trsf echelle;
        echelle.SetScale(r.centre, nouveauRayon / r.rayon);
        return BRepBuilderAPI_Transform(forme, echelle, true).Shape();
    }
    if (r.cylindre)
        return BRepPrimAPI_MakeCylinder(gp_Ax2(r.centre, r.axe), nouveauRayon, r.hauteur).Shape();
    return forme;
}

namespace
{
TopoDS_Shape redimensionnerBrut(const TopoDS_Shape& forme, double nx, double ny, double nz)
{
    Bnd_Box boite;
    BRepBndLib::Add(forme, boite, false);
    if (boite.IsVoid() || nx <= 1e-6 || ny <= 1e-6 || nz <= 1e-6)
        return forme;

    double x0, y0, z0, x1, y1, z1;
    boite.Get(x0, y0, z0, x1, y1, z1);
    const double dx = x1 - x0, dy = y1 - y0, dz = z1 - z0;
    if (dx < 1e-9 || dy < 1e-9 || dz < 1e-9)
        return forme;

    // Parallélépipède aligné sur les axes : seules des faces planes et volume = volume de la boîte
    bool planes = true;
    for (TopExp_Explorer ex(forme, TopAbs_FACE); ex.More(); ex.Next())
    {
        if (BRepAdaptor_Surface(TopoDS::Face(ex.Current())).GetType() != GeomAbs_Plane)
            planes = false;
    }
    GProp_GProps proprietes;
    BRepGProp::VolumeProperties(forme, proprietes);
    if (planes && std::abs(proprietes.Mass() - dx * dy * dz) < 1e-6 * dx * dy * dz)
        return BRepPrimAPI_MakeBox(gp_Pnt(x0, y0, z0), nx, ny, nz).Shape();

    // Cylindre vertical (1 face cylindrique + 2 faces planes) : on le reconstruit au lieu de le
    // déformer (une déformation non uniforme le transformerait en surfaces B-spline fragiles).
    {
        int    nbCylindres = 0, nbPlans = 0, nbAutres = 0;
        bool   vertical = true;
        double rayon = 0.0;
        gp_Pnt centre;
        for (TopExp_Explorer ex(forme, TopAbs_FACE); ex.More(); ex.Next())
        {
            BRepAdaptor_Surface surface(TopoDS::Face(ex.Current()));
            if (surface.GetType() == GeomAbs_Cylinder)
            {
                ++nbCylindres;
                const gp_Cylinder cylindre = surface.Cylinder();
                if (!cylindre.Axis().Direction().IsParallel(gp_Dir(0.0, 0.0, 1.0), 1e-9))
                    vertical = false;
                rayon  = cylindre.Radius();
                centre = cylindre.Axis().Location();
            }
            else if (surface.GetType() == GeomAbs_Plane)
                ++nbPlans;
            else
                ++nbAutres;
        }
        if (nbCylindres == 1 && nbPlans == 2 && nbAutres == 0 && vertical && rayon > 1e-9)
        {
            // Le diamètre est celui des champs X/Y qui a été modifié (les champs ont 2 décimales)
            double diametre = 2.0 * rayon;
            if (std::abs(nx - diametre) > 0.006)
                diametre = nx;
            else if (std::abs(ny - diametre) > 0.006)
                diametre = ny;
            const gp_Ax2 axe(gp_Pnt(centre.X(), centre.Y(), z0), gp_Dir(0.0, 0.0, 1.0));
            return BRepPrimAPI_MakeCylinder(axe, diametre / 2.0, nz).Shape();
        }
    }

    // Sinon : étirement différent selon les axes, coin bas fixe
    const double sx = nx / dx, sy = ny / dy, sz = nz / dz;
    if (std::abs(sx - sy) < 1e-9 && std::abs(sy - sz) < 1e-9)
    {
        gp_Trsf echelle;
        echelle.SetScale(gp_Pnt(x0, y0, z0), sx);
        return BRepBuilderAPI_Transform(forme, echelle, true).Shape();
    }

    gp_GTrsf g;
    g.SetValue(1, 1, sx); g.SetValue(2, 2, sy); g.SetValue(3, 3, sz);
    g.SetValue(1, 4, x0 * (1.0 - sx));
    g.SetValue(2, 4, y0 * (1.0 - sy));
    g.SetValue(3, 4, z0 * (1.0 - sz));
    return BRepBuilderAPI_GTransform(forme, g, true).Shape();
}
} // namespace

TopoDS_Shape redimensionner(const TopoDS_Shape& forme, double nx, double ny, double nz)
{
    // Une dimension trop faible peut faire échouer OpenCascade : dans ce cas (ou si le résultat est
    // invalide) on rend la forme d'origine, ce que l'appelant détecte en comparant les formes.
    try
    {
        const TopoDS_Shape resultat = redimensionnerBrut(forme, nx, ny, nz);
        if (resultat.IsNull() || !BRepCheck_Analyzer(resultat).IsValid())
            return forme;
        return resultat;
    }
    catch (const Standard_Failure&)
    {
        return forme;
    }
}

bool booleen(const TopoDS_Shape& base, const std::vector<TopoDS_Shape>& outils,
             OperationBooleenne operation, TopoDS_Shape& resultat)
{
    TopoDS_Shape courant = base;

    for (const TopoDS_Shape& outil : outils)
    {
        TopoDS_Shape suivant;
        switch (operation)
        {
        case OperationBooleenne::Union:
        {
            BRepAlgoAPI_Fuse op(courant, outil);
            if (!op.IsDone()) return false;
            suivant = op.Shape();
            break;
        }
        case OperationBooleenne::Soustraction:
        {
            BRepAlgoAPI_Cut op(courant, outil);
            if (!op.IsDone()) return false;
            suivant = op.Shape();
            break;
        }
        case OperationBooleenne::Intersection:
        {
            BRepAlgoAPI_Common op(courant, outil);
            if (!op.IsDone()) return false;
            suivant = op.Shape();
            break;
        }
        }

        // Un résultat sans aucun solide (pièces qui ne se touchent pas, tout soustrait...) est un échec.
        if (suivant.IsNull() || !TopExp_Explorer(suivant, TopAbs_SOLID).More())
            return false;
        courant = suivant;
    }

    // Fusionne les faces coplanaires issues de l'opération
    ShapeUpgrade_UnifySameDomain nettoyage(courant, true, true, true);
    nettoyage.Build();
    resultat = nettoyage.Shape();
    return !resultat.IsNull();
}

std::vector<TopoDS_Shape> aretesDe(const std::vector<TopoDS_Shape>& elements)
{
    TopTools_IndexedMapOfShape aretes;
    for (const TopoDS_Shape& element : elements)
    {
        if (element.IsNull())
            continue;
        if (element.ShapeType() == TopAbs_EDGE)
            aretes.Add(element);
        else if (element.ShapeType() == TopAbs_FACE)
            TopExp::MapShapes(element, TopAbs_EDGE, aretes);
    }

    std::vector<TopoDS_Shape> resultat;
    for (int i = 1; i <= aretes.Extent(); ++i)
        resultat.push_back(aretes(i));
    return resultat;
}

bool congeOuChanfrein(const TopoDS_Shape& forme, const std::vector<TopoDS_Shape>& aretes,
                      double distance, bool chanfrein, TopoDS_Shape& resultat)
{
    if (aretes.empty() || distance <= 1e-6)
        return false;

    try
    {
        if (chanfrein)
        {
            BRepFilletAPI_MakeChamfer operation(forme);
            for (const TopoDS_Shape& arete : aretes)
                operation.Add(distance, TopoDS::Edge(arete));
            operation.Build();
            if (!operation.IsDone() || operation.Shape().IsNull())
                return false;
            resultat = operation.Shape();
        }
        else
        {
            BRepFilletAPI_MakeFillet operation(forme);
            for (const TopoDS_Shape& arete : aretes)
                operation.Add(distance, TopoDS::Edge(arete));
            operation.Build();
            if (!operation.IsDone() || operation.Shape().IsNull())
                return false;
            resultat = operation.Shape();
        }
    }
    catch (const Standard_Failure&)
    {
        return false;
    }
    return true;
}

bool supprimerFaces(const TopoDS_Shape& forme, const std::vector<TopoDS_Shape>& faces,
                    TopoDS_Shape& resultat)
{
    if (faces.empty())
        return false;

    try
    {
        BRepAlgoAPI_Defeaturing operation;
        operation.SetShape(forme);
        for (const TopoDS_Shape& face : faces)
            operation.AddFaceToRemove(face);
        operation.SetRunParallel(true);
        operation.Build();

        if (!operation.IsDone() || operation.HasErrors() || operation.Shape().IsNull())
            return false;
        if (!TopExp_Explorer(operation.Shape(), TopAbs_SOLID).More())
            return false;
        resultat = operation.Shape();
    }
    catch (const Standard_Failure&)
    {
        return false;
    }
    return true;
}

bool extruderProfils(const std::vector<TopoDS_Shape>& faces, const gp_Vec& vecteur,
                     TopoDS_Shape& resultat)
{
    if (faces.empty() || vecteur.Magnitude() < 1e-6)
        return false;

    try
    {
        TopoDS_Shape courant;
        for (const TopoDS_Shape& face : faces)
        {
            BRepPrimAPI_MakePrism prisme(face, vecteur);
            if (!prisme.IsDone())
                return false;

            if (courant.IsNull())
            {
                courant = prisme.Shape();
            }
            else
            {
                BRepAlgoAPI_Fuse fusion(courant, prisme.Shape());
                if (!fusion.IsDone())
                    return false;
                courant = fusion.Shape();
            }
        }

        if (courant.IsNull() || !TopExp_Explorer(courant, TopAbs_SOLID).More())
            return false;

        ShapeUpgrade_UnifySameDomain nettoyage(courant, true, true, true);
        nettoyage.Build();
        resultat = nettoyage.Shape();
    }
    catch (const Standard_Failure&)
    {
        return false;
    }
    return !resultat.IsNull();
}

bool percer(const TopoDS_Shape& forme, const TopoDS_Shape& face, double diametre,
            double profondeur, double du, double dv, TopoDS_Shape& resultat,
            std::string* raison)
{
    auto echec = [raison](const std::string& texte) {
        if (raison)
            *raison = texte;
        return false;
    };

    if (diametre <= 1e-6)
        return echec("diamètre nul");

    gp_Pnt point;
    gp_Vec normale;
    if (!reperFacePlane(face, point, normale))
        return echec("la face sélectionnée n'est pas plane");

    try
    {
        // Centre de la face
        GProp_GProps proprietes;
        BRepGProp::SurfaceProperties(face, proprietes);
        const gp_Pnt centre = proprietes.CentreOfMass();

        // Mêmes axes que l'esquisse : u = X du monde (ou Z si la face est verticale)
        const gp_Dir n(normale);
        const gp_Dir indice = std::abs(n.Z()) > 0.9 ? gp_Dir(1.0, 0.0, 0.0) : gp_Dir(0.0, 0.0, 1.0);
        const gp_Ax3 repere(centre, n, indice);
        const gp_Pnt axe = centre.Translated(gp_Vec(repere.XDirection()) * du
                                             + gp_Vec(repere.YDirection()) * dv);

        Bnd_Box boite;
        BRepBndLib::Add(forme, boite, false);
        if (boite.IsVoid())
            return echec("pièce vide");
        double x0, y0, z0, x1, y1, z1;
        boite.Get(x0, y0, z0, x1, y1, z1);
        const double diagonale = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)
                                           + (z1 - z0) * (z1 - z0));
        const double longueur = profondeur > 0.0 ? profondeur : diagonale + 1.0;

        GProp_GProps avant;
        BRepGProp::VolumeProperties(forme, avant);
        const double volumeAvant = std::abs(avant.Mass());

        // Essaie de percer dans le sens « -normale » (vers l'intérieur) ; si rien n'est retiré,
        // essaie l'autre sens. Le foret démarre 1 mm hors de la face pour éviter les faces confondues.
        std::string detail = "aucun essai";
        for (int essai = 0; essai < 2; ++essai)
        {
            const double sens = essai == 0 ? 1.0 : -1.0;
            const gp_Dir vers(-n.X() * sens, -n.Y() * sens, -n.Z() * sens);
            const gp_Pnt depart = axe.Translated(gp_Vec(n) * sens * 1.0);

            BRepPrimAPI_MakeCylinder foret(gp_Ax2(depart, vers), diametre / 2.0, longueur + 1.0);
            foret.Build(); // les primitives ne se construisent qu'à l'appel de Build() / Shape()
            if (!foret.IsDone() || foret.Shape().IsNull())
            {
                detail = "création du foret impossible";
                continue;
            }

            BRepAlgoAPI_Cut coupe(forme, foret.Shape());
            if (!coupe.IsDone() || coupe.HasErrors() || coupe.Shape().IsNull())
            {
                detail = "la découpe a échoué (OpenCascade)";
                continue;
            }
            if (!TopExp_Explorer(coupe.Shape(), TopAbs_SOLID).More())
            {
                detail = "le résultat ne contient plus de solide (trou plus grand que la pièce ?)";
                continue;
            }

            GProp_GProps apres;
            BRepGProp::VolumeProperties(coupe.Shape(), apres);
            if (volumeAvant - std::abs(apres.Mass()) < 1e-6)
            {
                detail = "le trou ne rencontre pas la pièce (décalage trop grand ?)";
                continue;
            }

            resultat = coupe.Shape();
            return true;
        }
        return echec(detail);
    }
    catch (const Standard_Failure& e)
    {
        return echec(std::string("erreur OpenCascade : ") + (e.GetMessageString() ? e.GetMessageString() : ""));
    }
}

Verification verifier(const TopoDS_Shape& forme)
{
    Verification v;
    if (forme.IsNull())
        return v;

    try
    {
        v.valide = BRepCheck_Analyzer(forme).IsValid();
    }
    catch (const Standard_Failure&)
    {
        v.valide = false;
    }

    for (TopExp_Explorer ex(forme, TopAbs_SOLID); ex.More(); ex.Next())
        ++v.nbSolides;

    GProp_GProps proprietes;
    BRepGProp::VolumeProperties(forme, proprietes);
    v.volume = proprietes.Mass();

    Bnd_Box boite;
    BRepBndLib::Add(forme, boite, false);
    if (!boite.IsVoid())
    {
        double x0, y0, z0, x1, y1, z1;
        boite.Get(x0, y0, z0, x1, y1, z1);
        v.dx = x1 - x0;
        v.dy = y1 - y0;
        v.dz = z1 - z0;
    }
    return v;
}

bool evider(const TopoDS_Shape& forme, const std::vector<TopoDS_Shape>& facesOuvertes,
            double epaisseur, TopoDS_Shape& resultat)
{
    if (facesOuvertes.empty() || epaisseur <= 1e-6)
        return false;

    try
    {
        TopTools_ListOfShape ouvertes;
        for (const TopoDS_Shape& face : facesOuvertes)
            ouvertes.Append(face);

        // Décalage négatif : la paroi est creusée vers l'intérieur.
        BRepOffsetAPI_MakeThickSolid evidement;
        evidement.MakeThickSolidByJoin(forme, ouvertes, -epaisseur, 1.0e-3, BRepOffset_Skin, false, false,
                                       GeomAbs_Arc);
        if (!evidement.IsDone() || evidement.Shape().IsNull()
            || !TopExp_Explorer(evidement.Shape(), TopAbs_SOLID).More())
            return false;

        resultat = evidement.Shape();
    }
    catch (const Standard_Failure&)
    {
        return false;
    }
    return true;
}

} // namespace modelisation
