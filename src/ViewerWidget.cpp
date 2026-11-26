#include "ViewerWidget.h"

#include <algorithm>
#include <algorithm>
#include <array>
#include <functional>
#include <cstdio>
#include <string>
#include <cmath>
#include <cstddef>

#include <QCursor>
#include <QEvent>
#include <QInputDialog>
#include <QLocale>
#include <QRegularExpression>
#include <QMouseEvent>
#include <QObject>
#include <QTimer>
#include <QMoveEvent>
#include <QWheelEvent>
#include <QWindow>

#include <AIS_SelectionScheme.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_TypeOfLine.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Graphic3d_Camera.hxx>
#include <Graphic3d_ZLayerId.hxx>
#include <Geom_CartesianPoint.hxx>
#include <SelectMgr_ViewerSelector.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TopoDS_Vertex.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Graphic3d_GraphicDriver.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Aspect_GradientFillMethod.hxx>
#include <Graphic3d_NameOfMaterial.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <Graphic3d_AspectFillArea3d.hxx>
#include <Aspect_PolygonOffsetMode.hxx>
#include <Prs3d_TypeOfHighlight.hxx>
#include <Quantity_Color.hxx>
#include <StdSelect_BRepOwner.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Trsf.hxx>

#include "Modelisation.h"

#ifdef _WIN32
#include <WNT_Window.hxx>
#else
#include <Xw_Window.hxx>
#endif

namespace
{

// Texte décrivant une pièce, une face ou une arête (dimensions en mm).
QString decrire(const TopoDS_Shape& forme)
{
    const QLocale lieu;
    auto nombre = [&lieu](double valeur) { return lieu.toString(valeur, 'f', 2); };
    GProp_GProps proprietes;

    switch (forme.ShapeType())
    {
    case TopAbs_FACE:
    {
        BRepGProp::SurfaceProperties(forme, proprietes);
        QString genre;
        switch (BRepAdaptor_Surface(TopoDS::Face(forme)).GetType())
        {
        case GeomAbs_Plane:    genre = QObject::tr("plane"); break;
        case GeomAbs_Cylinder: genre = QObject::tr("cylindrique"); break;
        case GeomAbs_Sphere:   genre = QObject::tr("sphérique"); break;
        case GeomAbs_Cone:     genre = QObject::tr("conique"); break;
        case GeomAbs_Torus:    genre = QObject::tr("torique"); break;
        default:               genre = QObject::tr("gauche"); break;
        }
        return QObject::tr("Face %1 · aire %2 mm²").arg(genre, nombre(proprietes.Mass()));
    }

    case TopAbs_EDGE:
    {
        BRepGProp::LinearProperties(forme, proprietes);
        QString genre;
        switch (BRepAdaptor_Curve(TopoDS::Edge(forme)).GetType())
        {
        case GeomAbs_Line:   genre = QObject::tr("droite"); break;
        case GeomAbs_Circle: genre = QObject::tr("circulaire"); break;
        default:             genre = QObject::tr("courbe"); break;
        }
        return QObject::tr("Arête %1 · longueur %2 mm").arg(genre, nombre(proprietes.Mass()));
    }

    default:
    {
        Bnd_Box boite;
        BRepBndLib::Add(forme, boite);
        double xmin = 0, ymin = 0, zmin = 0, xmax = 0, ymax = 0, zmax = 0;
        if (!boite.IsVoid())
            boite.Get(xmin, ymin, zmin, xmax, ymax, zmax);

        BRepGProp::VolumeProperties(forme, proprietes);
        return QObject::tr("Pièce · %1 × %2 × %3 mm · volume %4 mm³")
            .arg(nombre(xmax - xmin), nombre(ymax - ymin), nombre(zmax - zmin),
                 nombre(proprietes.Mass()));
    }
    }
}

} // namespace

ViewerWidget::ViewerWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_NativeWindow);
    setBackgroundRole(QPalette::NoRole);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(400, 300);

    // Pilote graphique OpenGL
    Handle(Aspect_DisplayConnection) connexion = new Aspect_DisplayConnection();
    Handle(OpenGl_GraphicDriver) pilote = new OpenGl_GraphicDriver(connexion);

    // Scène (viewer) et contexte de sélection/affichage
    m_viewer = new V3d_Viewer(pilote);
    m_viewer->SetDefaultLights();
    m_viewer->SetLightOn();
    m_contexte = new AIS_InteractiveContext(m_viewer);

    // Couleurs de surbrillance : par défaut elles sont trop proches de la couleur
    // des pièces. Survol = gris foncé, sélection = orange vif, avec des lignes
    // épaisses pour bien voir les arêtes.
    // Le décalage de profondeur (polygon offset) évite que la surbrillance et la face
    // d'origine, situées au même endroit, se disputent les pixels (scintillement,
    // surtout autour des perçages). La surbrillance de survol passe devant celle de
    // sélection, qui passe elle-même devant la pièce.
    auto configurerSurbrillance = [this](Prs3d_TypeOfHighlight type, const Quantity_Color& couleur,
                                         float decalage) {
        const Handle(Prs3d_Drawer)& style = m_contexte->HighlightStyle(type);
        style->SetColor(couleur);
        style->SetDisplayMode(AIS_Shaded);
        style->SetFaceBoundaryDraw(false);
        style->SetLineAspect(new Prs3d_LineAspect(couleur, Aspect_TOL_SOLID, 3.0));
        style->SetWireAspect(new Prs3d_LineAspect(couleur, Aspect_TOL_SOLID, 3.0));

        Handle(Prs3d_ShadingAspect) ombrage = new Prs3d_ShadingAspect();
        ombrage->SetColor(couleur);
        ombrage->Aspect()->SetPolygonOffsets(Aspect_POM_Fill, -decalage, -decalage);
        style->SetShadingAspect(ombrage);
    };
    const Quantity_Color grisSurvol(0.33, 0.35, 0.40, Quantity_TOC_RGB);
    const Quantity_Color orangeSelection(1.00, 0.42, 0.00, Quantity_TOC_RGB);

    // OpenCascade utilise des styles distincts selon que l'élément est un objet
    // entier (Dynamic / Selected) ou une partie d'un objet, face ou arête
    // (LocalDynamic / LocalSelected) : il faut régler les quatre.
    configurerSurbrillance(Prs3d_TypeOfHighlight_Dynamic, grisSurvol, 4.0f);
    configurerSurbrillance(Prs3d_TypeOfHighlight_LocalDynamic, grisSurvol, 4.0f);
    configurerSurbrillance(Prs3d_TypeOfHighlight_Selected, orangeSelection, 2.0f);
    configurerSurbrillance(Prs3d_TypeOfHighlight_LocalSelected, orangeSelection, 2.0f);

    // Vue : créée ici, mais rattachée à la fenêtre native seulement à l'affichage
    // (voir attacherFenetre) : à ce moment le widget est enfin à sa place dans la fenêtre.
    m_vue = m_viewer->CreateView();

    // Grille millimétrique de ±100 mm : lignes fines au mm, lignes marquées tous les 10 mm
    m_grille = new GrilleAxes(100, 1, 10);
    m_contexte->Display(m_grille, 0, -1, false);
}

int ViewerWidget::modeOcct() const
{
    switch (m_mode)
    {
    case ModeSelection::Face:  return AIS_Shape::SelectionMode(TopAbs_FACE);
    case ModeSelection::Arete: return AIS_Shape::SelectionMode(TopAbs_EDGE);
    case ModeSelection::Piece: break;
    }
    return 0; // mode 0 : l'objet entier
}

int ViewerWidget::idPieceDe(const Handle(AIS_InteractiveObject)& objet) const
{
    for (const ObjetAffiche& affiche : m_objets)
    {
        if (affiche.objet.get() == objet.get())
            return affiche.idPiece;
    }
    return 0;
}

void ViewerWidget::afficherDocument(const Document& document)
{
    effacerParoisFines();
    m_centresPieces.clear();
    m_dernierSurvol.Nullify();
    for (const ObjetAffiche& affiche : m_objets)
        m_contexte->Remove(affiche.objet, false);
    m_objets.clear();

    for (const Piece& piece : document.pieces())
    {
        Handle(AIS_Shape) objet = new AIS_Shape(piece.forme);
        objet->SetMaterial(Graphic3d_NameOfMaterial_Plastified); // d'abord la matière,
        objet->SetColor(Quantity_Color(0.62, 0.70, 0.80, Quantity_TOC_RGB)); // puis la couleur

        // Arêtes dessinées en sombre : on distingue les faces et on voit les arêtes à choisir.
        objet->Attributes()->SetFaceBoundaryDraw(true);
        objet->Attributes()->SetFaceBoundaryAspect(
            new Prs3d_LineAspect(Quantity_NOC_BLACK, Aspect_TOL_SOLID, 1.5));

        m_contexte->Display(objet, AIS_Shaded, modeOcct(), false);
        m_objets.push_back({piece.id, objet});
    }
    if (m_mesureActive)
        activerModesMesure();
    actualiserMesure(document); // la mesure suit les pièces déplacées ou modifiées
    m_contexte->UpdateCurrentViewer();
}

void ViewerWidget::afficherPlateau(bool visible, double x, double y, double z)
{
    if (!m_plateau.IsNull())
    {
        m_contexte->Remove(m_plateau, false);
        m_plateau.Nullify();
    }
    if (!m_volumePlateau.IsNull())
    {
        m_contexte->Remove(m_volumePlateau, false);
        m_volumePlateau.Nullify();
    }

    if (visible && x > 0.0 && y > 0.0 && z > 0.0)
    {
        // Plateau : dalle fine sous Z = 0 (évite de se confondre avec la grille), translucide
        m_plateau = new AIS_Shape(BRepPrimAPI_MakeBox(gp_Pnt(-x / 2.0, -y / 2.0, -0.3), x, y, 0.3).Shape());
        m_plateau->SetColor(Quantity_Color(0.25, 0.65, 0.45, Quantity_TOC_RGB));
        m_plateau->SetTransparency(0.6);
        m_contexte->Display(m_plateau, AIS_Shaded, -1, false);

        // Volume d'impression : contour filaire
        m_volumePlateau = new AIS_Shape(BRepPrimAPI_MakeBox(gp_Pnt(-x / 2.0, -y / 2.0, 0.0), x, y, z).Shape());
        m_volumePlateau->SetColor(Quantity_Color(0.35, 0.85, 0.55, Quantity_TOC_RGB));
        m_volumePlateau->SetWidth(1.5);
        m_contexte->Display(m_volumePlateau, AIS_WireFrame, -1, false);
    }
    m_contexte->UpdateCurrentViewer();
}

void ViewerWidget::afficherGrille(bool visible)
{
    if (visible)
        m_contexte->Display(m_grille, 0, -1, true);
    else
        m_contexte->Erase(m_grille, true);
}

void ViewerWidget::definirAimantation(double pasMm)
{
    m_pasAimantation = pasMm;
}

void ViewerWidget::definirOutilRotation(bool actif)
{
    m_outilRotation = actif;
}

void ViewerWidget::definirAxeRotation(AxeRotation axe)
{
    m_axe = axe;
}

void ViewerWidget::definirAimantationAngle(double pasDegres)
{
    m_pasAngle = pasDegres;
}

void ViewerWidget::selectionnerPieces(const std::vector<int>& ids)
{
    m_contexte->ClearSelected(false);
    for (const ObjetAffiche& affiche : m_objets)
    {
        if (std::find(ids.begin(), ids.end(), affiche.idPiece) != ids.end())
            m_contexte->AddOrRemoveSelected(affiche.objet, false);
    }
    m_contexte->UpdateCurrentViewer();
}

void ViewerWidget::recadrer()
{
    if (!m_fenetreAttachee)
        return; // sera fait au premier affichage
    // On cadre sur les pièces seules : la grille, plus grande, ne doit pas
    // empêcher de zoomer sur une petite pièce.
    Bnd_Box boite;
    for (const ObjetAffiche& affiche : m_objets)
        boite.Add(affiche.objet->BoundingBox());

    if (boite.IsVoid())
        boite.Update(-60.0, -60.0, -10.0, 60.0, 60.0, 60.0); // aperçu de la zone de travail

    m_vue->FitAll(boite, 0.1, false);
    m_vue->ZFitAll();
    ajusterGrilleFine();
    m_vue->Redraw();
}

