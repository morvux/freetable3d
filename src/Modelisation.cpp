#include "Modelisation.h"

#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <Bnd_Box.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Builder.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>
#include <StlAPI_Writer.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
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
    BRepMesh_IncrementalMesh maillage(forme, deflexion);

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

} // namespace modelisation
