#include "ViewerWidget.h"

#include <QLocale>
#include <QMouseEvent>
#include <QObject>
#include <QTimer>
#include <QWheelEvent>

#include <AIS_SelectionScheme.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_TypeOfLine.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Prs3d_TypeOfHighlight.hxx>
#include <Quantity_Color.hxx>
#include <StdSelect_BRepOwner.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>

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
    auto configurerSurbrillance = [this](Prs3d_TypeOfHighlight type, const Quantity_Color& couleur) {
        const Handle(Prs3d_Drawer)& style = m_contexte->HighlightStyle(type);
        style->SetColor(couleur);
        style->SetDisplayMode(AIS_Shaded);
        style->SetLineAspect(new Prs3d_LineAspect(couleur, Aspect_TOL_SOLID, 3.0));
        style->SetWireAspect(new Prs3d_LineAspect(couleur, Aspect_TOL_SOLID, 3.0));
    };
    const Quantity_Color grisSurvol(0.33, 0.35, 0.40, Quantity_TOC_RGB);
    const Quantity_Color orangeSelection(1.00, 0.42, 0.00, Quantity_TOC_RGB);

    // OpenCascade utilise des styles distincts selon que l'élément est un objet
    // entier (Dynamic / Selected) ou une partie d'un objet, face ou arête
    // (LocalDynamic / LocalSelected) : il faut régler les quatre.
    configurerSurbrillance(Prs3d_TypeOfHighlight_Dynamic, grisSurvol);
    configurerSurbrillance(Prs3d_TypeOfHighlight_LocalDynamic, grisSurvol);
    configurerSurbrillance(Prs3d_TypeOfHighlight_Selected, orangeSelection);
    configurerSurbrillance(Prs3d_TypeOfHighlight_LocalSelected, orangeSelection);

    // Vue rattachée à la fenêtre native de ce widget
    m_vue = m_viewer->CreateView();
#ifdef _WIN32
    Handle(WNT_Window) fenetre = new WNT_Window(reinterpret_cast<Aspect_Handle>(winId()));
#else
    Handle(Xw_Window) fenetre = new Xw_Window(connexion, static_cast<Aspect_Drawable>(winId()));
#endif
    m_vue->SetWindow(fenetre);
    if (!fenetre->IsMapped())
        fenetre->Map();

    m_vue->SetBackgroundColor(Quantity_Color(0.15, 0.17, 0.20, Quantity_TOC_RGB));
    m_vue->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_WHITE, 0.08, V3d_ZBUFFER);
    m_vue->SetProj(V3d_XposYnegZpos);
    m_vue->MustBeResized();

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
    for (const ObjetAffiche& affiche : m_objets)
        m_contexte->Remove(affiche.objet, false);
    m_objets.clear();

    for (const Piece& piece : document.pieces())
    {
        Handle(AIS_Shape) objet = new AIS_Shape(piece.forme);
        objet->SetColor(Quantity_Color(0.72, 0.76, 0.82, Quantity_TOC_RGB));

        // Arêtes dessinées en sombre : on distingue les faces et on voit les arêtes à choisir.
        objet->Attributes()->SetFaceBoundaryDraw(true);
        objet->Attributes()->SetFaceBoundaryAspect(
            new Prs3d_LineAspect(Quantity_NOC_BLACK, Aspect_TOL_SOLID, 1.5));

        m_contexte->Display(objet, AIS_Shaded, modeOcct(), false);
        m_objets.push_back({piece.id, objet});
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

void ViewerWidget::recadrer()
{
    // On cadre sur les pièces seules : la grille, plus grande, ne doit pas
    // empêcher de zoomer sur une petite pièce.
    Bnd_Box boite;
    for (const ObjetAffiche& affiche : m_objets)
        boite.Add(affiche.objet->BoundingBox());

    if (boite.IsVoid())
        boite.Update(-60.0, -60.0, -10.0, 60.0, 60.0, 60.0); // aperçu de la zone de travail

    m_vue->FitAll(boite, 0.1, false);
    m_vue->ZFitAll();
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

void ViewerWidget::ajusterVue()
{
    if (m_vue.IsNull())
        return;

    m_vue->Window()->DoResize();
    m_vue->MustBeResized();
    m_vue->Invalidate();
    m_vue->Redraw();
}

void ViewerWidget::paintEvent(QPaintEvent*)
{
    m_vue->Redraw();
}

void ViewerWidget::resizeEvent(QResizeEvent*)
{
    ajusterVue();
}

void ViewerWidget::showEvent(QShowEvent*)
{
    ajusterVue();
    // Une fois la mise en page terminée, on resynchronise une dernière fois
    // (et on cadre la vue au tout premier affichage).
    QTimer::singleShot(0, this, [this]() {
        ajusterVue();
        if (m_premierAffichage)
        {
            m_premierAffichage = false;
            recadrer();
        }
    });
}

void ViewerWidget::mousePressEvent(QMouseEvent* evenement)
{
    m_dernierePosition = evenement->position().toPoint();
    m_positionPression = m_dernierePosition;
    if (evenement->button() == Qt::LeftButton)
        m_vue->StartRotation(m_dernierePosition.x(), m_dernierePosition.y());
}

void ViewerWidget::mouseMoveEvent(QMouseEvent* evenement)
{
    const QPoint position = evenement->position().toPoint();

    if (evenement->buttons() & Qt::LeftButton)
    {
        m_vue->Rotation(position.x(), position.y());
    }
    else if (evenement->buttons() & (Qt::RightButton | Qt::MiddleButton))
    {
        m_vue->Pan(position.x() - m_dernierePosition.x(),
                   m_dernierePosition.y() - position.y());
        m_dernierePosition = position;
    }
    else
    {
        // Simple survol : mise en surbrillance de l'élément sous le curseur
        m_contexte->MoveTo(position.x(), position.y(), m_vue, true);
    }
}

void ViewerWidget::mouseReleaseEvent(QMouseEvent* evenement)
{
    if (evenement->button() != Qt::LeftButton)
        return;

    const QPoint position = evenement->position().toPoint();

    // Un glissement est une rotation, pas une sélection.
    if ((position - m_positionPression).manhattanLength() > 4)
        return;

    const AIS_SelectionScheme schema = evenement->modifiers().testFlag(Qt::ShiftModifier)
                                           ? AIS_SelectionScheme_XOR
                                           : AIS_SelectionScheme_Replace;

    m_contexte->MoveTo(position.x(), position.y(), m_vue, false);
    m_contexte->SelectDetected(schema);
    m_contexte->UpdateCurrentViewer(); // redessine pour afficher la couleur de sélection
    emit selectionChangee();
}

void ViewerWidget::wheelEvent(QWheelEvent* evenement)
{
    const QPoint position = evenement->position().toPoint();
    m_vue->StartZoomAtPoint(position.x(), position.y());
    m_vue->ZoomAtPoint(0, 0, evenement->angleDelta().y(), 0);
}