void ViewerWidget::orienterVue(V3d_TypeOfOrientation orientation)
{
    m_vue->SetProj(orientation);
    recadrer();
}

void ViewerWidget::definirModeSelection(ModeSelection mode)
{
    m_mode = mode;
    if (m_mesureActive)
        arreterMesure();

    // Faces : surfaces étendues, une tolérance de 1 pixel évite d'hésiter entre deux faces
    // voisines près d'une arête. Arêtes et pièces gardent la tolérance par défaut (2).
    m_contexte->SetPixelTolerance(mode == ModeSelection::Face ? 1 : 2);

    m_contexte->ClearSelected(false);
    for (const ObjetAffiche& affiche : m_objets)
    {
        m_contexte->Deactivate(affiche.objet);
        m_contexte->Activate(affiche.objet, modeOcct());
    }
    m_contexte->UpdateCurrentViewer();
}

std::vector<ElementSelectionne> ViewerWidget::selection() const
{
    std::vector<ElementSelectionne> resultat;

    for (m_contexte->InitSelected(); m_contexte->MoreSelected(); m_contexte->NextSelected())
    {
        Handle(StdSelect_BRepOwner) proprietaire =
            Handle(StdSelect_BRepOwner)::DownCast(m_contexte->SelectedOwner());
        if (proprietaire.IsNull() || !proprietaire->HasShape())
            continue;

        Handle(AIS_InteractiveObject) objet =
            Handle(AIS_InteractiveObject)::DownCast(proprietaire->Selectable());

        ElementSelectionne element;
        element.idPiece     = idPieceDe(objet);
        element.forme       = proprietaire->Shape();
        element.description = decrire(element.forme);
        resultat.push_back(element);
    }
    return resultat;
}

void ViewerWidget::selectionnerPiece(int idPiece)
{
    m_contexte->ClearSelected(false);
    for (const ObjetAffiche& affiche : m_objets)
    {
        if (affiche.idPiece == idPiece)
        {
            m_contexte->SetSelected(affiche.objet, false);
            break;
        }
    }
    m_contexte->UpdateCurrentViewer();
}

// Grille adaptative : selon le zoom, ajoute les graduations au mm puis les lignes au 0,1 mm,
// seulement dans la zone visible (le nombre de segments reste petit).
void ViewerWidget::ajusterGrilleFine()
{
    if (m_vue.IsNull() || !m_fenetreAttachee || m_grille.IsNull() || !m_contexte->IsDisplayed(m_grille))
        return;

    Standard_Integer largeur = 0, hauteur = 0;
    m_vue->Window()->Size(largeur, hauteur);
    if (largeur <= 0 || hauteur <= 0)
        return;

    int niveau = 0, cx = 0, cy = 0, demi = 0;
    const double mmParPixel = m_vue->Convert(1);
    // 1 mm fait au moins 30 px -> graduations au mm ; au moins 100 px -> lignes au 0,1 mm
    // (donc des lignes au 0,1 mm espacées d'au moins 10 px, bien lisibles).
    // Hystérésis : on change de niveau avec une marge, pour éviter le clignotement
    // quand le zoom reste autour d'un seuil.
    const int niveauActuel = m_grille->niveauDetail();
    const double seuilMm = (niveauActuel >= 1) ? 1.0 / 25.0 : 1.0 / 30.0;
    const double seuilDixiemes = (niveauActuel >= 2) ? 1.0 / 80.0 : 1.0 / 100.0;
    if (mmParPixel > 0.0 && mmParPixel <= seuilMm)
        niveau = (mmParPixel <= seuilDixiemes) ? 2 : 1;

    if (niveau > 0)
    {
        // Les 4 coins de la vue projetés sur le plan Z = 0 : fenêtre exacte même en vue inclinée
        const Standard_Integer coins[4][2] = {{0, 0}, {largeur, 0}, {0, hauteur}, {largeur, hauteur}};
        double xMin = 1e30, xMax = -1e30, yMin = 1e30, yMax = -1e30;
        bool ok = true;
        for (const auto& coin : coins)
        {
            Standard_Real x, y, z, vx, vy, vz;
            m_vue->ConvertWithProj(coin[0], coin[1], x, y, z, vx, vy, vz);
            if (std::abs(vz) < 0.05)
            {
                ok = false;
                break;
            }
            const double t = -z / vz;
            const double px = x + t * vx, py = y + t * vy;
            xMin = std::min(xMin, px);
            xMax = std::max(xMax, px);
            yMin = std::min(yMin, py);
            yMax = std::max(yMax, py);
        }
        if (ok)
        {
            // Fenêtre actuelle encore valable ? (zone visible incluse, et pas démesurée)
            // -> on ne recalcule rien : plus de reconstruction à chaque cran de molette.
            const int ancienCx = m_grille->centreX(), ancienCy = m_grille->centreY();
            const int ancienDemi = m_grille->demiFenetre();
            const double rayonVisible = std::max({xMax - xMin, yMax - yMin}) / 2.0;
            if (niveauActuel >= 1 && ancienDemi > 0 && xMin >= ancienCx - ancienDemi
                && xMax <= ancienCx + ancienDemi && yMin >= ancienCy - ancienDemi
                && yMax <= ancienCy + ancienDemi && ancienDemi <= 4.0 * std::max(rayonVisible, 5.0))
            {
                cx = ancienCx;
                cy = ancienCy;
                demi = ancienDemi;
            }
            else
            {
                cx = static_cast<int>(std::lround((xMin + xMax) / 20.0)) * 10; // pas de 10 mm
                cy = static_cast<int>(std::lround((yMin + yMax) / 20.0)) * 10;
                const double rayon = std::max({std::abs(xMin - cx), std::abs(xMax - cx),
                                               std::abs(yMin - cy), std::abs(yMax - cy)});
                // Tailles de fenêtre en paliers (10, 20, 40, 80 mm) avec 50 % de marge
                demi = 10;
                while (demi < 80 && demi < rayon * 1.5)
                    demi *= 2;
            }
        }
        else
            niveau = 0;
    }

    if (m_grille->definirDetail(niveau, cx, cy, demi))
        m_contexte->Redisplay(m_grille, false, true);
}

// Rattache la vue OpenCascade à la fenêtre native du widget. Appelé à l'affichage, quand la
// fenêtre native existe et se trouve à sa place dans la fenêtre principale, et de nouveau si Qt
// en change (QEvent::WinIdChange).
void ViewerWidget::attacherFenetre()
{
    if (m_vue.IsNull())
        return;

#ifdef _WIN32
    Handle(WNT_Window) fenetre = new WNT_Window(reinterpret_cast<Aspect_Handle>(winId()));
#else
    Handle(Aspect_DisplayConnection) connexion = m_viewer->Driver()->GetDisplayConnection();
    Handle(Xw_Window) fenetre = new Xw_Window(connexion, static_cast<Aspect_Drawable>(winId()));
#endif
    m_vue->SetWindow(fenetre);
    // Pas de Map() explicite : c'est Qt qui affiche la fenêtre native. L'afficher soi-même la
    // rendait indépendante de la fenêtre principale dans certains environnements.

    // Fond en dégradé sombre, lissage des arêtes
    m_vue->SetBackgroundColor(Quantity_Color(0.10, 0.12, 0.15, Quantity_TOC_RGB));
    m_vue->SetBgGradientColors(Quantity_Color(0.20, 0.24, 0.30, Quantity_TOC_RGB),
                               Quantity_Color(0.07, 0.08, 0.10, Quantity_TOC_RGB),
                               Aspect_GFM_VER, false);
    m_vue->ChangeRenderingParams().IsAntialiasingEnabled = true;
#ifdef Q_OS_WIN
    m_vue->ChangeRenderingParams().NbMsaaSamples = 2; // plus léger sur les cartes graphiques intégrées
#else
    m_vue->ChangeRenderingParams().NbMsaaSamples = 4;
#endif
    m_vue->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_WHITE, 0.08, V3d_ZBUFFER);
    m_vue->SetProj(V3d_XposYnegZpos);
    m_vue->MustBeResized();
    m_fenetreAttachee = true;
}

// La zone 3D est une fenêtre native (X11/Windows). Dans certains environnements (machine
// virtuelle, gestionnaire de fenêtres sans composition...), sa position n'est pas toujours mise
// à jour quand la mise en page change : elle déborde alors sur les barres d'outils.
// On la recale donc explicitement sur l'emplacement du widget.
void ViewerWidget::synchroniserFenetreNative()
{
    QWindow* fenetre = windowHandle();
    if (!fenetre || !isVisible())
        return;
    QWidget* parentNatif = nativeParentWidget();
    if (!parentNatif)
        parentNatif = window();
    const QRect voulu(mapTo(parentNatif, QPoint(0, 0)), size());
    if (fenetre->geometry() != voulu)
        fenetre->setGeometry(voulu);
}

void ViewerWidget::moveEvent(QMoveEvent*)
{
    synchroniserFenetreNative();
}

void ViewerWidget::ajusterVue()
{
    if (m_vue.IsNull() || !m_fenetreAttachee)
        return;

    synchroniserFenetreNative();

    m_vue->Window()->DoResize();
    m_vue->MustBeResized();
    m_vue->Invalidate();
    m_vue->Redraw();
}

// ---------------------------------------------------------------------------
// Géométrie sous le curseur
// ---------------------------------------------------------------------------

bool ViewerWidget::rayonSousCurseur(const QPoint& ecran, gp_Pnt& origine, gp_Vec& direction) const
{
    Standard_Real x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0;
    m_vue->ConvertWithProj(ecran.x(), ecran.y(), x, y, z, vx, vy, vz);

    origine   = gp_Pnt(x, y, z);
    direction = gp_Vec(vx, vy, vz);
    if (direction.Magnitude() < 1e-9)
        return false;

    direction.Normalize();
    return true;
}

bool ViewerWidget::pointSurPlanSol(const QPoint& ecran, gp_Pnt& point) const
{
    gp_Pnt origine;
    gp_Vec direction;
    if (!rayonSousCurseur(ecran, origine, direction))
        return false;

    // Plan du sol vu presque de profil : le déplacement n'aurait pas de sens.
    if (std::abs(direction.Z()) < 0.05)
        return false;

    const double t = -origine.Z() / direction.Z();
    point = gp_Pnt(origine.X() + t * direction.X(),
                   origine.Y() + t * direction.Y(),
                   0.0);
    return true;
}

bool ViewerWidget::parametreSurDroite(const QPoint& ecran, const gp_Pnt& point,
                                      const gp_Vec& normale, double& s) const
{
    gp_Pnt origine;
    gp_Vec direction;
    if (!rayonSousCurseur(ecran, origine, direction))
        return false;

    // Points les plus proches de deux droites : P(s) = point + s * normale
    // (normale de longueur 1) et Q(t) = origine + t * direction.
    const gp_Vec w0(origine, point);
    const double b = normale.Dot(direction);
    const double denominateur = 1.0 - b * b;
    if (denominateur < 1e-6)
        return false; // droite alignée avec la ligne de visée

    const double d = normale.Dot(w0);
    const double e = direction.Dot(w0);
    s = (b * e - d) / denominateur;
    return true;
}

double ViewerWidget::aimanter(double valeur) const
{
    if (m_pasAimantation <= 0.0)
        return valeur;
    return std::round(valeur / m_pasAimantation) * m_pasAimantation;
}

// ---------------------------------------------------------------------------
// Gestes : déplacement d'une pièce, étirement d'une face
// ---------------------------------------------------------------------------

