// Tests automatiques du noyau de FreeTable3D (géométrie, document, accroches de mesure).
// Aucune dépendance à Qt : ils se compilent et s'exécutent sans interface graphique.
//
//   cmake -B build -DFREETABLE3D_TESTS=ON && cmake --build build -j
//   ./build/test_noyau            (ou : ctest --test-dir build --output-on-failure)
//
// Chaque test affiche OK ou ÉCHEC ; le programme rend un code d'erreur non nul s'il y a un échec.

#include <cmath>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include "../src/AncragesMesure.h"
#include "../src/Document.h"
#include "../src/Modelisation.h"

namespace
{

int g_echecs = 0;

#define VERIF(cond)                                                                          \
    do                                                                                       \
    {                                                                                        \
        if (!(cond))                                                                         \
        {                                                                                    \
            ++g_echecs;                                                                      \
            std::cerr << "    ECHEC ligne " << __LINE__ << " : " << #cond << "\n";           \
        }                                                                                    \
    } while (0)

#define PROCHE(a, b, tol)                                                                    \
    do                                                                                       \
    {                                                                                        \
        const double va_ = (a), vb_ = (b);                                                   \
        if (!(std::abs(va_ - vb_) <= (tol)))                                                 \
        {                                                                                    \
            ++g_echecs;                                                                      \
            std::cerr << "    ECHEC ligne " << __LINE__ << " : " << #a << " = " << va_       \
                      << " au lieu de " << vb_ << " (tolérance " << (tol) << ")\n";          \
        }                                                                                    \
    } while (0)

constexpr double kPi = 3.14159265358979323846;

double volumeDe(const TopoDS_Shape& forme)
{
    return modelisation::mesurer(forme).volume;
}

// Première face plane de la pièce dont la normale sortante pointe dans la direction donnée.
TopoDS_Shape faceDirigee(const TopoDS_Shape& forme, double nx, double ny, double nz)
{
    const gp_Vec voulu(nx, ny, nz);
    for (TopExp_Explorer ex(forme, TopAbs_FACE); ex.More(); ex.Next())
    {
        gp_Pnt point;
        gp_Vec normale;
        if (modelisation::reperFacePlane(ex.Current(), point, normale) && normale.Dot(voulu) > 0.99)
            return ex.Current();
    }
    return TopoDS_Shape();
}

int compter(const TopoDS_Shape& forme, TopAbs_ShapeEnum type)
{
    int n = 0;
    for (TopExp_Explorer ex(forme, type); ex.More(); ex.Next())
        ++n;
    return n;
}

std::string fichierTemporaire(const std::string& nom)
{
    return (std::filesystem::temp_directory_path() / nom).string();
}

// ---------------------------------------------------------------------------

void testPrimitives()
{
    const TopoDS_Shape cube = modelisation::creerCube(20.0, 10.0, 5.0, 0.0, 0.0, 0.0);
    PROCHE(volumeDe(cube), 1000.0, 1e-6);
    const modelisation::Mesures m = modelisation::mesurer(cube);
    VERIF(m.boiteValide);
    PROCHE(m.xmax - m.xmin, 20.0, 1e-5);
    PROCHE(m.ymax - m.ymin, 10.0, 1e-5);
    PROCHE(m.zmax - m.zmin, 5.0, 1e-5);

    const modelisation::Verification v = modelisation::verifier(cube);
    VERIF(v.valide);
    VERIF(v.nbSolides == 1);

    const TopoDS_Shape cylindre = modelisation::creerCylindre(5.0, 10.0, 0.0, 0.0, 0.0);
    PROCHE(volumeDe(cylindre), kPi * 25.0 * 10.0, 0.01);

    const TopoDS_Shape sphere = modelisation::creerSphere(5.0, 0.0, 0.0, 0.0);
    PROCHE(volumeDe(sphere), 4.0 / 3.0 * kPi * 125.0, 0.05);
}

void testTransformations()
{
    const TopoDS_Shape cube = modelisation::creerCube(10.0, 10.0, 10.0, 0.0, 0.0, 5.0);

    const TopoDS_Shape pose = modelisation::poserSurPlateau(cube);
    PROCHE(modelisation::mesurer(pose).zmin, 0.0, 1e-5);
    PROCHE(volumeDe(pose), 1000.0, 1e-6);

    const TopoDS_Shape deplace = modelisation::transformer(cube, 3.0, -2.0, 1.0, 0.0, 0.0, 0.0);
    const modelisation::Mesures a = modelisation::mesurer(cube);
    const modelisation::Mesures b = modelisation::mesurer(deplace);
    PROCHE(b.xmin - a.xmin, 3.0, 1e-5);
    PROCHE(b.ymin - a.ymin, -2.0, 1e-5);
    PROCHE(b.zmin - a.zmin, 1.0, 1e-5);

    // Rotation de 90° autour de Z : les dimensions X et Y s'échangent
    const TopoDS_Shape pave = modelisation::creerCube(20.0, 10.0, 5.0, 0.0, 0.0, 0.0);
    const TopoDS_Shape tourne = modelisation::transformer(pave, 0.0, 0.0, 0.0, 0.0, 0.0, 90.0);
    const modelisation::Mesures t = modelisation::mesurer(tourne);
    PROCHE(t.xmax - t.xmin, 10.0, 1e-6);
    PROCHE(t.ymax - t.ymin, 20.0, 1e-6);
    PROCHE(volumeDe(tourne), 1000.0, 1e-6);
}

void testEtirement()
{
    const TopoDS_Shape cube = modelisation::creerCube(10.0, 10.0, 10.0, 0.0, 0.0, 0.0);
    const TopoDS_Shape haut = faceDirigee(cube, 0, 0, 1);
    VERIF(!haut.IsNull());

    const TopoDS_Shape plus = modelisation::etirerFace(cube, haut, 5.0);
    PROCHE(volumeDe(plus), 1500.0, 1e-3);
    PROCHE(modelisation::mesurer(plus).zmax, 15.0, 1e-6);
    VERIF(compter(plus, TopAbs_FACE) == 6); // reste un pavé

    const TopoDS_Shape moins = modelisation::etirerFace(cube, haut, -4.0);
    PROCHE(volumeDe(moins), 600.0, 1e-3);

    // Distance nulle : forme inchangée
    PROCHE(volumeDe(modelisation::etirerFace(cube, haut, 0.0)), 1000.0, 1e-6);

    // Cas extrêmes : ne doit pas planter
    modelisation::etirerFace(cube, haut, -100.0);
    modelisation::etirerFace(cube, haut, 1e6);
    modelisation::etirerFace(cube, haut, -10.0);
}

void testRedimensionner()
{
    const TopoDS_Shape cube = modelisation::creerCube(10.0, 10.0, 10.0, 1.0, 2.0, 3.0);
    const TopoDS_Shape r = modelisation::redimensionner(cube, 20.0, 5.0, 2.5);
    const modelisation::Mesures m = modelisation::mesurer(r);
    PROCHE(m.xmax - m.xmin, 20.0, 1e-6);
    PROCHE(m.ymax - m.ymin, 5.0, 1e-6);
    PROCHE(m.zmax - m.zmin, 2.5, 1e-6);
    PROCHE(m.xmin, 1.0, 1e-6); // le coin bas reste fixe
    PROCHE(m.ymin, 2.0, 1e-6);
    PROCHE(m.zmin, 3.0, 1e-6);

    // Valeurs minuscules, nulles ou négatives : jamais de plantage (c'était un ancien bug)
    modelisation::redimensionner(cube, 10.0, 10.0, 0.05);
    modelisation::redimensionner(cube, 10.0, 10.0, 2.06);
    modelisation::redimensionner(cube, 0.0, 10.0, 10.0);
    modelisation::redimensionner(cube, -5.0, 10.0, 10.0);

    const TopoDS_Shape cylindre = modelisation::creerCylindre(5.0, 10.0, 0.0, 0.0, 0.0);
    modelisation::redimensionner(cylindre, 10.0, 10.0, 0.05);
    const TopoDS_Shape cyl2 = modelisation::redimensionner(cylindre, 10.0, 10.0, 2.06);
    VERIF(modelisation::verifier(cyl2).valide);

    // Diamètre d'un cylindre
    VERIF(modelisation::diametreModifiable(cylindre));
    const TopoDS_Shape large = modelisation::changerDiametre(cylindre, 20.0);
    PROCHE(volumeDe(large), kPi * 100.0 * 10.0, 0.05);
}

void testPercage()
{
    const TopoDS_Shape cube = modelisation::creerCube(20.0, 20.0, 10.0, 0.0, 0.0, 0.0);
    const TopoDS_Shape haut = faceDirigee(cube, 0, 0, 1);
    VERIF(!haut.IsNull());

    TopoDS_Shape perce;
    std::string  raison;
    VERIF(modelisation::percer(cube, haut, 6.0, 0.0, 0.0, 0.0, perce, &raison));
    PROCHE(volumeDe(perce), 4000.0 - kPi * 9.0 * 10.0, 0.5);
    VERIF(modelisation::verifier(perce).valide);

    // Trou borgne
    TopoDS_Shape borgne;
    VERIF(modelisation::percer(cube, haut, 6.0, 4.0, 0.0, 0.0, borgne, &raison));
    PROCHE(volumeDe(borgne), 4000.0 - kPi * 9.0 * 4.0, 0.5);

    // Trou plus grand que la pièce : refus ou résultat valide, mais pas de plantage
    TopoDS_Shape enorme;
    if (modelisation::percer(cube, haut, 500.0, 0.0, 0.0, 0.0, enorme, &raison))
        VERIF(!enorme.IsNull());
    TopoDS_Shape nul;
    modelisation::percer(cube, haut, 0.0, 0.0, 0.0, 0.0, nul, &raison);
}

void testBooleens()
{
    const TopoDS_Shape a = modelisation::creerCube(10.0, 10.0, 10.0, 0.0, 0.0, 0.0);
    const TopoDS_Shape b = modelisation::creerCube(10.0, 10.0, 10.0, 5.0, 0.0, 0.0);

    TopoDS_Shape r;
    VERIF(modelisation::booleen(a, {b}, modelisation::OperationBooleenne::Union, r));
    PROCHE(volumeDe(r), 1500.0, 1e-3);
    VERIF(modelisation::booleen(a, {b}, modelisation::OperationBooleenne::Intersection, r));
    PROCHE(volumeDe(r), 500.0, 1e-3);
    VERIF(modelisation::booleen(a, {b}, modelisation::OperationBooleenne::Soustraction, r));
    PROCHE(volumeDe(r), 500.0, 1e-3);
}

void testCongesChanfreins()
{
    const TopoDS_Shape cube = modelisation::creerCube(10.0, 10.0, 10.0, 0.0, 0.0, 0.0);
    const TopoDS_Shape haut = faceDirigee(cube, 0, 0, 1);
    const std::vector<TopoDS_Shape> aretes = modelisation::aretesDe({haut});
    VERIF(aretes.size() == 4);

    TopoDS_Shape conge;
    VERIF(modelisation::congeOuChanfrein(cube, aretes, 1.0, false, conge));
    VERIF(volumeDe(conge) < 1000.0);
    VERIF(volumeDe(conge) > 950.0);
    VERIF(modelisation::verifier(conge).valide);

    TopoDS_Shape chanfrein;
    VERIF(modelisation::congeOuChanfrein(cube, aretes, 1.0, true, chanfrein));
    VERIF(volumeDe(chanfrein) < 1000.0);
    VERIF(modelisation::verifier(chanfrein).valide);

    // Rayon impossible : doit refuser proprement
    TopoDS_Shape trop;
    modelisation::congeOuChanfrein(cube, aretes, 50.0, false, trop);
}

void testEvidement()
{
    const TopoDS_Shape cube = modelisation::creerCube(20.0, 20.0, 20.0, 0.0, 0.0, 0.0);
    const TopoDS_Shape haut = faceDirigee(cube, 0, 0, 1);

    TopoDS_Shape coque;
    VERIF(modelisation::evider(cube, {haut}, 2.0, coque));
    PROCHE(volumeDe(coque), 8000.0 - 16.0 * 16.0 * 18.0, 8000.0 * 0.005);
    VERIF(modelisation::verifier(coque).valide);

    // Paroi trop épaisse : refus propre
    TopoDS_Shape trop;
    modelisation::evider(cube, {haut}, 15.0, trop);
    modelisation::evider(cube, {}, 2.0, trop);
}

void testParoisFines()
{
    const TopoDS_Shape cube = modelisation::creerCube(20.0, 20.0, 20.0, 0.0, 0.0, 0.0);
    const modelisation::ParoisFines epais = modelisation::controlerParois(cube, 1.2);
    VERIF(epais.mesure);
    VERIF(epais.nbPointsFins == 0);
    PROCHE(epais.epaisseurMin, 20.0, 0.05);

    const TopoDS_Shape plaque = modelisation::creerCube(20.0, 20.0, 0.8, 0.0, 0.0, 0.0);
    const modelisation::ParoisFines mince = modelisation::controlerParois(plaque, 1.2);
    VERIF(mince.mesure);
    VERIF(mince.nbPointsFins > 0);
    VERIF(!mince.points.empty());
    PROCHE(mince.epaisseurMin, 0.8, 0.02);

    // Coque de 1 mm : repérée ; coque de 2 mm : acceptée
    const TopoDS_Shape boite = modelisation::creerCube(20.0, 20.0, 20.0, 0.0, 0.0, 0.0);
    TopoDS_Shape       fine, solide;
    if (modelisation::evider(boite, {faceDirigee(boite, 0, 0, 1)}, 1.0, fine))
        VERIF(modelisation::controlerParois(fine, 1.2).nbPointsFins > 0);
    if (modelisation::evider(boite, {faceDirigee(boite, 0, 0, 1)}, 2.0, solide))
        VERIF(modelisation::controlerParois(solide, 1.2).nbPointsFins == 0);

    // Formes difficiles : pas de plantage
    modelisation::controlerParois(modelisation::creerSphere(10.0, 0.0, 0.0, 0.0), 1.2);
    modelisation::controlerParois(modelisation::creerCylindre(5.0, 10.0, 0.0, 0.0, 0.0), 1.2);
    modelisation::controlerParois(TopoDS_Shape(), 1.2);
}

void testEchangeFichiers()
{
    const TopoDS_Shape cube = modelisation::creerCube(10.0, 10.0, 10.0, 0.0, 0.0, 0.0);

    const std::string stl = fichierTemporaire("freetable3d_test.stl");
    VERIF(modelisation::exporterSTL(cube, stl));
    VERIF(std::filesystem::exists(stl));
    VERIF(std::filesystem::file_size(stl) > 84); // en-tête STL binaire = 84 octets
    std::filesystem::remove(stl);

    const std::string step = fichierTemporaire("freetable3d_test.step");
    VERIF(modelisation::exporterSTEP({cube}, step));
    TopoDS_Shape relu;
    VERIF(modelisation::importerSTEP(step, relu));
    PROCHE(volumeDe(relu), 1000.0, 1.0);
    std::filesystem::remove(step);

    // Chemin invalide : échec propre
    VERIF(!modelisation::exporterSTL(cube, "/dossier/inexistant/x.stl"));
    TopoDS_Shape rien;
    VERIF(!modelisation::importerSTEP("/dossier/inexistant/x.step", rien));
}

void testDocument()
{
    Document document;
    VERIF(document.vide());
    const int id = document.ajouter(modelisation::creerCube(10.0, 10.0, 10.0, 0.0, 0.0, 0.0), "Cube");
    VERIF(!document.vide());
    VERIF(document.trouver(id) != nullptr);
    VERIF(document.trouver(id + 99) == nullptr);

    document.modifier({id}, [](const TopoDS_Shape& f) {
        return modelisation::transformer(f, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    });
    PROCHE(modelisation::mesurer(document.trouver(id)->forme).xmin, 10.0, 1e-5);
    VERIF(document.peutAnnuler());

    document.annuler();
    PROCHE(modelisation::mesurer(document.trouver(id)->forme).xmin, 0.0, 1e-5);
    VERIF(document.peutRefaire());
    document.refaire();
    PROCHE(modelisation::mesurer(document.trouver(id)->forme).xmin, 10.0, 1e-5);

    // Enregistrement et relecture
    const std::string chemin = fichierTemporaire("freetable3d_test.ft3d");
    VERIF(document.enregistrer(chemin));
    Document relu;
    VERIF(relu.charger(chemin));
    VERIF(relu.pieces().size() == 1);
    PROCHE(volumeDe(relu.pieces().front().forme), 1000.0, 1e-3);
    std::filesystem::remove(chemin);

    // Fichier absent : le document reste inchangé
    VERIF(!relu.charger("/dossier/inexistant/x.ft3d"));
    VERIF(relu.pieces().size() == 1);

    // Suppression, puis annulation
    document.supprimer({id});
    VERIF(document.vide());
    document.annuler();
    VERIF(!document.vide());
}

void testAncragesMesure()
{
    const TopoDS_Shape cube = modelisation::creerCube(10.0, 10.0, 10.0, 0.0, 0.0, 0.0);
    const TopoDS_Shape haut = faceDirigee(cube, 0, 0, 1);

    // Une face de pavé : 4 sommets + 4 milieux d'arêtes + centre de face + centre de pièce
    const std::vector<ancrage::Ancrage> c = ancrage::candidats(cube, haut, 1, nullptr);
    VERIF(c.size() == 10);

    int nbSommets = 0;
    for (const ancrage::Ancrage& a : c)
    {
        if (a.genre == ancrage::Genre::CentrePiece)
        {
            PROCHE(a.position.X(), 5.0, 1e-6);
            PROCHE(a.position.Y(), 5.0, 1e-6);
            PROCHE(a.position.Z(), 5.0, 1e-6);
        }
        if (a.genre == ancrage::Genre::CentreFace)
        {
            PROCHE(a.position.Z(), 10.0, 1e-6);
            PROCHE(a.position.X(), 5.0, 1e-6);
        }
        if (a.genre == ancrage::Genre::Sommet)
        {
            ++nbSommets;
            PROCHE(a.position.Z(), 10.0, 1e-6);
        }
    }
    VERIF(nbSommets == 4);

    // Déplacement de la pièce : l'ancrage suit son sommet
    ancrage::Ancrage sommet;
    for (const ancrage::Ancrage& a : c)
        if (a.genre == ancrage::Genre::Sommet)
        {
            sommet = a;
            break;
        }
    const gp_Pnt avant = sommet.position;
    const TopoDS_Shape deplace = modelisation::transformer(cube, 5.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    VERIF(ancrage::recaler(sommet, cube, deplace));
    PROCHE(sommet.position.X(), avant.X() + 5.0, 1e-6);
    PROCHE(sommet.position.Y(), avant.Y(), 1e-6);
    PROCHE(sommet.position.Z(), avant.Z(), 1e-6);

    // Étirement de la face du haut de +5 : un sommet de cette face monte avec elle
    ancrage::Ancrage haute;
    for (const ancrage::Ancrage& a : c)
        if (a.genre == ancrage::Genre::Sommet)
        {
            haute = a;
            break;
        }
    const TopoDS_Shape etire = modelisation::etirerFace(cube, haut, 5.0);
    const gp_Pnt       pointFace(5.0, 5.0, 10.0);
    const gp_Vec       normale(0.0, 0.0, 1.0);
    const auto         prediction = [&](const gp_Pnt& p) {
        return std::abs(gp_Vec(pointFace, p).Dot(normale)) < 1e-6 ? p.Translated(normale * 5.0) : p;
    };
    const gp_Pnt avantEtirement = haute.position;
    VERIF(ancrage::recaler(haute, cube, etire, prediction));
    PROCHE(haute.position.X(), avantEtirement.X(), 1e-6);
    PROCHE(haute.position.Y(), avantEtirement.Y(), 1e-6);
    PROCHE(haute.position.Z(), 15.0, 1e-6);

    // Le centre de la pièce suit l'étirement
    ancrage::Ancrage centre;
    centre.genre   = ancrage::Genre::CentrePiece;
    centre.idPiece = 1;
    VERIF(ancrage::recaler(centre, cube, etire));
    PROCHE(centre.position.Z(), 7.5, 1e-6);

    // Pièce nulle : l'ancrage devient un point libre, sans plantage
    ancrage::Ancrage perdu = sommet;
    VERIF(!ancrage::recaler(perdu, cube, TopoDS_Shape()));
    VERIF(perdu.genre == ancrage::Genre::Libre);
}

struct Test
{
    const char* nom;
    void (*fonction)();
};

} // namespace

int main()
{
    const Test tests[] = {
        {"primitives (cube, cylindre, sphère)", testPrimitives},
        {"déplacement, rotation, pose sur le plateau", testTransformations},
        {"étirement de face", testEtirement},
        {"redimensionnement numérique", testRedimensionner},
        {"perçage", testPercage},
        {"opérations booléennes", testBooleens},
        {"congés et chanfreins", testCongesChanfreins},
        {"évidement", testEvidement},
        {"contrôle des parois fines", testParoisFines},
        {"export / import STL et STEP", testEchangeFichiers},
        {"document : annuler, rétablir, enregistrer", testDocument},
        {"ancrages de mesure", testAncragesMesure},
    };

    int testsEnEchec = 0;
    for (const Test& t : tests)
    {
        const int avant = g_echecs;
        std::cout << "[ .. ] " << t.nom << std::endl;
        try
        {
            t.fonction();
        }
        catch (const Standard_Failure& e)
        {
            ++g_echecs;
            std::cerr << "    EXCEPTION OpenCascade : " << e.GetMessageString() << "\n";
        }
        catch (const std::exception& e)
        {
            ++g_echecs;
            std::cerr << "    EXCEPTION : " << e.what() << "\n";
        }
        catch (...)
        {
            ++g_echecs;
            std::cerr << "    EXCEPTION inconnue\n";
        }

        if (g_echecs == avant)
        {
            std::cout << "[ OK ] " << t.nom << std::endl;
        }
        else
        {
            ++testsEnEchec;
            std::cout << "[ECHEC] " << t.nom << std::endl;
        }
    }

    std::cout << "\n" << (sizeof(tests) / sizeof(tests[0])) - testsEnEchec << " test(s) réussi(s), "
              << testsEnEchec << " en échec.\n";
    return testsEnEchec == 0 ? 0 : 1;
}