bool ViewerWidget::rassemblerSelection(Bnd_Box& boite)
{
    m_idsDeplaces.clear();
    m_objetsDeplaces.clear();
    m_centresDeplaces.clear();

    for (const ElementSelectionne& element : selection())
    {
        if (m_idsDeplaces.contains(element.idPiece))
            continue;

        for (const ObjetAffiche& affiche : m_objets)
        {
            if (affiche.idPiece != element.idPiece)
                continue;

            const Bnd_Box& boiteObjet = affiche.objet->BoundingBox();
            if (boiteObjet.IsVoid())
                continue;

            double xmin = 0, ymin = 0, zmin = 0, xmax = 0, ymax = 0, zmax = 0;
            boiteObjet.Get(xmin, ymin, zmin, xmax, ymax, zmax);

            m_objetsDeplaces.push_back(affiche.objet);
            m_centresDeplaces.push_back(
                gp_Pnt((xmin + xmax) / 2.0, (ymin + ymax) / 2.0, (zmin + zmax) / 2.0));
            boite.Add(boiteObjet);
        }
        m_idsDeplaces.append(element.idPiece);
    }
    return !m_objetsDeplaces.empty() && !boite.IsVoid();
}

bool ViewerWidget::preparerDeplacement()
{
    if (!pointSurPlanSol(m_positionPression, m_pointDepart))
        return false;

    Bnd_Box boite;
    if (!rassemblerSelection(boite))
        return false;

    // Le coin le plus bas-gauche de la boîte englobante sert de repère d'aimantation.
    double zmin = 0, xmax = 0, ymax = 0, zmax = 0;
    boite.Get(m_coinX, m_coinY, zmin, xmax, ymax, zmax);
    m_dx = 0.0;
    m_dy = 0.0;
    return true;
}

bool ViewerWidget::preparerEtirement(const Handle(SelectMgr_EntityOwner)& proprietaire)
{
    Handle(StdSelect_BRepOwner) brep = Handle(StdSelect_BRepOwner)::DownCast(proprietaire);
    if (brep.IsNull() || !brep->HasShape())
        return false;

    gp_Pnt point;
    gp_Vec normale;
    if (!modelisation::reperFacePlane(brep->Shape(), point, normale))
        return false; // seules les faces planes peuvent être étirées

    Handle(AIS_Shape) objet = Handle(AIS_Shape)::DownCast(brep->Selectable());
    if (objet.IsNull())
        return false;

    const int id = idPieceDe(objet);
    if (id == 0)
        return false;

    double s0 = 0.0;
    if (!parametreSurDroite(m_positionPression, point, normale, s0))
        return false;

    m_idEtire       = id;
    m_objetEtire    = objet;
    m_formeInitiale = objet->Shape();
    m_faceEtiree    = brep->Shape();
    m_pointFace     = point;
    m_normaleFace   = normale;
    m_s0            = s0;
    m_distance      = 0.0;
    return true;
}

void ViewerWidget::mettreAJourDeplacement(const QPoint& ecran)
{
    gp_Pnt courant;
    if (!pointSurPlanSol(ecran, courant))
        return;

    // On aimante la position finale du coin de référence, pas le déplacement brut :
    // la pièce se pose ainsi exactement sur la grille.
    const double cibleX = aimanter(m_coinX + courant.X() - m_pointDepart.X());
    const double cibleY = aimanter(m_coinY + courant.Y() - m_pointDepart.Y());
    m_dx = cibleX - m_coinX;
    m_dy = cibleY - m_coinY;

    gp_Trsf translation;
    translation.SetTranslation(gp_Vec(m_dx, m_dy, 0.0));
    for (const Handle(AIS_Shape)& objet : m_objetsDeplaces)
        objet->SetLocalTransformation(translation);
    suivreMesureRigide();
    m_contexte->UpdateCurrentViewer();

    auto n = [](double v) { return QString::number(v == 0.0 ? 0.0 : v, 'f', 2); };
    annoncerGeste(tr("Position : X = %1 mm, Y = %2 mm\nDéplacement : ΔX = %3, ΔY = %4 mm")
                      .arg(n(cibleX), n(cibleY), n(m_dx), n(m_dy)));
}

void ViewerWidget::mettreAJourEtirement(const QPoint& ecran)
{
    double s = 0.0;
    if (!parametreSurDroite(ecran, m_pointFace, m_normaleFace, s))
        return;

    const double distance = aimanter(s - m_s0);
    if (distance == m_distance)
        return; // rien de neuf : on évite de recalculer la pièce

    m_distance = distance;
    m_objetEtire->SetShape(modelisation::etirerFace(m_formeInitiale, m_faceEtiree, distance));
    m_contexte->Redisplay(m_objetEtire, true);
    suivreMesureEtirement(m_objetEtire->Shape(), distance);

    const modelisation::Mesures m = modelisation::mesurer(m_objetEtire->Shape());
    QString dimensions;
    if (m.boiteValide)
        dimensions = tr("\nDimensions : %1 × %2 × %3 mm")
                         .arg(QString::number(m.xmax - m.xmin, 'f', 2),
                              QString::number(m.ymax - m.ymin, 'f', 2),
                              QString::number(m.zmax - m.zmin, 'f', 2));
    // Dimensions de la face étirée : les deux plus grandes étendues de sa boîte englobante
    const modelisation::Mesures face = modelisation::mesurer(m_faceEtiree);
    if (face.boiteValide)
    {
        std::array<double, 3> e = {face.xmax - face.xmin, face.ymax - face.ymin, face.zmax - face.zmin};
        std::sort(e.begin(), e.end(), std::greater<double>());
        dimensions += tr("\nFace : %1 × %2 mm")
                          .arg(QString::number(e[0], 'f', 2), QString::number(e[1], 'f', 2));
    }
    annoncerGeste(tr("Étirement : %1 mm%2").arg(QString::number(distance, 'f', 2), dimensions));
}

bool ViewerWidget::preparerOrientation()
{
    Bnd_Box boite;
    if (!rassemblerSelection(boite))
        return false;

    m_angle = 0.0;
    return true;
}

double ViewerWidget::aimanterAngle(double degres) const
{
    if (m_pasAngle <= 0.0)
        return degres;
    return std::round(degres / m_pasAngle) * m_pasAngle;
}

void ViewerWidget::mettreAJourOrientation(const QPoint& ecran)
{
    // 0,5° par pixel parcouru horizontalement ; vers la droite = angle positif
    // (règle de la main droite autour de l'axe choisi).
    m_angle = aimanterAngle((ecran.x() - m_positionPression.x()) * 0.5);

    QString nomAxe = QStringLiteral("Z");
    gp_Dir direction(0.0, 0.0, 1.0);
    if (m_axe == AxeRotation::X)
    {
        nomAxe = QStringLiteral("X");
        direction = gp_Dir(1.0, 0.0, 0.0);
    }
    else if (m_axe == AxeRotation::Y)
    {
        nomAxe = QStringLiteral("Y");
        direction = gp_Dir(0.0, 1.0, 0.0);
    }

    constexpr double pi = 3.14159265358979323846;
    for (std::size_t i = 0; i < m_objetsDeplaces.size(); ++i)
    {
        // Chaque pièce tourne autour de son propre centre.
        gp_Trsf rotation;
        rotation.SetRotation(gp_Ax1(m_centresDeplaces[i], direction), m_angle * pi / 180.0);
        m_objetsDeplaces[i]->SetLocalTransformation(rotation);
    }
    suivreMesureRigide();
    m_contexte->UpdateCurrentViewer();

    annoncerGeste(tr("Rotation autour de %1 : %2°")
                      .arg(nomAxe, QString::number(m_angle, 'f', 1)));
}

void ViewerWidget::annoncerGeste(const QString& texte)
{
    QString pourBarre = texte;
    pourBarre.replace('\n', QStringLiteral("   |   "));
    emit messageApercu(pourBarre);
    emit apercuGeste(texte);
}

void ViewerWidget::terminerGeste()
{
    emit apercuGeste(QString()); // masque l'affichage près du curseur
    const Geste geste = m_geste;
    m_geste = Geste::Aucun;
    m_gesteDemarre = false;

    if (geste == Geste::Deplacement)
    {
        for (const Handle(AIS_Shape)& objet : m_objetsDeplaces)
            objet->ResetTransformation();
        m_contexte->UpdateCurrentViewer();
        m_objetsDeplaces.clear();

        if (m_dx != 0.0 || m_dy != 0.0)
            emit deplacementTermine(m_idsDeplaces, m_dx, m_dy);
    }
    else if (geste == Geste::Orientation)
    {
        for (const Handle(AIS_Shape)& objet : m_objetsDeplaces)
            objet->ResetTransformation();
        m_contexte->UpdateCurrentViewer();
        m_objetsDeplaces.clear();

        if (m_angle != 0.0)
        {
            const double rx = (m_axe == AxeRotation::X) ? m_angle : 0.0;
            const double ry = (m_axe == AxeRotation::Y) ? m_angle : 0.0;
            const double rz = (m_axe == AxeRotation::Z) ? m_angle : 0.0;
            emit rotationTerminee(m_idsDeplaces, rx, ry, rz);
        }
    }
    else if (geste == Geste::Etirement)
    {
        const Handle(AIS_Shape) objet = m_objetEtire;
        m_objetEtire.Nullify();

        if (m_distance != 0.0)
        {
            emit etirementTermine(m_idEtire, m_faceEtiree, m_distance);
        }
        else
        {
            objet->SetShape(m_formeInitiale);
            m_contexte->Redisplay(objet, true);
        }
    }
}

// ---------------------------------------------------------------------------
// Événements Qt
// ---------------------------------------------------------------------------

void ViewerWidget::afficherBulle(const QString& texte)
{
    if (!m_bulle)
    {
        // Fenêtre indépendante (la vue OpenGL ne peut pas porter de widget enfant).
        m_bulle = new QLabel(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowTransparentForInput
                                          | Qt::WindowDoesNotAcceptFocus);
        m_bulle->setAttribute(Qt::WA_ShowWithoutActivating);
        m_bulle->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_bulle->setTextFormat(Qt::PlainText);
        m_bulle->setStyleSheet(QStringLiteral(
            "QLabel { background-color: rgba(30, 36, 48, 225); color: #ffffff; border: 1px solid #f2c200;"
            " border-radius: 4px; padding: 3px 7px; }"));
    }
    m_bulle->setText(texte);
    m_bulle->adjustSize();
    m_bulle->move(QCursor::pos() + QPoint(18, 22));
    if (!m_bulle->isVisible())
        m_bulle->show();
}

void ViewerWidget::masquerBulle()
{
    if (m_bulle)
        m_bulle->hide();
}

void ViewerWidget::leaveEvent(QEvent*)
{
    if (m_minuterieSurvol)
        m_minuterieSurvol->stop();
    masquerBulle();
    m_dernierSurvol.Nullify();
}

void ViewerWidget::paintEvent(QPaintEvent*)
{
    if (!m_fenetreAttachee)
        return;
    synchroniserFenetreNative();
    ajusterGrilleFine(); // la vue a pu changer autrement qu'à la souris (cadrage, saisie de valeur...)
    m_vue->Redraw();
}

QPoint ViewerWidget::versPixels(const QPointF& logique) const
{
    // OpenCascade travaille en pixels réels de la fenêtre ; Qt donne des coordonnées
    // logiques. Les deux diffèrent dès que l'affichage Windows est à 125 %, 150 %...
    const qreal f = devicePixelRatioF();
    return QPoint(qRound(logique.x() * f), qRound(logique.y() * f));
}

void ViewerWidget::resizeEvent(QResizeEvent*)
{
    ajusterVue();
}

void ViewerWidget::showEvent(QShowEvent*)
{
    if (!m_fenetreAttachee)
        attacherFenetre();
    ajusterVue();
    // Une fois la mise en page terminée, on resynchronise une dernière fois
    // (et on cadre la vue au tout premier affichage).
    QTimer::singleShot(0, this, [this]() {
        ajusterVue();
        // Seconde passe un peu plus tard : certains gestionnaires de fenêtres finissent la mise en page après
        QTimer::singleShot(300, this, [this]() { ajusterVue(); });
        if (m_premierAffichage)
        {
            m_premierAffichage = false;
            recadrer();
        }
    });
}

void ViewerWidget::mousePressEvent(QMouseEvent* evenement)
{
    m_ignorerRelache = false;
    if (m_minuterieSurvol)
        m_minuterieSurvol->stop();
    m_dernierSurvol.Nullify();
    m_dernierePosition = versPixels(evenement->position());
    m_positionPression = m_dernierePosition;
    m_gesteDemarre = false;
    m_geste = Geste::Aucun;

    if (evenement->button() != Qt::LeftButton)
        return;

    m_geste = Geste::Rotation; // par défaut : on fait tourner la vue

    // Appui sur un élément DÉJÀ sélectionné : on le déplace (pièce) ou on l'étire (face plane).
    if (!m_esquisseActive && !evenement->modifiers().testFlag(Qt::ShiftModifier))
    {
        m_contexte->MoveTo(m_dernierePosition.x(), m_dernierePosition.y(), m_vue, false);
        if (m_contexte->HasDetected())
        {
            const Handle(SelectMgr_EntityOwner) proprietaire = m_contexte->DetectedOwner();
            if (!proprietaire.IsNull() && m_contexte->IsSelected(proprietaire))
            {
                if (m_mode == ModeSelection::Piece && m_outilRotation && preparerOrientation())
                    m_geste = Geste::Orientation;
                else if (m_mode == ModeSelection::Piece && !m_outilRotation && preparerDeplacement())
                    m_geste = Geste::Deplacement;
                else if (m_mode == ModeSelection::Face && preparerEtirement(proprietaire))
                    m_geste = Geste::Etirement;
            }
        }
    }

    if (m_geste == Geste::Rotation)
        m_vue->StartRotation(m_dernierePosition.x(), m_dernierePosition.y());
}

void ViewerWidget::mouseMoveEvent(QMouseEvent* evenement)
{
    const QPoint position = versPixels(evenement->position());

    if (evenement->buttons() & Qt::LeftButton)
    {
        if (m_geste == Geste::Rotation)
        {
            m_vue->Rotation(position.x(), position.y());
            ajusterGrilleFine();
        }
        else if (m_geste == Geste::Deplacement || m_geste == Geste::Orientation
                 || m_geste == Geste::Etirement)
        {
            if (!m_gesteDemarre)
            {
                // On attend un vrai glissement : un simple clic reste une sélection.
                if ((position - m_positionPression).manhattanLength() <= 4)
                    return;
                m_gesteDemarre = true;
                memoriserPositionsMesure();

                if (m_geste == Geste::Etirement)
                {
                    // Pendant l'étirement, la face n'a plus à rester en surbrillance.
                    m_contexte->ClearSelected(true);
                    emit selectionChangee();
                }
            }

            if (m_geste == Geste::Deplacement)
                mettreAJourDeplacement(position);
            else if (m_geste == Geste::Orientation)
                mettreAJourOrientation(position);
            else
                mettreAJourEtirement(position);
        }
    }
    else if (evenement->buttons() & (Qt::RightButton | Qt::MiddleButton))
    {
        m_vue->Pan(position.x() - m_dernierePosition.x(),
                   m_dernierePosition.y() - position.y());
        m_dernierePosition = position;
        ajusterGrilleFine();
    }
    else
    {
        if (m_esquisseActive)
        {
            // Esquisse : aperçu du profil en cours de tracé
            gp_Pnt2d uv;
            if (pointSurEsquisse(position, uv))
            {
                m_dernierUV = uv;
                gp_Pnt2d saisi;
                if (!m_saisie.isEmpty() && pointDepuisSaisie(saisi))
                    mettreAJourApercu(saisi);
                else
                    mettreAJourApercu(uv);
            }
        }
        else
        {
            // Simple survol : mise en surbrillance de l'élément sous le curseur.
            // On ne redessine que si l'élément survolé change, pour éviter le scintillement.
            // La détection est limitée à ~20 fois par seconde : près du bord d'un perçage,
            // deux faces sont détectées à tour de rôle d'un pixel à l'autre, ce qui faisait
            // clignoter la surbrillance quand la souris bougeait.
            if (!m_mesureActive)
                masquerBulle();
            constexpr qint64 intervalleMs = 50;
            m_survolEnAttente = position;
            if (m_horlogeSurvol.isValid() && m_horlogeSurvol.elapsed() < intervalleMs)
            {
                if (!m_minuterieSurvol)
                {
                    m_minuterieSurvol = new QTimer(this);
                    m_minuterieSurvol->setSingleShot(true);
                    connect(m_minuterieSurvol, &QTimer::timeout, this, [this]() {
                        if (!m_esquisseActive && m_geste == Geste::Aucun)
                            appliquerSurvol(m_survolEnAttente);
                    });
                }
                if (!m_minuterieSurvol->isActive())
                    m_minuterieSurvol->start(static_cast<int>(intervalleMs - m_horlogeSurvol.elapsed()) + 1);
                return;
            }
            appliquerSurvol(position);
        }
    }
}

void ViewerWidget::appliquerSurvol(const QPoint& position)
{
    m_horlogeSurvol.restart();
    m_contexte->MoveTo(position.x(), position.y(), m_vue, false);
    Handle(SelectMgr_EntityOwner) survole;
    if (m_contexte->HasDetected())
        survole = m_contexte->DetectedOwner();
    if (survole.get() != m_dernierSurvol.get())
    {
        m_dernierSurvol = survole;
        m_contexte->UpdateCurrentViewer();
    }
    if (m_mesureActive)
        mettreAJourApercuMesure(position);
}

void ViewerWidget::mouseReleaseEvent(QMouseEvent* evenement)
{
    if (evenement->button() != Qt::LeftButton)
        return;

    // Relâchement qui suit le double-clic sur une cote : pas de nouveau point
    if (m_ignorerRelache)
    {
        m_ignorerRelache = false;
        m_geste          = Geste::Aucun;
        return;
    }

    // Fin d'un déplacement ou d'un étirement
    if (m_gesteDemarre)
    {
        terminerGeste();
        return;
    }

    m_geste = Geste::Aucun;

    const QPoint position = versPixels(evenement->position());

    // Un glissement est une rotation, pas une sélection.
    if ((position - m_positionPression).manhattanLength() > 4)
        return;

    // En esquisse, un clic place un point au lieu de sélectionner.
    if (m_esquisseActive)
    {
        placerPointEsquisse(position);
        return;
    }

    // Outil de mesure : un clic choisit un point à mesurer.
    if (m_mesureActive)
    {
        ajouterPointMesure(position);
        return;
    }

    const AIS_SelectionScheme schema = evenement->modifiers().testFlag(Qt::ShiftModifier)
                                           ? AIS_SelectionScheme_XOR
                                           : AIS_SelectionScheme_Replace;

    m_contexte->MoveTo(position.x(), position.y(), m_vue, false);
    m_contexte->SelectDetected(schema);
    m_contexte->UpdateCurrentViewer(); // redessine pour afficher la couleur de sélection
    m_dernierSurvol.Nullify();
    emit selectionChangee();
}

void ViewerWidget::wheelEvent(QWheelEvent* evenement)
{
    const QPoint position = versPixels(evenement->position());

    // Souris : angleDelta (120 par cran). Pavé tactile : valeurs plus petites, ou seulement
    // pixelDelta selon le système.
    int delta = evenement->angleDelta().y();
    if (delta == 0)
        delta = evenement->pixelDelta().y() * 2;
    if (delta == 0)
        delta = evenement->angleDelta().x();
    if (delta == 0)
        return;

    // Zoom centré sur le curseur ; si la vue n'a pas bougé, on applique directement
    // un changement d'échelle (proportionnel à la valeur reçue, donc doux au pavé tactile).
    const double avant = m_vue->Scale();
    m_vue->StartZoomAtPoint(position.x(), position.y());
    m_vue->ZoomAtPoint(0, 0, delta, 0);
    if (std::abs(m_vue->Scale() - avant) <= 1e-9 * std::abs(avant))
        m_vue->SetScale(avant * std::pow(1.0015, delta));
    ajusterGrilleFine();
    m_vue->Redraw();
    evenement->accept();
}

// ---------------------------------------------------------------------------
// Esquisse 2D
// ---------------------------------------------------------------------------

void ViewerWidget::demarrerEsquisse(const gp_Pnt& origine, const gp_Dir& normale)
{
    if (m_mesureActive)
        arreterMesure();
    arreterEsquisse();

    // Direction « u » du plan : X du monde, ou Z pour un plan vertical
    const gp_Dir indice = std::abs(normale.Z()) > 0.9 ? gp_Dir(1.0, 0.0, 0.0) : gp_Dir(0.0, 0.0, 1.0);
    m_repere = gp_Ax3(origine, normale, indice);

    m_esquisseActive = true;
    m_outilEsquisse  = OutilEsquisse::Rectangle;
    m_pointsEnCours.clear();
    m_profils.clear();
    m_descriptions.clear();

    // La vue se place face au plan d'esquisse
    m_vue->SetProj(normale.X(), normale.Y(), normale.Z());
    recadrer();
}

void ViewerWidget::arreterEsquisse()
{
    masquerBulle();
    effacerApercu();
    if (!m_objetCotes.IsNull())
    {
        m_contexte->Remove(m_objetCotes, false);
        m_objetCotes.Nullify();
    }
    if (!m_objetProfils.IsNull())
    {
        m_contexte->Remove(m_objetProfils, false);
        m_objetProfils.Nullify();
    }
    m_pointsEnCours.clear();
    m_profils.clear();
    m_descriptions.clear();
    m_esquisseActive = false;
    m_saisie.clear();
    m_contexte->UpdateCurrentViewer();
}

void ViewerWidget::definirOutilEsquisse(OutilEsquisse outil)
{
    m_outilEsquisse = outil;
    m_pointsEnCours.clear();
    m_saisie.clear();
    effacerApercu();
    m_contexte->UpdateCurrentViewer();
}

void ViewerWidget::effacerDernierProfil()
{
    if (!m_profils.empty())
        m_profils.pop_back();
    if (!m_descriptions.empty())
        m_descriptions.pop_back();
    mettreAJourAffichageEsquisse();
}

Esquisse ViewerWidget::esquisseCourante() const
{
    Esquisse e;
    e.origine = m_repere.Location();
    e.normale = m_repere.Direction();
    e.axeU    = m_repere.XDirection();
    e.profils = m_descriptions;
    return e;
}

bool ViewerWidget::pointSurEsquisse(const QPoint& ecran, gp_Pnt2d& uv) const
{
    gp_Pnt origine;
    gp_Vec direction;
    if (!rayonSousCurseur(ecran, origine, direction))
        return false;

    const gp_Vec normale(m_repere.Direction());
    const double denominateur = normale.Dot(direction);
    if (std::abs(denominateur) < 0.05)
        return false; // plan vu de profil

    const double t = normale.Dot(gp_Vec(origine, m_repere.Location())) / denominateur;
    const gp_Pnt  sur(origine.X() + t * direction.X(),
                      origine.Y() + t * direction.Y(),
                      origine.Z() + t * direction.Z());
    const gp_Vec  w(m_repere.Location(), sur);

    // En esquisse, la position se règle toujours au dixième de mm (sauf mode « Libre »)
    auto aimanterEsquisse = [this](double valeur) {
        if (m_pasAimantation <= 0.0)
            return valeur;
        const double pas = std::min(m_pasAimantation, 0.1);
        return std::round(valeur / pas) * pas;
    };
    uv = gp_Pnt2d(aimanterEsquisse(w.Dot(gp_Vec(m_repere.XDirection()))),
                  aimanterEsquisse(w.Dot(gp_Vec(m_repere.YDirection()))));
    return true;
}

gp_Pnt ViewerWidget::point3D(const gp_Pnt2d& uv) const
{
    const gp_Vec deplacement = gp_Vec(m_repere.XDirection()) * uv.X()
                             + gp_Vec(m_repere.YDirection()) * uv.Y();
    return m_repere.Location().Translated(deplacement);
}

TopoDS_Wire ViewerWidget::construireProfil(const std::vector<gp_Pnt2d>& points,
                                           const gp_Pnt2d* courant, bool ferme) const
{
    try
    {
        switch (m_outilEsquisse)
        {
        case OutilEsquisse::Rectangle:
        {
            if (points.size() != 1 || !courant)
                return TopoDS_Wire();
            const gp_Pnt2d a = points[0];
            const gp_Pnt2d b = *courant;
            if (std::abs(a.X() - b.X()) < 1e-6 || std::abs(a.Y() - b.Y()) < 1e-6)
                return TopoDS_Wire();

            BRepBuilderAPI_MakePolygon polygone(point3D(gp_Pnt2d(a.X(), a.Y())),
                                                point3D(gp_Pnt2d(b.X(), a.Y())),
                                                point3D(gp_Pnt2d(b.X(), b.Y())),
                                                point3D(gp_Pnt2d(a.X(), b.Y())), true);
            return polygone.IsDone() ? polygone.Wire() : TopoDS_Wire();
        }

        case OutilEsquisse::Cercle:
        {
            if (points.size() != 1 || !courant)
                return TopoDS_Wire();
            const double rayon = points[0].Distance(*courant);
            if (rayon < 1e-6)
                return TopoDS_Wire();

            const gp_Circ cercle(gp_Ax2(point3D(points[0]), m_repere.Direction(), m_repere.XDirection()),
                                 rayon);
            BRepBuilderAPI_MakeEdge arete(cercle);
            if (!arete.IsDone())
                return TopoDS_Wire();
            BRepBuilderAPI_MakeWire fil(arete.Edge());
            return fil.IsDone() ? fil.Wire() : TopoDS_Wire();
        }

        case OutilEsquisse::Polygone:
        {
            std::vector<gp_Pnt2d> sommets = points;
            if (courant && !ferme)
                sommets.push_back(*courant);

            BRepBuilderAPI_MakePolygon polygone;
            const gp_Pnt2d* precedent = nullptr;
            int nombre = 0;
            for (const gp_Pnt2d& sommet : sommets)
            {
                if (precedent && precedent->Distance(sommet) < 1e-6)
                    continue;
                polygone.Add(point3D(sommet));
                precedent = &sommet;
                ++nombre;
            }
            if (nombre < (ferme ? 3 : 2))
                return TopoDS_Wire();
            if (ferme)
                polygone.Close();
            return polygone.IsDone() ? polygone.Wire() : TopoDS_Wire();
        }
        }
    }
    catch (const Standard_Failure&)
    {
    }
    return TopoDS_Wire();
}

void ViewerWidget::effacerApercu()
{
    if (!m_objetApercu.IsNull())
    {
        m_contexte->Remove(m_objetApercu, false);
        m_objetApercu.Nullify();
    }
}

void ViewerWidget::mettreAJourApercu(const gp_Pnt2d& uv)
{
    effacerApercu();

    if (!m_pointsEnCours.empty())
    {
        const TopoDS_Wire contour = construireProfil(m_pointsEnCours, &uv, false);
        if (!contour.IsNull())
        {
            m_objetApercu = new AIS_Shape(contour);
            m_objetApercu->SetColor(Quantity_Color(1.0, 0.85, 0.1, Quantity_TOC_RGB));
            m_objetApercu->SetWidth(2.5);
            m_contexte->Display(m_objetApercu, AIS_WireFrame, -1, false);
            // Couche supérieure : sinon la face sélectionnée (surbrillance) recouvre le trait
            m_contexte->SetZLayer(m_objetApercu, Graphic3d_ZLayerId_Topmost);
        }

        const QLocale lieu;
        const gp_Pnt2d& depart = m_pointsEnCours.front();
        {
            auto n = [&lieu](double valeur) { return lieu.toString(valeur, 'f', 2); };
            QString bulle;
            if (m_outilEsquisse == OutilEsquisse::Rectangle)
                bulle = QObject::tr("L %1 × H %2 mm").arg(n(std::abs(uv.X() - depart.X())),
                                                          n(std::abs(uv.Y() - depart.Y())));
            else if (m_outilEsquisse == OutilEsquisse::Cercle)
                bulle = QObject::tr("R %1 mm · Ø %2 mm").arg(n(depart.Distance(uv)), n(2.0 * depart.Distance(uv)));
            else
            {
                const gp_Pnt2d& dernier = m_pointsEnCours.back();
                const double    angle   = std::atan2(uv.Y() - dernier.Y(), uv.X() - dernier.X()) * 180.0 / 3.14159265358979323846;
                bulle = QObject::tr("Longueur %1 mm · angle %2°").arg(n(dernier.Distance(uv)), n(angle));
            }
            bulle += QStringLiteral("\n") + QObject::tr("X %1 · Y %2").arg(n(uv.X()), n(uv.Y()));
            afficherBulle(bulle);
        }
        if (m_outilEsquisse == OutilEsquisse::Rectangle)
            emit messageApercu(QObject::tr("Rectangle : %1 × %2 mm")
                                   .arg(lieu.toString(std::abs(uv.X() - depart.X()), 'f', 2),
                                        lieu.toString(std::abs(uv.Y() - depart.Y()), 'f', 2)));
        else if (m_outilEsquisse == OutilEsquisse::Cercle)
            emit messageApercu(QObject::tr("Cercle : rayon %1 mm, diamètre %2 mm")
                                   .arg(lieu.toString(depart.Distance(uv), 'f', 2),
                                        lieu.toString(2.0 * depart.Distance(uv), 'f', 2)));
        else
            emit messageApercu(QObject::tr("Polygone : %1 sommet(s) — Entrée ou clic sur le 1er sommet pour fermer")
                                   .arg(static_cast<int>(m_pointsEnCours.size())));
    }
    else
    {
        const QLocale lieu;
        afficherBulle(QObject::tr("X %1 · Y %2").arg(lieu.toString(uv.X(), 'f', 2), lieu.toString(uv.Y(), 'f', 2)));
        emit messageApercu(QObject::tr("Esquisse : position (%1 ; %2) mm")
                               .arg(lieu.toString(uv.X(), 'f', 2), lieu.toString(uv.Y(), 'f', 2)));
    }
    m_contexte->UpdateCurrentViewer();
}

void ViewerWidget::ajouterProfil(const TopoDS_Wire& contour, const Profil& description)
{
    if (contour.IsNull())
        return;

    BRepBuilderAPI_MakeFace face(contour, true);
    if (!face.IsDone())
        return;

    m_profils.push_back(face.Face());
    m_descriptions.push_back(description);
    mettreAJourAffichageEsquisse();
}

void ViewerWidget::fermerPolygone()
{
    if (m_outilEsquisse != OutilEsquisse::Polygone || m_pointsEnCours.size() < 3)
        return;

    Profil description;
    description.type = TypeProfil::Polygone;
    for (const gp_Pnt2d& sommet : m_pointsEnCours)
    {
        description.v.push_back(sommet.X());
        description.v.push_back(sommet.Y());
    }
    ajouterProfil(construireProfil(m_pointsEnCours, nullptr, true), description);
    m_pointsEnCours.clear();
    effacerApercu();
    m_contexte->UpdateCurrentViewer();
}

void ViewerWidget::placerPointEsquisse(const QPoint& ecran)
{
    gp_Pnt2d uv;
    if (!pointSurEsquisse(ecran, uv))
        return;

    m_dernierUV = uv;
    gp_Pnt2d saisi;
    if (!m_saisie.isEmpty() && pointDepuisSaisie(saisi))
        uv = saisi;
    validerPointEsquisse(uv);
}

void ViewerWidget::validerPointEsquisse(const gp_Pnt2d& uv)
{
    m_saisie.clear();

    switch (m_outilEsquisse)
    {
    case OutilEsquisse::Rectangle:
    case OutilEsquisse::Cercle:
        if (m_pointsEnCours.empty())
        {
            m_pointsEnCours.push_back(uv);
        }
        else
        {
            Profil description;
            const gp_Pnt2d depart = m_pointsEnCours.front();
            if (m_outilEsquisse == OutilEsquisse::Rectangle)
            {
                description.type = TypeProfil::Rectangle;
                description.v = {std::min(depart.X(), uv.X()), std::min(depart.Y(), uv.Y()),
                                 std::abs(uv.X() - depart.X()), std::abs(uv.Y() - depart.Y())};
            }
            else
            {
                description.type = TypeProfil::Cercle;
                description.v = {depart.X(), depart.Y(), depart.Distance(uv)};
            }
            ajouterProfil(construireProfil(m_pointsEnCours, &uv, true), description);
            m_pointsEnCours.clear();
            effacerApercu();
            m_contexte->UpdateCurrentViewer();
        }
        break;

    case OutilEsquisse::Polygone:
    {
        const double tolerance = std::max(0.5, m_pasAimantation * 0.5);
        if (m_pointsEnCours.size() >= 3 && m_pointsEnCours.front().Distance(uv) < tolerance)
        {
            fermerPolygone();
        }
        else if (m_pointsEnCours.empty() || m_pointsEnCours.back().Distance(uv) > 1e-6)
        {
            m_pointsEnCours.push_back(uv);
        }
        break;
    }
    }
}

void ViewerWidget::mettreAJourAffichageEsquisse()
{
    if (!m_objetProfils.IsNull())
    {
        m_contexte->Remove(m_objetProfils, false);
        m_objetProfils.Nullify();
    }

    if (!m_profils.empty())
    {
        BRep_Builder constructeur;
        TopoDS_Compound ensemble;
        constructeur.MakeCompound(ensemble);
        for (const TopoDS_Shape& profil : m_profils)
            constructeur.Add(ensemble, profil);

        m_objetProfils = new AIS_Shape(ensemble);
        m_objetProfils->SetColor(Quantity_Color(0.2, 0.8, 1.0, Quantity_TOC_RGB));
        m_objetProfils->SetTransparency(0.5);
        m_objetProfils->SetWidth(2.5);
        m_objetProfils->Attributes()->SetFaceBoundaryDraw(true);
        m_objetProfils->Attributes()->SetFaceBoundaryAspect(
            new Prs3d_LineAspect(Quantity_Color(0.2, 0.8, 1.0, Quantity_TOC_RGB), Aspect_TOL_SOLID, 2.5));
        m_contexte->Display(m_objetProfils, AIS_Shaded, -1, false);
        m_contexte->SetZLayer(m_objetProfils, Graphic3d_ZLayerId_Topmost);
    }
    mettreAJourCotes();
    m_contexte->UpdateCurrentViewer();
}

namespace
{
std::string valeurCote(double v, const char* prefixe = "")
{
    char tampon[64];
    std::snprintf(tampon, sizeof(tampon), "%s%.2f", prefixe, v);
    return tampon;
}
} // namespace

TopoDS_Shape ViewerWidget::faceDepuisProfil(const Profil& profil) const
{
    try
    {
        const std::vector<double>& v = profil.v;
        TopoDS_Wire                contour;
        switch (profil.type)
        {
        case TypeProfil::Rectangle:
        {
            if (v.size() < 4)
                return TopoDS_Shape();
            BRepBuilderAPI_MakePolygon polygone(point3D(gp_Pnt2d(v[0], v[1])),
                                                point3D(gp_Pnt2d(v[0] + v[2], v[1])),
                                                point3D(gp_Pnt2d(v[0] + v[2], v[1] + v[3])),
                                                point3D(gp_Pnt2d(v[0], v[1] + v[3])), true);
            if (!polygone.IsDone())
                return TopoDS_Shape();
            contour = polygone.Wire();
            break;
        }
        case TypeProfil::Cercle:
        {
            if (v.size() < 3 || v[2] < 1e-6)
                return TopoDS_Shape();
            const gp_Circ cercle(gp_Ax2(point3D(gp_Pnt2d(v[0], v[1])), m_repere.Direction(), m_repere.XDirection()), v[2]);
            BRepBuilderAPI_MakeEdge arete(cercle);
            if (!arete.IsDone())
                return TopoDS_Shape();
            BRepBuilderAPI_MakeWire fil(arete.Edge());
            if (!fil.IsDone())
                return TopoDS_Shape();
            contour = fil.Wire();
            break;
        }
        case TypeProfil::Polygone:
        {
            if (v.size() < 6)
                return TopoDS_Shape();
            BRepBuilderAPI_MakePolygon polygone;
            for (size_t i = 0; i + 1 < v.size(); i += 2)
                polygone.Add(point3D(gp_Pnt2d(v[i], v[i + 1])));
            polygone.Close();
            if (!polygone.IsDone())
                return TopoDS_Shape();
            contour = polygone.Wire();
            break;
        }
        }
        BRepBuilderAPI_MakeFace face(contour, true);
        return face.IsDone() ? TopoDS_Shape(face.Face()) : TopoDS_Shape();
    }
    catch (const Standard_Failure&)
    {
        return TopoDS_Shape();
    }
}

bool ViewerWidget::modifierCote(const InfoCote& info, double valeur)
{
    if (info.profil < 0 || info.profil >= static_cast<int>(m_descriptions.size()) || valeur < 0.01)
        return false;

    Profil nouveau = m_descriptions[static_cast<size_t>(info.profil)];
    std::vector<double>& v = nouveau.v;
    switch (info.genre)
    {
    case 0: v[2] = valeur; break;       // largeur du rectangle
    case 1: v[3] = valeur; break;       // hauteur du rectangle
    case 2: v[2] = valeur / 2.0; break; // diamètre du cercle -> rayon
    case 3:                             // côté d'un polygone : on déplace son sommet d'arrivée
    {
        const size_t n = v.size() / 2;
        const size_t i = static_cast<size_t>(info.indice);
        const size_t j = (i + 1) % n;
        const double dx = v[2 * j] - v[2 * i];
        const double dy = v[2 * j + 1] - v[2 * i + 1];
        const double l  = std::hypot(dx, dy);
        if (l < 1e-9)
            return false;
        v[2 * j]     = v[2 * i] + dx / l * valeur;
        v[2 * j + 1] = v[2 * i + 1] + dy / l * valeur;
        break;
    }
    default: return false;
    }

    const TopoDS_Shape face = faceDepuisProfil(nouveau);
    if (face.IsNull())
        return false;
    m_descriptions[static_cast<size_t>(info.profil)] = nouveau;
    m_profils[static_cast<size_t>(info.profil)]      = face;
    mettreAJourAffichageEsquisse();
    return true;
}

void ViewerWidget::mouseDoubleClickEvent(QMouseEvent* evenement)
{
    // En esquisse, un double-clic sur une cote permet de changer sa valeur
    if (m_esquisseActive && evenement->button() == Qt::LeftButton)
    {
        const QPoint position = versPixels(evenement->position());
        int          meilleur = -1;
        double       distance = 1e9;
        auto versEcran = [this](const gp_Pnt& p) {
            Standard_Integer px = 0, py = 0;
            m_vue->Convert(p.X(), p.Y(), p.Z(), px, py);
            return QPointF(px, py);
        };
        for (size_t i = 0; i < m_infosCotes.size(); ++i)
        {
            // Proximité du nombre (40 px) ou du trait de cote (12 px) : on garde le plus proche
            const QPointF texte = versEcran(m_infosCotes[i].position);
            double        d     = std::hypot(texte.x() - position.x(), texte.y() - position.y());
            if (d > 40.0)
                d = 1e9;

            const QPointF a  = versEcran(m_infosCotes[i].debut);
            const QPointF b  = versEcran(m_infosCotes[i].fin);
            const QPointF ab = b - a;
            const double  l2 = ab.x() * ab.x() + ab.y() * ab.y();
            double        t  = l2 > 1e-9 ? ((position.x() - a.x()) * ab.x() + (position.y() - a.y()) * ab.y()) / l2 : 0.0;
            t                = std::max(0.0, std::min(1.0, t));
            const double dSegment = std::hypot(a.x() + t * ab.x() - position.x(), a.y() + t * ab.y() - position.y());
            if (dSegment <= 12.0)
                d = std::min(d, dSegment);

            if (d < distance)
            {
                distance = d;
                meilleur = static_cast<int>(i);
            }
        }
        if (distance > 1e8)
            meilleur = -1;

        if (meilleur >= 0)
        {
            // Le premier clic du double-clic a pu poser un point : on l'annule
            m_pointsEnCours.clear();
            m_saisie.clear();
            effacerApercu();
            masquerBulle();
            m_contexte->UpdateCurrentViewer();
            m_ignorerRelache = true;

            const InfoCote info   = m_infosCotes[static_cast<size_t>(meilleur)];
            bool           ok     = false;
            const double   valeur = QInputDialog::getDouble(this, QObject::tr("Modifier la cote"),
                                                            QObject::tr("Nouvelle valeur (mm) :"), info.valeur,
                                                            0.01, 100000.0, 2, &ok);
            if (ok && !modifierCote(info, valeur))
                emit messageApercu(QObject::tr("Cette valeur est impossible pour ce profil."));
            return;
        }
    }
    if (m_esquisseActive && evenement->button() == Qt::LeftButton)
        emit messageApercu(QObject::tr("Aucune cote à cet endroit : double-cliquez sur le nombre d'une cote (%1 cote(s) affichée(s)).")
                               .arg(static_cast<int>(m_infosCotes.size())));
    QWidget::mouseDoubleClickEvent(evenement);
}

void ViewerWidget::mettreAJourCotes()
{
    if (!m_objetCotes.IsNull())
    {
        m_contexte->Remove(m_objetCotes, false);
        m_objetCotes.Nullify();
    }

    const gp_Vec axeU(m_repere.XDirection());
    const gp_Vec axeV(m_repere.YDirection());
    auto clamp = [](double valeur, double mini, double maxi) { return std::max(mini, std::min(maxi, valeur)); };

    m_infosCotes.clear();
    std::vector<Cote> cotes;
    size_t            numeroProfil = 0;
    auto ajouter = [&](const Cote& cote, int genre, int indice, double valeur) {
        cotes.push_back(cote);
        InfoCote info;
        info.profil   = static_cast<int>(numeroProfil);
        info.genre    = genre;
        info.indice   = indice;
        info.valeur   = valeur;
        info.position = gp_Pnt((cote.a.X() + cote.b.X()) / 2.0 + cote.decalage.X() + cote.decalageTexte.X(),
                               (cote.a.Y() + cote.b.Y()) / 2.0 + cote.decalage.Y() + cote.decalageTexte.Y(),
                               (cote.a.Z() + cote.b.Z()) / 2.0 + cote.decalage.Z() + cote.decalageTexte.Z());
        info.debut = cote.a.Translated(cote.decalage);
        info.fin   = cote.b.Translated(cote.decalage);
        m_infosCotes.push_back(info);
    };
    for (size_t ip = 0; ip < m_descriptions.size(); ++ip, numeroProfil = ip)
    {
        const Profil& profil = m_descriptions[ip];
        const std::vector<double>& v = profil.v;
        switch (profil.type)
        {
        case TypeProfil::Rectangle:
        {
            if (v.size() < 4)
                break;
            const double u0 = v[0], v0 = v[1], largeur = v[2], hauteur = v[3];
            const double ecart = clamp(0.12 * std::min(std::abs(largeur), std::abs(hauteur)), 1.5, 8.0);

            Cote bas;
            bas.a        = point3D(gp_Pnt2d(u0, v0));
            bas.b        = point3D(gp_Pnt2d(u0 + largeur, v0));
            bas.decalage = axeV * (hauteur >= 0.0 ? -ecart : ecart);
            bas.texte    = valeurCote(std::abs(largeur));
            ajouter(bas, 0, 0, std::abs(largeur));

            Cote cote;
            cote.a        = point3D(gp_Pnt2d(u0 + largeur, v0));
            cote.b        = point3D(gp_Pnt2d(u0 + largeur, v0 + hauteur));
            cote.decalage = axeU * (largeur >= 0.0 ? ecart : -ecart);
            cote.texte    = valeurCote(std::abs(hauteur));
            ajouter(cote, 1, 0, std::abs(hauteur));
            break;
        }
        case TypeProfil::Cercle:
        {
            if (v.size() < 3)
                break;
            Cote diametre;
            diametre.a             = point3D(gp_Pnt2d(v[0] - v[2], v[1]));
            diametre.b             = point3D(gp_Pnt2d(v[0] + v[2], v[1]));
            diametre.attaches      = false;
            diametre.decalageTexte = axeV * clamp(0.25 * v[2], 1.5, 6.0);
            diametre.texte         = valeurCote(2.0 * v[2], "Ø ");
            ajouter(diametre, 2, 0, 2.0 * v[2]);
            break;
        }
        case TypeProfil::Polygone:
        {
            const size_t n = v.size() / 2;
            if (n < 3)
                break;
            double cu = 0.0, cv = 0.0;
            for (size_t i = 0; i < n; ++i)
            {
                cu += v[2 * i];
                cv += v[2 * i + 1];
            }
            cu /= static_cast<double>(n);
            cv /= static_cast<double>(n);

            for (size_t i = 0; i < n; ++i)
            {
                const size_t j = (i + 1) % n;
                const gp_Pnt2d p(v[2 * i], v[2 * i + 1]);
                const gp_Pnt2d q(v[2 * j], v[2 * j + 1]);
                const double   longueur = p.Distance(q);
                if (longueur < 1e-6)
                    continue;

                // Normale du côté, tournée vers l'extérieur du polygone
                double nu = -(q.Y() - p.Y()) / longueur;
                double nv = (q.X() - p.X()) / longueur;
                if (nu * ((p.X() + q.X()) / 2.0 - cu) + nv * ((p.Y() + q.Y()) / 2.0 - cv) < 0.0)
                {
                    nu = -nu;
                    nv = -nv;
                }
                const double ecart = clamp(0.12 * longueur, 1.5, 8.0);

                Cote cote;
                cote.a        = point3D(p);
                cote.b        = point3D(q);
                cote.decalage = (axeU * nu + axeV * nv) * ecart;
                cote.texte    = valeurCote(longueur);
                ajouter(cote, 3, static_cast<int>(i), longueur);
            }
            break;
        }
        }
    }

    if (cotes.empty())
        return;
    m_objetCotes = new CotesEsquisse(cotes);
    m_contexte->Display(m_objetCotes, 0, -1, false);
    m_contexte->SetZLayer(m_objetCotes, Graphic3d_ZLayerId_Topmost);
}

namespace
{

// Nombres contenus dans un texte tapé (séparés par x, espace, ;...), virgule ou point décimal.
std::vector<double> nombresDe(const QString& texte)
{
    std::vector<double> valeurs;
    const QStringList morceaux = texte.split(QRegularExpression(QStringLiteral("[^0-9.,]+")),
                                             Qt::SkipEmptyParts);
    for (QString morceau : morceaux)
    {
        morceau.replace(',', '.');
        bool ok = false;
        const double v = morceau.toDouble(&ok);
        if (ok)
            valeurs.push_back(v);
    }
    return valeurs;
}

} // namespace

bool ViewerWidget::pointDepuisSaisie(gp_Pnt2d& uv) const
{
    if (m_pointsEnCours.empty() || m_saisie.isEmpty())
        return false;

    const std::vector<double> v = nombresDe(m_saisie);
    if (v.empty() || v[0] <= 0.0)
        return false;

    const gp_Pnt2d depart = m_pointsEnCours.back();
    const double sensU = m_dernierUV.X() < depart.X() ? -1.0 : 1.0;
    const double sensV = m_dernierUV.Y() < depart.Y() ? -1.0 : 1.0;

    switch (m_outilEsquisse)
    {
    case OutilEsquisse::Rectangle:
    {
        const double largeur = v[0];
        const double hauteur = v.size() > 1 ? v[1] : v[0]; // une seule valeur : carré
        if (hauteur <= 0.0)
            return false;
        uv = gp_Pnt2d(depart.X() + sensU * largeur, depart.Y() + sensV * hauteur);
        return true;
    }

    case OutilEsquisse::Cercle:
    {
        // « d20 » = diamètre 20 mm ; sinon la valeur est le rayon
        const bool diametre = m_saisie.startsWith('d', Qt::CaseInsensitive);
        uv = gp_Pnt2d(depart.X() + (diametre ? v[0] / 2.0 : v[0]), depart.Y());
        return true;
    }

    case OutilEsquisse::Polygone:
    {
        // Longueur du côté, dans la direction de la souris
        double dx = m_dernierUV.X() - depart.X();
        double dy = m_dernierUV.Y() - depart.Y();
        const double norme = std::hypot(dx, dy);
        if (norme < 1e-9) { dx = 1.0; dy = 0.0; }
        else              { dx /= norme; dy /= norme; }
        uv = gp_Pnt2d(depart.X() + dx * v[0], depart.Y() + dy * v[0]);
        return true;
    }
    }
    return false;
}

void ViewerWidget::rafraichirApercuSaisie()
{
    gp_Pnt2d saisi;
    if (pointDepuisSaisie(saisi))
        mettreAJourApercu(saisi);

    const QString aide = m_outilEsquisse == OutilEsquisse::Rectangle
                             ? QObject::tr("largeur x hauteur")
                             : (m_outilEsquisse == OutilEsquisse::Cercle ? QObject::tr("rayon (d20 = diamètre)")
                                                                          : QObject::tr("longueur du côté"));
    emit messageApercu(QObject::tr("Cote saisie : %1   [%2] — Entrée pour valider, Retour arrière pour corriger")
                           .arg(m_saisie, aide));
}

bool ViewerWidget::event(QEvent* evenement)
{
    // Pendant un tracé, les chiffres et quelques lettres servent à saisir des cotes : on les
    // empêche de déclencher les raccourcis de la fenêtre (vues 1-4, X = extruder...).
    if (evenement->type() == QEvent::ShortcutOverride && m_esquisseActive && !m_pointsEnCours.empty())
    {
        auto* touche = static_cast<QKeyEvent*>(evenement);
        const QString t = touche->text();
        const bool saisie = (!t.isEmpty() && QStringLiteral("0123456789.,xXdD ;").contains(t[0]))
                            || touche->key() == Qt::Key_Backspace
                            || touche->key() == Qt::Key_Return || touche->key() == Qt::Key_Enter
                            || touche->key() == Qt::Key_Escape;
        if (saisie && touche->modifiers() == Qt::NoModifier)
        {
            evenement->accept();
            return true;
        }
    }
    // Qt a changé l'identifiant de la fenêtre native (réparentage...) : on rattache la vue à la nouvelle
    if (evenement->type() == QEvent::WinIdChange && m_fenetreAttachee)
    {
        const bool resultat = QWidget::event(evenement);
        attacherFenetre();
        ajusterVue();
        return resultat;
    }
    return QWidget::event(evenement);
}

// ---------------------------------------------------------------------------
// Mesure de distance entre deux points
// ---------------------------------------------------------------------------

void ViewerWidget::activerModesMesure()
{
    // Sommets, arêtes et faces sont tous sélectionnables : les sommets (prioritaires) donnent
    // un point exact, arêtes et faces donnent le point sous le curseur.
    for (const ObjetAffiche& affiche : m_objets)
    {
        m_contexte->Deactivate(affiche.objet);
        m_contexte->Activate(affiche.objet, AIS_Shape::SelectionMode(TopAbs_VERTEX));
        m_contexte->Activate(affiche.objet, AIS_Shape::SelectionMode(TopAbs_EDGE));
        m_contexte->Activate(affiche.objet, AIS_Shape::SelectionMode(TopAbs_FACE));
    }
}

void ViewerWidget::afficherParoisFines(const std::vector<gp_Pnt>& points, double rayon)
{
    effacerParoisFines();
    if (points.empty())
    {
        m_contexte->UpdateCurrentViewer();
        return;
    }

    BRep_Builder    constructeur;
    TopoDS_Compound reperes;
    constructeur.MakeCompound(reperes);
    for (const gp_Pnt& p : points)
        constructeur.Add(reperes, BRepPrimAPI_MakeSphere(p, std::max(rayon, 0.05)).Shape());

    m_paroisFines = new AIS_Shape(reperes);
    m_paroisFines->SetColor(Quantity_Color(0.95, 0.10, 0.10, Quantity_TOC_RGB));
    m_contexte->Display(m_paroisFines, AIS_Shaded, -1, false);
    m_contexte->UpdateCurrentViewer();
}

void ViewerWidget::effacerParoisFines()
{
    if (m_paroisFines.IsNull())
        return;
    m_contexte->Remove(m_paroisFines, false);
    m_paroisFines.Nullify();
}

void ViewerWidget::definirOutilMesure(bool actif)
{
    if (actif == m_mesureActive)
        return;

    if (!actif)
    {
        arreterMesure();
        return;
    }

    if (m_esquisseActive)
        return; // pas de mesure pendant une esquisse

    m_contexte->ClearSelected(false);
    m_mesureActive = true;
    if (m_ancragesMesure.size() >= 2)
        effacerMesure(); // on repart d'une mesure vierge
    m_dernierSurvol.Nullify();
    m_contexte->SetPixelTolerance(5); // viser un sommet doit rester facile
    activerModesMesure();
    // Pendant la mesure, le survol n'est plus rempli mais seulement dessiné en contour :
    // une face pleine en surbrillance recouvrirait le trait de mesure posé sur elle.
    m_contexte->HighlightStyle(Prs3d_TypeOfHighlight_Dynamic)->SetDisplayMode(AIS_WireFrame);
    m_contexte->HighlightStyle(Prs3d_TypeOfHighlight_LocalDynamic)->SetDisplayMode(AIS_WireFrame);
    setCursor(Qt::CrossCursor);
    m_contexte->UpdateCurrentViewer();
    emit selectionChangee();
    emit messageApercu(tr("Mesure : cliquez sur un premier point (sommet, milieu d'arête, centre de face, "
                          "de cercle ou de pièce), puis sur un second. Échap pour effacer puis quitter."));
}

void ViewerWidget::arreterMesure()
{
    if (!m_mesureActive)
        return;
    m_mesureActive = false;
    // Une mesure terminée reste affichée et suit la pièce quand on la déplace ou l'étire ;
    // un premier point resté seul est abandonné.
    if (m_ancragesMesure.size() < 2)
        effacerMesure();
    retirerMarqueurAccroche();
    masquerBulle();
    unsetCursor();
    m_dernierSurvol.Nullify();
    m_contexte->SetPixelTolerance(m_mode == ModeSelection::Face ? 1 : 2);
    m_contexte->HighlightStyle(Prs3d_TypeOfHighlight_Dynamic)->SetDisplayMode(AIS_Shaded);
    m_contexte->HighlightStyle(Prs3d_TypeOfHighlight_LocalDynamic)->SetDisplayMode(AIS_Shaded);

    for (const ObjetAffiche& affiche : m_objets)
    {
        m_contexte->Deactivate(affiche.objet);
        m_contexte->Activate(affiche.objet, modeOcct());
    }
    m_contexte->ClearSelected(false);
    m_contexte->UpdateCurrentViewer();
    emit outilMesureArrete();
    emit selectionChangee();
}

void ViewerWidget::retirerSegmentMesure()
{
    if (!m_ligneMesure.IsNull())
    {
        m_contexte->Remove(m_ligneMesure, false);
        m_ligneMesure.Nullify();
    }
    if (!m_texteMesure.IsNull())
    {
        m_contexte->Remove(m_texteMesure, false);
        m_texteMesure.Nullify();
    }
}

void ViewerWidget::retirerMarqueurAccroche()
{
    if (!m_marqueurAccroche.IsNull())
    {
        m_contexte->Remove(m_marqueurAccroche, false);
        m_marqueurAccroche.Nullify();
    }
}

void ViewerWidget::effacerMesure()
{
    retirerSegmentMesure();
    retirerMarqueurAccroche();
    for (const Handle(AIS_Point)& marqueur : m_marqueursMesure)
        m_contexte->Remove(marqueur, false);
    m_marqueursMesure.clear();
    m_ancragesMesure.clear();
    m_formesMesure.clear();
    m_positionsAvantGeste.clear();
}

QString ViewerWidget::nomAccroche(ancrage::Genre genre)
{
    switch (genre)
    {
    case ancrage::Genre::Sommet:       return QObject::tr("Sommet");
    case ancrage::Genre::MilieuArete:  return QObject::tr("Milieu d'arête");
    case ancrage::Genre::CentreCercle: return QObject::tr("Centre du cercle");
    case ancrage::Genre::CentreFace:   return QObject::tr("Centre de la face");
    case ancrage::Genre::CentrePiece:  return QObject::tr("Centre de la pièce");
    default:                           break;
    }
    return QString();
}

void ViewerWidget::afficherMarqueur(Handle(AIS_Point)& marqueur, const gp_Pnt& p, const Quantity_Color& couleur)
{
    if (!marqueur.IsNull())
        m_contexte->Remove(marqueur, false);
    marqueur = new AIS_Point(new Geom_CartesianPoint(p));
    marqueur->SetColor(couleur);
    m_contexte->Display(marqueur, 0, -1, false);
    m_contexte->SetZLayer(marqueur, Graphic3d_ZLayerId_Topmost);
}

bool ViewerWidget::pointMesure(gp_Pnt& point, bool& sommet) const
{
    sommet = false;
    if (!m_contexte->HasDetected())
        return false;

    const Handle(SelectMgr_EntityOwner) proprietaire = m_contexte->DetectedOwner();
    if (proprietaire.IsNull())
        return false;

    // Sommet : coordonnées exactes
    const Handle(StdSelect_BRepOwner) brep = Handle(StdSelect_BRepOwner)::DownCast(proprietaire);
    if (!brep.IsNull() && brep->HasShape() && brep->Shape().ShapeType() == TopAbs_VERTEX)
    {
        point  = BRep_Tool::Pnt(TopoDS::Vertex(brep->Shape()));
        sommet = true;
        return true;
    }

    // Arête ou face : point touché par le rayon de visée
    const auto& selecteur = m_contexte->MainSelector();
    for (Standard_Integer i = 1; i <= selecteur->NbPicked(); ++i)
    {
        if (selecteur->Picked(i).get() == proprietaire.get())
        {
            point = selecteur->PickedPoint(i);
            return true;
        }
    }
    return false;
}

bool ViewerWidget::chercherAccroche(const QPoint& ecran, ancrage::Ancrage& choix, bool& libre) const
{
    libre = false;
    if (!m_contexte->HasDetected())
        return false;

    const Handle(StdSelect_BRepOwner) brep = Handle(StdSelect_BRepOwner)::DownCast(m_contexte->DetectedOwner());
    if (brep.IsNull() || !brep->HasShape())
        return false;
    const Handle(AIS_Shape) objet = Handle(AIS_Shape)::DownCast(brep->Selectable());
    if (objet.IsNull())
        return false;
    const int id = idPieceDe(objet);
    if (id == 0)
        return false;

    const TopoDS_Shape piece   = objet->Shape();
    const TopoDS_Shape element = brep->Shape();

    auto cache = m_centresPieces.find(id);
    if (cache == m_centresPieces.end())
        cache = const_cast<ViewerWidget*>(this)->m_centresPieces.emplace(id, ancrage::centreDePiece(piece)).first;

    // Candidats : celui qui est le plus près du curseur, à l'écran, dans un rayon de 14 pixels.
    constexpr double rayonPixels = 14.0;
    double           meilleur    = rayonPixels * rayonPixels;
    bool             trouve      = false;
    for (const ancrage::Ancrage& c : ancrage::candidats(piece, element, id, &cache->second))
    {
        Standard_Integer px = 0, py = 0;
        m_vue->Convert(c.position.X(), c.position.Y(), c.position.Z(), px, py);
        const double dx = px - ecran.x();
        const double dy = py - ecran.y();
        const double d  = dx * dx + dy * dy;
        if (d < meilleur)
        {
            meilleur = d;
            choix    = c;
            trouve   = true;
        }
    }
    if (trouve)
        return true;

    // Aucun point remarquable à proximité : point quelconque de la face ou de l'arête visée.
    gp_Pnt p;
    bool   sommet = false;
    if (!pointMesure(p, sommet))
        return false;
    if (ancrage::surElement(piece, element, id, p, choix))
    {
        libre = true;
        return true;
    }
    choix          = ancrage::Ancrage();
    choix.position = p;
    libre          = true;
    return true;
}

QString ViewerWidget::texteMesure(const gp_Pnt& a, const gp_Pnt& b, bool detail) const
{
    const QLocale lieu;
    const QString distance = lieu.toString(a.Distance(b), 'f', 2);
    if (!detail)
        return QObject::tr("%1 mm").arg(distance);
    return QObject::tr("Distance %1 mm  (dX %2 · dY %3 · dZ %4)")
        .arg(distance,
             lieu.toString(std::abs(b.X() - a.X()), 'f', 2),
             lieu.toString(std::abs(b.Y() - a.Y()), 'f', 2),
             lieu.toString(std::abs(b.Z() - a.Z()), 'f', 2));
}

void ViewerWidget::dessinerSegmentMesure(const gp_Pnt& a, const gp_Pnt& b)
{
    retirerSegmentMesure();
    if (a.Distance(b) < 1e-7)
        return;

    const Quantity_Color bleu(0.15, 0.50, 1.00, Quantity_TOC_RGB);

    m_ligneMesure = new AIS_Shape(BRepBuilderAPI_MakeEdge(a, b).Edge());
    m_ligneMesure->SetColor(bleu);
    m_ligneMesure->SetWidth(3.0);
    m_contexte->Display(m_ligneMesure, AIS_WireFrame, -1, false);
    // Couche supérieure : le trait reste visible même posé sur une face.
    m_contexte->SetZLayer(m_ligneMesure, Graphic3d_ZLayerId_Topmost);

    gp_Pnt milieu((a.X() + b.X()) / 2.0, (a.Y() + b.Y()) / 2.0, (a.Z() + b.Z()) / 2.0);

    // Le texte est écarté du trait (environ 22 pixels), perpendiculairement à lui à l'écran,
    // du côté « haut » de la vue, pour rester lisible.
    {
        const Handle(Graphic3d_Camera)& camera = m_vue->Camera();
        const gp_Vec axe(a, b);
        gp_Vec       lateral = axe.Crossed(gp_Vec(camera->Direction()));
        if (lateral.Magnitude() < 1e-9 * (1.0 + axe.Magnitude()))
            lateral = gp_Vec(camera->Up()); // trait pointé vers l'observateur
        if (lateral.Magnitude() > 1e-12)
        {
            lateral.Normalize();
            if (lateral.Dot(gp_Vec(camera->Up())) < 0.0)
                lateral.Reverse();
            milieu.Translate(lateral * m_vue->Convert(22));
        }
    }

    m_texteMesure = new AIS_TextLabel();
    m_texteMesure->SetPosition(milieu);
    m_texteMesure->SetText(TCollection_ExtendedString(texteMesure(a, b, false).toUtf8().constData(), true));
    m_texteMesure->SetColor(bleu);
    m_texteMesure->SetHeight(18.0);
    m_contexte->Display(m_texteMesure, 0, -1, false);
    m_contexte->SetZLayer(m_texteMesure, Graphic3d_ZLayerId_Topmost);
}

void ViewerWidget::redessinerMesure()
{
    // Repose les repères et le trait d'après la position actuelle des ancrages.
    const Quantity_Color bleu(0.15, 0.50, 1.00, Quantity_TOC_RGB);
    for (const Handle(AIS_Point)& marqueur : m_marqueursMesure)
        m_contexte->Remove(marqueur, false);
    m_marqueursMesure.clear();
    for (const ancrage::Ancrage& a : m_ancragesMesure)
    {
        Handle(AIS_Point) marqueur;
        afficherMarqueur(marqueur, a.position, bleu);
        m_marqueursMesure.push_back(marqueur);
    }
    if (m_ancragesMesure.size() >= 2)
        dessinerSegmentMesure(m_ancragesMesure[0].position, m_ancragesMesure[1].position);
}

void ViewerWidget::actualiserMesure(const Document& document)
{
    if (m_ancragesMesure.empty())
        return;

    m_centresPieces.clear();
    for (ancrage::Ancrage& a : m_ancragesMesure)
    {
        if (a.idPiece == 0)
            continue;
        const Piece* piece = document.trouver(a.idPiece);
        if (!piece)
        {
            a.genre   = ancrage::Genre::Libre; // la pièce a disparu : le point reste où il était
            a.idPiece = 0;
            continue;
        }
        const auto ancienne = m_formesMesure.find(a.idPiece);
        ancrage::recaler(a, ancienne != m_formesMesure.end() ? ancienne->second : TopoDS_Shape(), piece->forme);
    }

    // Mémorise l'état actuel des pièces concernées pour la prochaine fois.
    m_formesMesure.clear();
    for (const ancrage::Ancrage& a : m_ancragesMesure)
    {
        if (a.idPiece == 0)
            continue;
        if (const Piece* piece = document.trouver(a.idPiece))
            m_formesMesure[a.idPiece] = piece->forme;
    }
    redessinerMesure();
}

void ViewerWidget::memoriserPositionsMesure()
{
    // Au début d'un geste : positions de départ et forme de départ de chaque pièce mesurée.
    m_positionsAvantGeste.clear();
    for (const ancrage::Ancrage& a : m_ancragesMesure)
        m_positionsAvantGeste.push_back(a.position);
}

void ViewerWidget::suivreMesureRigide()
{
    // Pendant un déplacement ou une rotation : les ancrages de la pièce bougent avec elle.
    if (m_ancragesMesure.empty() || m_positionsAvantGeste.size() != m_ancragesMesure.size())
        return;

    for (std::size_t i = 0; i < m_ancragesMesure.size(); ++i)
    {
        ancrage::Ancrage& a = m_ancragesMesure[i];
        if (a.idPiece == 0)
            continue;
        for (const ObjetAffiche& affiche : m_objets)
        {
            if (affiche.idPiece != a.idPiece)
                continue;
            if (std::find(m_objetsDeplaces.begin(), m_objetsDeplaces.end(), affiche.objet) == m_objetsDeplaces.end())
                break;
            a.position = m_positionsAvantGeste[i].Transformed(affiche.objet->LocalTransformation());
            break;
        }
    }
    redessinerMesure();
}

void ViewerWidget::suivreMesureEtirement(const TopoDS_Shape& nouvelle, double distance)
{
    // Pendant l'étirement d'une face : un point situé sur cette face la suit, les autres
    // restent attachés à leur élément dans la pièce modifiée.
    if (m_ancragesMesure.empty() || m_positionsAvantGeste.size() != m_ancragesMesure.size())
        return;

    const gp_Pnt   pointFace = m_pointFace;
    const gp_Vec   normale   = m_normaleFace;
    const gp_Vec   decalage  = normale.Multiplied(distance);
    const std::function<gp_Pnt(const gp_Pnt&)> prediction = [pointFace, normale, decalage](const gp_Pnt& p) {
        const double hauteur = gp_Vec(pointFace, p).Dot(normale);
        return std::abs(hauteur) < 1e-6 ? p.Translated(decalage) : p;
    };

    for (std::size_t i = 0; i < m_ancragesMesure.size(); ++i)
    {
        ancrage::Ancrage& a = m_ancragesMesure[i];
        if (a.idPiece != m_idEtire)
            continue;
        a.position = m_positionsAvantGeste[i];
        ancrage::recaler(a, m_formeInitiale, nouvelle, prediction);
    }
    redessinerMesure();
}

void ViewerWidget::mettreAJourApercuMesure(const QPoint& ecran)
{
    // Accroche : un repère vert montre le point remarquable visé. Après le premier point,
    // un trait bleu suit le curseur et la bulle donne la distance en direct.
    ancrage::Ancrage choix;
    bool             libre = false;
    const bool       trouve = chercherAccroche(ecran, choix, libre);

    if (trouve && !libre)
        afficherMarqueur(m_marqueurAccroche, choix.position, Quantity_Color(0.10, 0.80, 0.25, Quantity_TOC_RGB));
    else
        retirerMarqueurAccroche();

    const QString nom = (trouve && !libre) ? nomAccroche(choix.genre) : QString();

    if (m_ancragesMesure.size() == 1)
    {
        const gp_Pnt& depart = m_ancragesMesure.front().position;
        gp_Pnt        p;
        if (trouve)
        {
            p = choix.position;
        }
        else
        {
            // Hors des pièces : point du rayon de visée le plus proche du premier point
            gp_Pnt origine;
            gp_Vec direction;
            if (!rayonSousCurseur(ecran, origine, direction) || direction.SquareMagnitude() < 1e-18)
                return;
            const double t = gp_Vec(origine, depart).Dot(direction) / direction.SquareMagnitude();
            p = origine.Translated(direction * t);
        }
        dessinerSegmentMesure(depart, p);
        QString texte = texteMesure(depart, p, true);
        if (!nom.isEmpty())
            texte += QStringLiteral("\n") + nom;
        afficherBulle(texte);
    }
    else if (!nom.isEmpty())
    {
        afficherBulle(nom);
    }
    else
    {
        masquerBulle();
    }
    m_contexte->UpdateCurrentViewer();
}

void ViewerWidget::ajouterPointMesure(const QPoint& ecran)
{
    m_contexte->MoveTo(ecran.x(), ecran.y(), m_vue, false);

    ancrage::Ancrage choix;
    bool             libre = false;
    if (!chercherAccroche(ecran, choix, libre))
        return;

    if (m_ancragesMesure.size() >= 2)
    {
        effacerMesure(); // nouvelle mesure
        masquerBulle();
    }

    m_ancragesMesure.push_back(choix);
    if (choix.idPiece != 0)
    {
        // Forme de la pièce à cet instant : sert à retrouver l'élément après une modification.
        for (const ObjetAffiche& affiche : m_objets)
        {
            if (affiche.idPiece == choix.idPiece)
                m_formesMesure[choix.idPiece] = affiche.objet->Shape();
        }
    }
    retirerMarqueurAccroche();
    redessinerMesure();

    if (m_ancragesMesure.size() == 1)
    {
        emit messageApercu(tr("Mesure : premier point posé. Cliquez sur le second point."));
    }
    else
    {
        masquerBulle();
        emit mesureEffectuee(texteMesure(m_ancragesMesure[0].position, m_ancragesMesure[1].position, true));
    }
    m_dernierSurvol.Nullify();
    m_contexte->UpdateCurrentViewer();
}

void ViewerWidget::keyPressEvent(QKeyEvent* evenement)
{
    if (m_mesureActive && evenement->key() == Qt::Key_Escape)
    {
        if (!m_ancragesMesure.empty())
        {
            effacerMesure();
            masquerBulle();
            m_contexte->UpdateCurrentViewer();
            emit messageApercu(tr("Mesure : cliquez sur un premier point. Échap pour quitter l'outil."));
        }
        else
        {
            arreterMesure();
        }
        return;
    }
    if (!m_mesureActive && !m_esquisseActive && evenement->key() == Qt::Key_Escape
        && (!m_paroisFines.IsNull() || !m_ancragesMesure.empty()))
    {
        effacerParoisFines();
        effacerMesure();
        m_contexte->UpdateCurrentViewer();
        return;
    }
    if (m_esquisseActive)
    {
        const QString t = evenement->text();

        if (!m_pointsEnCours.empty() && evenement->modifiers() == Qt::NoModifier)
        {
            if (!t.isEmpty() && QStringLiteral("0123456789.,xXdD ;").contains(t[0]))
            {
                m_saisie += t;
                rafraichirApercuSaisie();
                return;
            }
            if (evenement->key() == Qt::Key_Backspace && !m_saisie.isEmpty())
            {
                m_saisie.chop(1);
                rafraichirApercuSaisie();
                return;
            }
        }

        if (evenement->key() == Qt::Key_Return || evenement->key() == Qt::Key_Enter)
        {
            gp_Pnt2d saisi;
            if (!m_saisie.isEmpty() && pointDepuisSaisie(saisi))
                validerPointEsquisse(saisi);
            else
                fermerPolygone();
            return;
        }
        if (evenement->key() == Qt::Key_Escape)
        {
            if (!m_saisie.isEmpty())
            {
                m_saisie.clear();
                effacerApercu();
                m_contexte->UpdateCurrentViewer();
                return;
            }
            m_pointsEnCours.clear();
            effacerApercu();
            m_contexte->UpdateCurrentViewer();
            return;
        }
    }
    QWidget::keyPressEvent(evenement);
}
