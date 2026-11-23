#include "MainWindow.h"

#include <algorithm>
#include <cmath>

#include <QAction>
#include <QCoreApplication>
#include <QActionGroup>
#include <BRepBuilderAPI_Transform.hxx>
#include <memory>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QLocale>
#include <QDoubleSpinBox>
#include <QSettings>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QFileInfo>
#include <QDockWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QKeySequence>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QCursor>
#include <QToolBar>
#include <QToolTip>
#include <QRect>
#include <QScreen>
#include <QScrollArea>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTimer>

#include "DialogueDimensions.h"
#include "Icones.h"
#include "DialogueTransformation.h"
#include "Modelisation.h"
#include "Traduction.h"
#include "PanneauProprietes.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("FreeTable3D");
    resize(1315, 920);

    m_vue = new ViewerWidget;
    setCentralWidget(m_vue);

    creerPanneauPieces();
    creerPanneaux();
    creerMenus();

    // Disposition par défaut, puis celle de la dernière session si elle a été mémorisée
    dispositionParDefaut();
    {
        QSettings reglages("FreeTable3D", "FreeTable3D");
        if (reglages.contains("fenetre/geometrie"))
            restoreGeometry(reglages.value("fenetre/geometrie").toByteArray());
        if (reglages.contains("fenetre/etat"))
            restoreState(reglages.value("fenetre/etat").toByteArray());
    }

    connect(m_vue, &ViewerWidget::selectionChangee, this, &MainWindow::surSelectionChangee);

    // Pendant un déplacement ou un étirement à la souris : message dans la barre d'état
    connect(m_vue, &ViewerWidget::messageApercu, this,
            [this](const QString& message) { statusBar()->showMessage(message); });

    // Valeurs affichées près du curseur pendant un geste à la souris
    connect(m_vue, &ViewerWidget::apercuGeste, this, [this](const QString& texte) {
        if (texte.isEmpty())
            QToolTip::hideText();
        else
            QToolTip::showText(QCursor::pos() + QPoint(20, 24), texte, m_vue);
    });

    // Déplacement à la souris terminé : on l'enregistre dans le document (annulable)
    connect(m_vue, &ViewerWidget::deplacementTermine, this,
            [this](const QVector<int>& ids, double dx, double dy) {
                const std::vector<int> pieces(ids.begin(), ids.end());
                appliquerTransformation(pieces, dx, dy, 0.0, 0.0, 0.0, 0.0);
                rafraichirEnGardantSelection(pieces);
            });

    // Rotation à la souris terminée
    connect(m_vue, &ViewerWidget::rotationTerminee, this,
            [this](const QVector<int>& ids, double rx, double ry, double rz) {
                const std::vector<int> pieces(ids.begin(), ids.end());
                appliquerTransformation(pieces, 0.0, 0.0, 0.0, rx, ry, rz);
                rafraichirEnGardantSelection(pieces);
            });

    // Étirement d'une face à la souris terminé
    connect(m_vue, &ViewerWidget::etirementTermine, this,
            [this](int idPiece, const TopoDS_Shape& face, double distance) {
                m_document.modifier({idPiece}, [face, distance](const TopoDS_Shape& forme) {
                    return modelisation::etirerFace(forme, face, distance);
                });
                rafraichir();
            });

    rafraichir();
    appliquerPlateau();
    m_modifie = false;
    majTitre();

    // Sauvegarde automatique : une minute d'intervalle, seulement dans la première instance ouverte
    m_verrou = std::make_unique<QLockFile>(dossierRecuperation() + QStringLiteral("/instance.lock"));
    if (m_verrou->tryLock(0))
    {
        m_minuteurAuto = new QTimer(this);
        m_minuteurAuto->setInterval(60 * 1000);
        connect(m_minuteurAuto, &QTimer::timeout, this, &MainWindow::sauvegardeAutomatique);
        m_minuteurAuto->start();
        QTimer::singleShot(0, this, &MainWindow::proposerRecuperation);
    }
}

void MainWindow::creerPanneauPieces()
{
    m_liste = new QListWidget;
    m_liste->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_liste, &QListWidget::itemSelectionChanged, this, &MainWindow::surSelectionListe);

    auto* panneau = new QDockWidget(tr("Pièces"), this);
    panneau->setObjectName(QStringLiteral("dockPieces"));
    panneau->setWidget(m_liste);
    panneau->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::LeftDockWidgetArea, panneau);
}

void MainWindow::creerPanneaux()
{
    m_panneau = new PanneauProprietes;

    auto* dock = new QDockWidget(tr("Propriétés"), this);
    dock->setObjectName(QStringLiteral("dockProprietes"));
    // Zone à défilement : un panneau très long (esquisse avec beaucoup de profils) ne doit jamais
    // agrandir la fenêtre au-delà de l'écran.
    auto* defilement = new QScrollArea;
    defilement->setWidget(m_panneau);
    defilement->setWidgetResizable(true);
    defilement->setFrameShape(QFrame::NoFrame);
    defilement->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    dock->setWidget(defilement);
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    connect(m_panneau, &PanneauProprietes::poserFaceAPlatDemande, this, &MainWindow::poserFaceAPlat);
    connect(m_panneau, &PanneauProprietes::centrerDemande, this, &MainWindow::centrerSurPlateau);
    connect(m_panneau, &PanneauProprietes::poserSurPlateauDemande, this, &MainWindow::poserSurPlateau);
    connect(m_panneau, &PanneauProprietes::dimensionsDemandees, this, &MainWindow::surDimensionsDemandees);
    connect(m_panneau, &PanneauProprietes::positionDemandee, this, &MainWindow::surPositionDemandee);
    connect(m_panneau, &PanneauProprietes::orientationDemandee, this, &MainWindow::surOrientationDemandee);
    connect(m_panneau, &PanneauProprietes::parametresModifies, this, &MainWindow::surParametresModifies);
    connect(m_panneau, &PanneauProprietes::diametreDemande, this, &MainWindow::surDiametreDemande);
    connect(m_panneau, &PanneauProprietes::aimantationGrilleChangee, this,
            [this](double pas) { m_vue->definirAimantation(pas); });
    connect(m_panneau, &PanneauProprietes::aimantationAngleChangee, this,
            [this](double degres) { m_vue->definirAimantationAngle(degres); });
    connect(m_panneau, &PanneauProprietes::axeRotationChange, this,
            [this](AxeRotation axe) { m_vue->definirAxeRotation(axe); });
}

void MainWindow::creerMenus()
{
    // Fabrique une action, l'ajoute au menu et la relie à une fonction.
    auto action = [this](QMenu* menu, const QString& texte,
                         const QKeySequence& raccourci, auto fonction) {
        QAction* a = menu->addAction(texte);
        a->setShortcut(raccourci);
        connect(a, &QAction::triggered, this, fonction);
        return a;
    };

    QToolBar* barre = addToolBar(tr("Outils"));
    barre->setObjectName(QStringLiteral("barreOutils"));
    barre->setMovable(false);

    // --- Fichier ---
    QMenu* menuFichier = menuBar()->addMenu(tr("&Fichier"));
    action(menuFichier, tr("&Nouveau projet"), QKeySequence::New,
           [this]() { nouveauProjet(); });
    action(menuFichier, tr("&Ouvrir un projet…"), QKeySequence::Open,
           [this]() { ouvrirProjet(); });
    action(menuFichier, tr("&Enregistrer"), QKeySequence::Save,
           [this]() { enregistrerProjet(); });
    action(menuFichier, tr("Enregistrer &sous…"), QKeySequence("Ctrl+Shift+S"),
           [this]() { enregistrerProjetSous(); });
    menuFichier->addSeparator();
    action(menuFichier, tr("&Importer un fichier STEP…"), QKeySequence("Ctrl+I"),
           [this]() { importerSTEP(); });
    action(menuFichier, tr("&Exporter en STL…"), QKeySequence("Ctrl+E"),
           [this]() { exporterSTL(); });
    menuFichier->addSeparator();
    action(menuFichier, tr("&Quitter"), QKeySequence("Ctrl+Q"),
           [this]() { close(); });

    // --- Édition ---
    QMenu* menuEdition = menuBar()->addMenu(tr("&Édition"));
    m_actionAnnuler = action(menuEdition, tr("&Annuler"), QKeySequence::Undo,
                             [this]() { annuler(); });
    m_actionRefaire = action(menuEdition, tr("&Rétablir"), QKeySequence::Redo,
                             [this]() { refaire(); });
    menuEdition->addSeparator();
    action(menuEdition, tr("&Copier"), QKeySequence::Copy, [this]() { copierSelection(); });
    action(menuEdition, tr("Co&ller"), QKeySequence::Paste, [this]() { collerSelection(); });
    menuEdition->addSeparator();
    action(menuEdition, tr("&Déplacer / tourner…"), QKeySequence("M"),
           [this]() { deplacerSelection(); });
    action(menuEdition, tr("&Étirer une face…"), QKeySequence("E"),
           [this]() { etirerFace(); });
    QMenu* menuBool = menuEdition->addMenu(tr("&Combiner des pièces"));
    QAction* aUnion = action(menuBool, tr("&Union (assembler en une seule pièce)"), QKeySequence(),
                             [this]() { operationBooleenne(modelisation::OperationBooleenne::Union); });
    QAction* aSoustraction = action(menuBool, tr("&Soustraction (percer / découper)"), QKeySequence(),
                                    [this]() { operationBooleenne(modelisation::OperationBooleenne::Soustraction); });
    QAction* aIntersection = action(menuBool, tr("&Intersection (garder la partie commune)"), QKeySequence(),
                                    [this]() { operationBooleenne(modelisation::OperationBooleenne::Intersection); });
    aUnion->setText(tr("Union"));
    aUnion->setToolTip(tr("Union : assemble les pièces sélectionnées en une seule"));
    aSoustraction->setText(tr("Soustraire"));
    aSoustraction->setToolTip(tr("Soustraction : retire les autres pièces sélectionnées de la pièce de base (trous, découpes)"));
    aIntersection->setText(tr("Intersection"));
    aIntersection->setToolTip(tr("Intersection : ne garde que la partie commune aux pièces sélectionnées"));
    action(menuEdition, tr("Percer un &trou dans la face…"), QKeySequence("T"),
           [this]() { percerTrou(); });
    QAction* aDupliquer = action(menuEdition, tr("D&upliquer / réseau…"), QKeySequence("Ctrl+D"),
                                 [this]() { dupliquerSelection(); });
    aDupliquer->setToolTip(tr("Copier la sélection, une ou plusieurs fois, avec un décalage (Ctrl+D)"));
    QAction* aSymetrie = action(menuEdition, tr("S&ymétrie…"), QKeySequence(),
                                [this]() { symetrieSelection(); });
    aSymetrie->setToolTip(tr("Créer la copie symétrique de la sélection par rapport à un plan"));
    QAction* aEvider = action(menuEdition, tr("É&vider (coque)…"), QKeySequence(),
                              [this]() { eviderSelection(); });
    aEvider->setToolTip(tr("Creuser la pièce en gardant une paroi ; sélectionner la ou les faces à ouvrir (mode Face)"));
    action(menuEdition, tr("&Congé (arrondir des arêtes)…"), QKeySequence(),
           [this]() { congeOuChanfrein(false); });
    action(menuEdition, tr("C&hanfrein (couper des arêtes)…"), QKeySequence(),
           [this]() { congeOuChanfrein(true); });
    QAction* aSupprimer = action(menuEdition, tr("&Supprimer la sélection"),
                                 QKeySequence::Delete, [this]() { supprimerSelection(); });
    action(menuEdition, tr("&Tout supprimer"), QKeySequence("Ctrl+Del"),
           [this]() { toutSupprimer(); });

    // --- Créer ---
    QMenu* menuCreer = menuBar()->addMenu(tr("&Créer"));
    QAction* aCube = action(menuCreer, tr("&Cube…"), QKeySequence(),
                            [this]() { creerCube(); });
    QAction* aCylindre = action(menuCreer, tr("C&ylindre…"), QKeySequence(),
                                [this]() { creerCylindre(); });
    QAction* aSphere = action(menuCreer, tr("&Sphère…"), QKeySequence(),
                              [this]() { creerSphere(); });

    // --- Outils ---
    QMenu* menuOutils = menuBar()->addMenu(tr("&Outils"));
    action(menuOutils, tr("&Vérifier pour l'impression…"), QKeySequence(),
           [this]() { verifierImpression(); });

    // --- Esquisse ---
    QMenu* menuEsquisse = menuBar()->addMenu(tr("E&squisse"));
    m_actionNouvelleEsquisse = action(menuEsquisse, tr("&Nouvelle esquisse…"), QKeySequence("N"),
                                      [this]() { nouvelleEsquisse(); });
    m_actionNouvelleEsquisse->setToolTip(tr("Dessiner un profil 2D sur un plan ou sur une face plane sélectionnée (N)"));
    menuEsquisse->addSeparator();

    auto* groupeOutilEsquisse = new QActionGroup(this);
    groupeOutilEsquisse->setExclusive(true);
    const struct
    {
        QString       texte;
        QString       aide;
        OutilEsquisse outil;
    } outilsEsquisse[] = {
        {tr("Rectangle"), tr("Rectangle : cliquez deux coins opposés"), OutilEsquisse::Rectangle},
        {tr("Cercle"), tr("Cercle : cliquez le centre, puis un point du cercle"), OutilEsquisse::Cercle},
        {tr("Polygone"), tr("Polygone : cliquez les sommets ; Entrée ou clic sur le 1er sommet pour fermer"),
         OutilEsquisse::Polygone},
    };
    QList<QAction*> actionsOutilsEsquisse;
    for (const auto& o : outilsEsquisse)
    {
        QAction* a = menuEsquisse->addAction(o.texte);
        a->setCheckable(true);
        a->setToolTip(o.aide);
        groupeOutilEsquisse->addAction(a);
        const OutilEsquisse outil = o.outil;
        connect(a, &QAction::triggered, this, [this, outil]() { m_vue->definirOutilEsquisse(outil); });
        actionsOutilsEsquisse << a;
        m_actionsEnEsquisse << a;
    }
    actionsOutilsEsquisse.first()->setChecked(true);

    menuEsquisse->addSeparator();
    QAction* aEffacerProfil = action(menuEsquisse, tr("&Effacer le dernier profil"), QKeySequence(),
                                     [this]() { m_vue->effacerDernierProfil(); });
    QAction* aExtruder = action(menuEsquisse, tr("E&xtruder…"), QKeySequence("X"),
                                [this]() { extruderEsquisse(); });
    aExtruder->setToolTip(tr("Donner du volume aux profils dessinés (X)"));
    QAction* aQuitterEsquisse = action(menuEsquisse, tr("&Quitter l'esquisse"), QKeySequence(),
                                       [this]() { quitterEsquisse(); });
    m_actionsEnEsquisse << aEffacerProfil << aExtruder << aQuitterEsquisse;

    // --- Vue ---
    QMenu* menuVue = menuBar()->addMenu(tr("&Vue"));
    QAction* aDessus = action(menuVue, tr("&Dessus"), QKeySequence("1"),
                              [this]() { m_vue->orienterVue(V3d_Zpos); });
    QAction* aFace = action(menuVue, tr("&Face"), QKeySequence("2"),
                            [this]() { m_vue->orienterVue(V3d_Yneg); });
    QAction* aCote = action(menuVue, tr("&Côté"), QKeySequence("3"),
                            [this]() { m_vue->orienterVue(V3d_Xpos); });
    QAction* aIso = action(menuVue, tr("&Isométrique"), QKeySequence("4"),
                           [this]() { m_vue->orienterVue(V3d_XposYnegZpos); });
    menuVue->addSeparator();
    QAction* aRecadrer = action(menuVue, tr("Tout &recadrer"), QKeySequence("F"),
                                [this]() { m_vue->recadrer(); });

    action(menuVue, tr("&Plateau de l'imprimante…"), QKeySequence(),
           [this]() { reglerPlateau(); });

    action(menuVue, tr("&Disposition par défaut"), QKeySequence(),
           [this]() { dispositionParDefaut(); });

    QAction* aGrille = menuVue->addAction(tr("&Grille et axes"));
    aGrille->setCheckable(true);
    aGrille->setChecked(true);
    aGrille->setShortcut(QKeySequence("G"));
    connect(aGrille, &QAction::toggled, this,
            [this](bool visible) { m_vue->afficherGrille(visible); });

    // --- Aide ---
    QMenu* menuAide = menuBar()->addMenu(tr("&Aide"));
    // Langue de l'interface (prise en compte au prochain démarrage)
    QMenu* menuLangue = menuAide->addMenu(QStringLiteral("&Langue / Language"));
    auto* groupeLangue = new QActionGroup(this);
    groupeLangue->setExclusive(true);
    const QString langueActuelle = traduction::langueChoisie();
    const struct
    {
        const char* nom;
        const char* code;
    } langues[] = {{"Français", "fr"}, {"English", "en"}};
    for (const auto& l : langues)
    {
        QAction* a = menuLangue->addAction(QString::fromUtf8(l.nom));
        a->setCheckable(true);
        a->setChecked(langueActuelle == QLatin1String(l.code));
        groupeLangue->addAction(a);
        const QString code = QLatin1String(l.code);
        connect(a, &QAction::triggered, this, [this, code]() {
            traduction::enregistrerLangue(code);
            QMessageBox::information(
                this, QStringLiteral("FreeTable3D"),
                QStringLiteral("Redémarrez FreeTable3D pour appliquer la langue.\n"
                               "Restart FreeTable3D to apply the language."));
        });
    }
    menuAide->addSeparator();
    action(menuAide, tr("À &propos de FreeTable3D…"), QKeySequence(),
           [this]() { aPropos(); });

    // --- Barre d'outils ---
    barre->addAction(m_actionAnnuler);
    barre->addAction(m_actionRefaire);
    barre->addSeparator();
    barre->addAction(aCube);
    barre->addAction(aCylindre);
    barre->addAction(aSphere);
    barre->addSeparator();

    // Ce que le clic sélectionne
    auto* groupeMode = new QActionGroup(this);
    groupeMode->setExclusive(true);

    const struct
    {
        QString       texte;
        const char*   raccourci;
        ModeSelection mode;
    } modes[] = {
        {tr("Pièce"), "P", ModeSelection::Piece},
        {tr("Face"), "S", ModeSelection::Face},
        {tr("Arête"), "A", ModeSelection::Arete},
    };

    for (const auto& m : modes)
    {
        QAction* a = new QAction(m.texte, this);
        a->setCheckable(true);
        a->setShortcut(QKeySequence(m.raccourci));
        a->setToolTip(tr("Sélectionner : %1 (%2)").arg(m.texte, m.raccourci));
        groupeMode->addAction(a);
        const ModeSelection mode = m.mode;
        connect(a, &QAction::triggered, this, [this, mode]() { definirMode(mode); });
        m_actionsMode[static_cast<int>(m.mode)] = a;
        barre->addAction(a);
    }
    m_actionsMode[static_cast<int>(ModeSelection::Piece)]->setChecked(true);
    barre->addSeparator();

    QAction* aOutilRotation = new QAction(tr("Rotation"), this);
    aOutilRotation->setCheckable(true);
    aOutilRotation->setShortcut(QKeySequence("R"));
    aOutilRotation->setToolTip(tr("Rotation à la souris (R) : glisser horizontalement sur la pièce sélectionnée"));
    connect(aOutilRotation, &QAction::toggled, this,
            [this](bool actif) { m_vue->definirOutilRotation(actif); });
    barre->addAction(aOutilRotation);

    QAction* aPlateau = new QAction(tr("Sur le plateau"), this);
    aPlateau->setShortcut(QKeySequence("B"));
    aPlateau->setToolTip(tr("Poser la pièce sur le plateau (B)"));
    connect(aPlateau, &QAction::triggered, this, [this]() { poserSurPlateau(); });
    barre->addAction(aPlateau);

    QAction* aAPlat = new QAction(tr("Face à plat"), this);
    aAPlat->setShortcut(QKeySequence("L"));
    aAPlat->setToolTip(tr("Poser la face sélectionnée à plat sur le plateau (L)"));
    connect(aAPlat, &QAction::triggered, this, [this]() { poserFaceAPlat(); });
    barre->addAction(aAPlat);

    barre->addSeparator();
    barre->addAction(aDupliquer);
    barre->addAction(aSymetrie);
    barre->addAction(aEvider);
    QAction* aTrou = new QAction(tr("Trou"), this);
    aTrou->setToolTip(tr("Percer un trou cylindrique dans la face plane sélectionnée (T)"));
    connect(aTrou, &QAction::triggered, this, [this]() { percerTrou(); });
    barre->addAction(aTrou);
    QAction* aConge = new QAction(tr("Congé"), this);
    aConge->setToolTip(tr("Arrondir les arêtes (ou toutes les arêtes d'une face) sélectionnées"));
    connect(aConge, &QAction::triggered, this, [this]() { congeOuChanfrein(false); });
    QAction* aChanfrein = new QAction(tr("Chanfrein"), this);
    aChanfrein->setToolTip(tr("Couper en biseau les arêtes (ou toutes les arêtes d'une face) sélectionnées"));
    connect(aChanfrein, &QAction::triggered, this, [this]() { congeOuChanfrein(true); });
    barre->addAction(aConge);
    barre->addAction(aChanfrein);
    barre->addSeparator();
    barre->addAction(aUnion);
    barre->addAction(aSoustraction);
    barre->addAction(aIntersection);
    barre->addSeparator();
    barre->addAction(m_actionNouvelleEsquisse);
    for (QAction* a : actionsOutilsEsquisse)
        barre->addAction(a);
    barre->addAction(aEffacerProfil);
    barre->addAction(aExtruder);
    barre->addAction(aQuitterEsquisse);
    majEtatEsquisse(false);
    barre->addSeparator();
    barre->addAction(aSupprimer);
    barre->addSeparator();
    barre->addAction(aDessus);
    barre->addAction(aFace);
    barre->addAction(aCote);
    barre->addAction(aIso);
    barre->addAction(aRecadrer);
    barre->addAction(aGrille);

    // --- Icônes (barre d'outils compacte : le texte reste en infobulle) ---
    m_actionAnnuler->setIcon(icones::creer("annuler"));
    m_actionRefaire->setIcon(icones::creer("refaire"));
    aCube->setIcon(icones::creer("cube"));
    aCylindre->setIcon(icones::creer("cylindre"));
    aSphere->setIcon(icones::creer("sphere"));
    m_actionsMode[static_cast<int>(ModeSelection::Piece)]->setIcon(icones::creer("piece"));
    m_actionsMode[static_cast<int>(ModeSelection::Face)]->setIcon(icones::creer("face"));
    m_actionsMode[static_cast<int>(ModeSelection::Arete)]->setIcon(icones::creer("arete"));
    aOutilRotation->setIcon(icones::creer("rotation"));
    aPlateau->setIcon(icones::creer("plateau"));
    aAPlat->setIcon(icones::creer("faceaplat"));
    aTrou->setIcon(icones::creer("trou"));
    aDupliquer->setIcon(icones::creer("dupliquer"));
    aSymetrie->setIcon(icones::creer("symetrie"));
    aEvider->setIcon(icones::creer("evider"));
    aConge->setIcon(icones::creer("conge"));
    aChanfrein->setIcon(icones::creer("chanfrein"));
    aUnion->setIcon(icones::creer("union"));
    aSoustraction->setIcon(icones::creer("soustraire"));
    aIntersection->setIcon(icones::creer("intersection"));
    m_actionNouvelleEsquisse->setIcon(icones::creer("esquisse"));
    const char* nomsOutilsEsquisse[] = {"rectangle", "cercle", "polygone"};
    for (int i = 0; i < 3 && i < actionsOutilsEsquisse.size(); ++i)
        actionsOutilsEsquisse[i]->setIcon(icones::creer(nomsOutilsEsquisse[i]));
    aEffacerProfil->setIcon(icones::creer("effacer"));
    aExtruder->setIcon(icones::creer("extruder"));
    aQuitterEsquisse->setIcon(icones::creer("quitter"));
    aSupprimer->setIcon(icones::creer("supprimer"));
    aDessus->setIcon(icones::creer("dessus"));
    aFace->setIcon(icones::creer("devant"));
    aCote->setIcon(icones::creer("cote"));
    aIso->setIcon(icones::creer("iso"));
    aRecadrer->setIcon(icones::creer("recadrer"));
    aGrille->setIcon(icones::creer("grille"));

    barre->setToolButtonStyle(Qt::ToolButtonIconOnly);
    barre->setIconSize(QSize(26, 26));
    setWindowIcon(icones::creer("cube"));
}

// Pièce unique sélectionnée en mode Pièce (pour les champs numériques), sinon nullptr.
static const Piece* pieceUniqueSelectionnee(const MainWindow* fenetre, const Document& document,
                                            const std::vector<int>& ids, ModeSelection mode)
{
    Q_UNUSED(fenetre);
    if (ids.size() != 1 || mode != ModeSelection::Piece)
        return nullptr;
    return document.trouver(ids.front());
}

void MainWindow::surDimensionsDemandees(double x, double y, double z)
{
    const std::vector<int> ids = idsSelectionnes();
    if (!pieceUniqueSelectionnee(this, m_document, ids, m_mode))
        return;
    m_document.modifier(ids, [=](const TopoDS_Shape& forme) {
        return modelisation::redimensionner(forme, x, y, z);
    });
    rafraichirEnGardantSelection(ids);
}

void MainWindow::operationBooleenne(modelisation::OperationBooleenne operation)
{
    const std::vector<int> ids = idsSelectionnes();
    if (ids.size() < 2 || m_mode != ModeSelection::Piece)
    {
        QMessageBox::information(
            this, tr("Combiner des pièces"),
            tr("Passez en mode Pièce (touche P) et sélectionnez au moins deux pièces "
               "(Maj+clic, ou la liste des pièces).\n\n"
               "Les pièces doivent se chevaucher : déplacez-les d'abord l'une dans l'autre."));
        return;
    }

    // Pièce de base : pour une soustraction, c'est celle dont on retire les autres.
    int idBase = ids.front();
    if (operation == modelisation::OperationBooleenne::Soustraction)
    {
        QStringList noms;
        for (int id : ids)
        {
            const Piece* piece = m_document.trouver(id);
            noms << QString::fromStdString(piece ? piece->nom : "?");
        }
        bool valide = false;
        const QString choix = QInputDialog::getItem(
            this, tr("Soustraction"),
            tr("Pièce dont on retire les autres (elle est conservée, les autres disparaissent) :"),
            noms, 0, false, &valide);
        if (!valide)
            return;
        idBase = ids[static_cast<size_t>(noms.indexOf(choix))];
    }

    const Piece* base = m_document.trouver(idBase);
    std::vector<TopoDS_Shape> outils;
    for (int id : ids)
    {
        if (id != idBase)
            outils.push_back(m_document.trouver(id)->forme);
    }

    TopoDS_Shape resultat;
    if (!base || !modelisation::booleen(base->forme, outils, operation, resultat))
    {
        QMessageBox::warning(
            this, tr("Combiner des pièces"),
            tr("L'opération n'a donné aucun solide. Vérifiez que les pièces se chevauchent "
               "(l'intersection de pièces séparées est vide)."));
        return;
    }

    m_document.remplacer(idBase, resultat, ids);
    rafraichirEnGardantSelection({idBase});
}

void MainWindow::congeOuChanfrein(bool chanfrein)
{
    const QString titre = chanfrein ? tr("Chanfrein") : tr("Congé");

    const std::vector<ElementSelectionne> selection = m_vue->selection();
    std::vector<TopoDS_Shape> elements;
    for (const ElementSelectionne& e : selection)
    {
        if (!e.forme.IsNull() && (e.forme.ShapeType() == TopAbs_EDGE || e.forme.ShapeType() == TopAbs_FACE))
            elements.push_back(e.forme);
    }

    const std::vector<int> ids = idsSelectionnes();
    if (elements.empty() || ids.size() != 1)
    {
        QMessageBox::information(
            this, titre,
            tr("Passez en mode Arête (touche A) et sélectionnez une ou plusieurs arêtes "
               "(Maj+clic), ou en mode Face (touche S) pour traiter toutes les arêtes d'une face.\n\n"
               "Toutes les arêtes doivent appartenir à la même pièce."));
        return;
    }

    const std::vector<TopoDS_Shape> aretes = modelisation::aretesDe(elements);

    bool valide = false;
    const double distance = QInputDialog::getDouble(
        this, titre,
        chanfrein ? tr("Largeur du chanfrein (mm) :") : tr("Rayon de l'arrondi (mm) :"),
        2.0, 0.01, 10000.0, 2, &valide);
    if (!valide)
        return;

    const Piece* piece = m_document.trouver(ids.front());
    TopoDS_Shape resultat;
    if (!piece || !modelisation::congeOuChanfrein(piece->forme, aretes, distance, chanfrein, resultat))
    {
        QMessageBox::warning(
            this, titre,
            tr("L'opération a échoué : la valeur est probablement trop grande pour ces arêtes. "
               "Essayez une valeur plus petite."));
        return;
    }

    m_document.modifier(ids, [resultat](const TopoDS_Shape&) { return resultat; });
    rafraichir();
    statusBar()->showMessage(tr("%1 appliqué sur %2 arête(s).").arg(titre).arg(static_cast<int>(aretes.size())));
}

void MainWindow::majEtatEsquisse(bool actif)
{
    m_actionNouvelleEsquisse->setEnabled(!actif);
    for (QAction* a : m_actionsEnEsquisse)
        a->setEnabled(actif);
}

void MainWindow::nouvelleEsquisse()
{
    if (m_vue->esquisseActive())
        return;

    // Plans proposés : la face plane sélectionnée (si elle existe), puis les trois plans de base.
    gp_Pnt pointFace;
    gp_Vec normaleFace;
    int    idFace = 0;
    const std::vector<ElementSelectionne> selection = m_vue->selection();
    if (selection.size() == 1 && !selection.front().forme.IsNull()
        && selection.front().forme.ShapeType() == TopAbs_FACE
        && modelisation::reperFacePlane(selection.front().forme, pointFace, normaleFace))
    {
        idFace = selection.front().idPiece;
    }

    QStringList choix;
    if (idFace != 0)
        choix << tr("Face plane sélectionnée");
    choix << tr("Plan XY (le plateau)") << tr("Plan XZ (vue de face)") << tr("Plan YZ (vue de côté)");

    bool valide = false;
    const QString plan = QInputDialog::getItem(this, tr("Nouvelle esquisse"),
                                               tr("Dessiner sur quel plan ?"), choix, 0, false, &valide);
    if (!valide)
        return;

    gp_Pnt origine(0.0, 0.0, 0.0);
    gp_Dir normale(0.0, 0.0, 1.0);
    m_idPieceEsquisse = 0;

    const int index = static_cast<int>(choix.indexOf(plan)) - (idFace != 0 ? 1 : 0);
    if (index < 0)
    {
        origine = pointFace;
        normale = gp_Dir(normaleFace);
        m_idPieceEsquisse = idFace;
    }
    else if (index == 1)
    {
        normale = gp_Dir(0.0, -1.0, 0.0);
    }
    else if (index == 2)
    {
        normale = gp_Dir(1.0, 0.0, 0.0);
    }

    m_vue->demarrerEsquisse(origine, normale);
    majEtatEsquisse(true);
    statusBar()->showMessage(
        tr("Esquisse : cliquez pour placer des points (Rectangle : 2 coins, Cercle : centre puis rayon, "
           "Polygone : sommets puis Entrée). Échap annule le tracé en cours. Puis Extruder (X)."));
}

void MainWindow::quitterEsquisse()
{
    m_vue->arreterEsquisse();
    majEtatEsquisse(false);
    statusBar()->clearMessage();
}

void MainWindow::extruderEsquisse()
{
    const std::vector<TopoDS_Shape> profils = m_vue->profilsEsquisse();
    if (profils.empty())
    {
        QMessageBox::information(
            this, tr("Extruder"),
            tr("Dessinez d'abord au moins un profil fermé (rectangle, cercle ou polygone)."));
        return;
    }

    bool valide = false;
    const double distance = QInputDialog::getDouble(
        this, tr("Extruder"),
        tr("Hauteur d'extrusion (mm) :\npositive = dans le sens de la normale du plan, négative = à l'opposé"),
        10.0, -10000.0, 10000.0, 2, &valide);
    if (!valide || distance == 0.0)
        return;

    QStringList operations;
    operations << tr("Nouvelle pièce") << tr("Ajouter à une pièce (union)")
               << tr("Découper dans une pièce (creuser)");
    const QString operation = QInputDialog::getItem(this, tr("Extruder"), tr("Résultat :"),
                                                    operations, 0, false, &valide);
    if (!valide)
        return;
    const int genre = static_cast<int>(operations.indexOf(operation));

    const gp_Vec normale(m_vue->normaleEsquisse());
    // Creuser va à l'opposé de la normale (vers l'intérieur d'une pièce quand on esquisse sur sa face).
    const gp_Vec vecteur = normale * (genre == 2 ? -distance : distance);

    TopoDS_Shape prisme;
    if (!modelisation::extruderProfils(profils, vecteur, prisme))
    {
        QMessageBox::warning(this, tr("Extruder"), tr("L'extrusion a échoué."));
        return;
    }

    if (genre == 0)
    {
        // La pièce garde son esquisse et sa hauteur : on pourra les modifier dans le panneau.
        auto parametres = std::make_shared<Parametrique>();
        parametres->esquisse = m_vue->esquisseCourante();
        parametres->hauteur  = distance;
        quitterEsquisse();
        m_document.ajouter(prisme, "Extrusion", parametres);
        rafraichir();
        m_vue->recadrer();
        return;
    }

    if (m_document.vide())
    {
        QMessageBox::information(this, tr("Extruder"),
                                 tr("Il n'y a aucune pièce : choisissez « Nouvelle pièce »."));
        return;
    }

    // Pièce de base : celle de la face d'esquisse, sinon l'unique pièce, sinon au choix.
    int idBase = 0;
    if (m_idPieceEsquisse != 0 && m_document.trouver(m_idPieceEsquisse))
    {
        idBase = m_idPieceEsquisse;
    }
    else if (m_document.pieces().size() == 1)
    {
        idBase = m_document.pieces().front().id;
    }
    else
    {
        QStringList noms;
        for (const Piece& piece : m_document.pieces())
            noms << QString::fromStdString(piece.nom);
        const QString choix = QInputDialog::getItem(this, tr("Extruder"), tr("Dans quelle pièce ?"),
                                                    noms, 0, false, &valide);
        if (!valide)
            return;
        idBase = m_document.pieces()[static_cast<size_t>(noms.indexOf(choix))].id;
    }

    const Piece* base = m_document.trouver(idBase);
    TopoDS_Shape resultat;
    const auto type = genre == 1 ? modelisation::OperationBooleenne::Union
                                 : modelisation::OperationBooleenne::Soustraction;
    if (!base || !modelisation::booleen(base->forme, {prisme}, type, resultat))
    {
        QMessageBox::warning(this, tr("Extruder"),
                             tr("L'opération n'a donné aucun solide. Vérifiez que l'extrusion touche la pièce "
                                "(pour creuser : le sens est opposé à la normale du plan)."));
        return;
    }

    m_document.modifier({idBase}, [resultat](const TopoDS_Shape&) { return resultat; });
    quitterEsquisse();
    rafraichirEnGardantSelection({idBase});
}

void MainWindow::aPropos()
{
    const QString texte = tr(
        "<h2>FreeTable3D %1</h2>"
        "<p>Logiciel libre de CAO pour l'impression 3D,<br>"
        "écrit en C++ avec OpenCascade et Qt 6.</p>"
        "<p>Copyright © 2026 KREUDER Olivier<br>"
        "Contact : <a href=\"mailto:okpcbrumath@orange.fr\">okpcbrumath@orange.fr</a><br>"
        "Code source : <a href=\"https://github.com/morvux/freetable3d\">github.com/morvux/freetable3d</a></p>"
        "<p><b>Licence :</b> GNU GPL version 3.<br>"
        "Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier "
        "selon les termes de la Licence Publique Générale GNU, telle que publiée par la "
        "Free Software Foundation, version 3 de la licence.</p>"
        "<p>Ce programme est distribué dans l'espoir qu'il sera utile, mais "
        "<b>sans aucune garantie</b>, sans même la garantie implicite de commercialisation "
        "ou d'adéquation à un usage particulier.</p>"
        "<p>Texte de la licence : "
        "<a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">gnu.org/licenses/gpl-3.0.html</a></p>")
                              .arg(QStringLiteral(FREETABLE3D_VERSION));

    QMessageBox boite(this);
    boite.setWindowTitle(tr("À propos de FreeTable3D"));
    boite.setIconPixmap(windowIcon().pixmap(64, 64));
    boite.setTextFormat(Qt::RichText);
    boite.setTextInteractionFlags(Qt::TextBrowserInteraction);
    boite.setText(texte);
    boite.setStandardButtons(QMessageBox::Ok);
    boite.exec();
}

void MainWindow::copierSelection()
{
    const std::vector<int> ids = idsSelectionnes();
    std::vector<Piece>     copies;
    for (int id : ids)
        if (const Piece* piece = m_document.trouver(id))
            copies.push_back(*piece);

    if (copies.empty())
    {
        statusBar()->showMessage(tr("Rien à copier : sélectionnez d'abord une ou plusieurs pièces."), 4000);
        return;
    }
    m_presspapiers = copies;
    m_nbCollages   = 0;
    statusBar()->showMessage(tr("%1 pièce(s) copiée(s). Ctrl+V pour coller.").arg(static_cast<int>(copies.size())), 4000);
}

void MainWindow::collerSelection()
{
    if (m_presspapiers.empty())
    {
        statusBar()->showMessage(tr("Rien à coller : copiez d'abord une pièce (Ctrl+C)."), 4000);
        return;
    }

    // Chaque collage est décalé de 10 mm de plus en X et en Y, pour voir la copie.
    ++m_nbCollages;
    gp_Trsf t;
    t.SetTranslation(gp_Vec(10.0 * m_nbCollages, 10.0 * m_nbCollages, 0.0));

    const std::vector<int> nouvelles = m_document.coller(m_presspapiers, t);
    rafraichirEnGardantSelection(nouvelles);
    statusBar()->showMessage(tr("%1 pièce(s) collée(s).").arg(static_cast<int>(nouvelles.size())), 4000);
}

void MainWindow::dupliquerSelection()
{
    const std::vector<int> ids = idsSelectionnes();
    if (ids.empty())
    {
        QMessageBox::information(this, tr("Dupliquer"), tr("Sélectionnez d'abord une ou plusieurs pièces."));
        return;
    }

    QDialog dialogue(this);
    dialogue.setWindowTitle(tr("Dupliquer / réseau"));
    auto* mise = new QVBoxLayout(&dialogue);
    auto* formulaire = new QFormLayout;

    auto* nombre = new QSpinBox;
    nombre->setRange(1, 200);
    nombre->setValue(1);
    formulaire->addRow(tr("Nombre de copies"), nombre);

    auto champ = [](double valeur) {
        auto* c = new QDoubleSpinBox;
        c->setRange(-100000.0, 100000.0);
        c->setDecimals(2);
        c->setSuffix(QStringLiteral(" mm"));
        c->setValue(valeur);
        return c;
    };
    auto* dx = champ(20.0);
    auto* dy = champ(0.0);
    auto* dz = champ(0.0);
    formulaire->addRow(tr("Décalage entre copies : X"), dx);
    formulaire->addRow(tr("Y"), dy);
    formulaire->addRow(tr("Z"), dz);
    mise->addLayout(formulaire);

    auto* boutons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(boutons, &QDialogButtonBox::accepted, &dialogue, &QDialog::accept);
    connect(boutons, &QDialogButtonBox::rejected, &dialogue, &QDialog::reject);
    mise->addWidget(boutons);
    if (dialogue.exec() != QDialog::Accepted)
        return;

    std::vector<gp_Trsf> transformations;
    for (int i = 1; i <= nombre->value(); ++i)
    {
        gp_Trsf t;
        t.SetTranslation(gp_Vec(dx->value() * i, dy->value() * i, dz->value() * i));
        transformations.push_back(t);
    }

    const std::vector<int> copies = m_document.dupliquer(ids, transformations, true);
    rafraichirEnGardantSelection(copies);
    statusBar()->showMessage(tr("%1 copie(s) créée(s).").arg(static_cast<int>(copies.size())));
}

void MainWindow::symetrieSelection()
{
    const std::vector<int> ids = idsSelectionnes();
    if (ids.empty())
    {
        QMessageBox::information(this, tr("Symétrie"), tr("Sélectionnez d'abord une ou plusieurs pièces."));
        return;
    }

    // Position proposée : le bord de la pièce, pour que la copie vienne se coller contre elle.
    const Piece* premiere = m_document.trouver(ids.front());
    const modelisation::Mesures m = modelisation::mesurer(premiere->forme);

    QDialog dialogue(this);
    dialogue.setWindowTitle(tr("Symétrie"));
    auto* mise = new QVBoxLayout(&dialogue);
    auto* formulaire = new QFormLayout;

    auto* plan = new QComboBox;
    plan->addItem(tr("Plan YZ (miroir selon X)"));
    plan->addItem(tr("Plan XZ (miroir selon Y)"));
    plan->addItem(tr("Plan XY (miroir selon Z)"));
    formulaire->addRow(tr("Plan de symétrie"), plan);

    auto* position = new QDoubleSpinBox;
    position->setRange(-100000.0, 100000.0);
    position->setDecimals(2);
    position->setSuffix(QStringLiteral(" mm"));
    formulaire->addRow(tr("Position du plan"), position);

    const double bords[3] = {m.xmax, m.ymax, m.zmax};
    auto majPosition = [&]() { position->setValue(bords[plan->currentIndex()]); };
    majPosition();
    connect(plan, &QComboBox::currentIndexChanged, &dialogue, [&](int) { majPosition(); });

    mise->addLayout(formulaire);
    mise->addWidget(new QLabel(tr("La pièce d'origine est conservée ; supprimez-la si besoin.")));
    auto* boutons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(boutons, &QDialogButtonBox::accepted, &dialogue, &QDialog::accept);
    connect(boutons, &QDialogButtonBox::rejected, &dialogue, &QDialog::reject);
    mise->addWidget(boutons);
    if (dialogue.exec() != QDialog::Accepted)
        return;

    const double p = position->value();
    gp_Trsf miroir;
    switch (plan->currentIndex())
    {
    case 0: miroir.SetMirror(gp_Ax2(gp_Pnt(p, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0))); break;
    case 1: miroir.SetMirror(gp_Ax2(gp_Pnt(0.0, p, 0.0), gp_Dir(0.0, 1.0, 0.0))); break;
    default: miroir.SetMirror(gp_Ax2(gp_Pnt(0.0, 0.0, p), gp_Dir(0.0, 0.0, 1.0))); break;
    }

    const std::vector<int> copies = m_document.dupliquer(ids, {miroir}, false);
    rafraichirEnGardantSelection(copies);
}

void MainWindow::eviderSelection()
{
    std::vector<TopoDS_Shape> faces;
    for (const ElementSelectionne& e : m_vue->selection())
    {
        if (!e.forme.IsNull() && e.forme.ShapeType() == TopAbs_FACE)
            faces.push_back(e.forme);
    }
    const std::vector<int> ids = idsSelectionnes();
    if (faces.empty() || ids.size() != 1)
    {
        QMessageBox::information(
            this, tr("Évider"),
            tr("Passez en mode Face (touche S) et sélectionnez la ou les faces à ouvrir "
               "(Maj+clic), toutes sur la même pièce : le reste de la pièce est creusé en gardant "
               "une paroi."));
        return;
    }

    bool valide = false;
    const double epaisseur = QInputDialog::getDouble(this, tr("Évider"), tr("Épaisseur de la paroi (mm) :"),
                                                     2.0, 0.05, 1000.0, 2, &valide);
    if (!valide)
        return;

    const Piece* piece = m_document.trouver(ids.front());
    TopoDS_Shape resultat;
    if (!piece || !modelisation::evider(piece->forme, faces, epaisseur, resultat))
    {
        QMessageBox::warning(this, tr("Évider"),
                             tr("L'évidement a échoué : l'épaisseur est probablement trop grande pour "
                                "cette pièce. Essayez une valeur plus petite."));
        return;
    }

    m_document.modifier(ids, [resultat](const TopoDS_Shape&) { return resultat; });
    rafraichir();
}

void MainWindow::percerTrou()
{
    const std::vector<ElementSelectionne> selection = m_vue->selection();
    gp_Pnt point;
    gp_Vec normale;
    if (selection.size() != 1 || selection.front().forme.IsNull()
        || selection.front().forme.ShapeType() != TopAbs_FACE
        || !modelisation::reperFacePlane(selection.front().forme, point, normale))
    {
        QMessageBox::information(
            this, tr("Percer un trou"),
            tr("Passez en mode Face (touche S) et sélectionnez une seule face plane : "
               "le trou sera percé depuis cette face."));
        return;
    }

    QDialog dialogue(this);
    dialogue.setWindowTitle(tr("Percer un trou"));
    auto* mise = new QVBoxLayout(&dialogue);
    auto* formulaire = new QFormLayout;

    auto champ = [](double valeur, double min, double max) {
        auto* c = new QDoubleSpinBox;
        c->setRange(min, max);
        c->setDecimals(2);
        c->setSuffix(QStringLiteral(" mm"));
        c->setValue(valeur);
        return c;
    };
    QSettings reglages("FreeTable3D", "FreeTable3D");
    auto* diametre = champ(reglages.value("trou/diametre", 5.0).toDouble(), 0.01, 10000.0);
    auto* profondeur = champ(reglages.value("trou/profondeur", 10.0).toDouble(), 0.01, 10000.0);
    auto* traversant = new QCheckBox(tr("Trou traversant"));
    auto* decalU = champ(0.0, -10000.0, 10000.0);
    auto* decalV = champ(0.0, -10000.0, 10000.0);
    connect(traversant, &QCheckBox::toggled, profondeur, &QWidget::setDisabled);

    formulaire->addRow(tr("Diamètre"), diametre);
    formulaire->addRow(tr("Profondeur"), profondeur);
    formulaire->addRow(QString(), traversant);
    formulaire->addRow(tr("Décalage / centre de la face (horizontal)"), decalU);
    formulaire->addRow(tr("Décalage / centre de la face (vertical)"), decalV);
    mise->addLayout(formulaire);

    auto* boutons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(boutons, &QDialogButtonBox::accepted, &dialogue, &QDialog::accept);
    connect(boutons, &QDialogButtonBox::rejected, &dialogue, &QDialog::reject);
    mise->addWidget(boutons);

    if (dialogue.exec() != QDialog::Accepted)
        return;
    reglages.setValue("trou/diametre", diametre->value());
    reglages.setValue("trou/profondeur", profondeur->value());

    const int idPiece = selection.front().idPiece;
    const TopoDS_Shape face = selection.front().forme;
    const Piece* piece = m_document.trouver(idPiece);
    TopoDS_Shape resultat;
    std::string raison;
    if (!piece || !modelisation::percer(piece->forme, face, diametre->value(),
                                        traversant->isChecked() ? 0.0 : profondeur->value(),
                                        decalU->value(), decalV->value(), resultat, &raison))
    {
        QMessageBox::warning(this, tr("Percer un trou"),
                             tr("Le perçage a échoué : %1.").arg(QCoreApplication::translate("Modelisation", raison.c_str())));
        return;
    }

    m_document.modifier({idPiece}, [resultat](const TopoDS_Shape&) { return resultat; });
    rafraichir();
}

void MainWindow::verifierImpression()
{
    if (m_document.vide())
    {
        QMessageBox::information(this, tr("Vérifier pour l'impression"), tr("Le document est vide."));
        return;
    }

    // Taille du plateau de l'imprimante (mémorisée d'une fois sur l'autre)
    QSettings reglages("FreeTable3D", "FreeTable3D");
    QDialog dialogue(this);
    dialogue.setWindowTitle(tr("Vérifier pour l'impression"));
    auto* mise = new QVBoxLayout(&dialogue);
    auto* formulaire = new QFormLayout;
    auto champ = [&reglages](const char* cle, double defaut) {
        auto* c = new QDoubleSpinBox;
        c->setRange(1.0, 10000.0);
        c->setDecimals(0);
        c->setSuffix(QStringLiteral(" mm"));
        c->setValue(reglages.value(cle, defaut).toDouble());
        return c;
    };
    auto* px = champ("plateau/x", 220.0);
    auto* py = champ("plateau/y", 220.0);
    auto* pz = champ("plateau/z", 250.0);
    formulaire->addRow(tr("Plateau : largeur X"), px);
    formulaire->addRow(tr("Plateau : profondeur Y"), py);
    formulaire->addRow(tr("Hauteur maximale Z"), pz);
    mise->addLayout(formulaire);
    mise->addWidget(new QLabel(tr("Les pièces sélectionnées sont vérifiées, ou toutes s'il n'y a pas de sélection.")));
    auto* boutons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(boutons, &QDialogButtonBox::accepted, &dialogue, &QDialog::accept);
    connect(boutons, &QDialogButtonBox::rejected, &dialogue, &QDialog::reject);
    mise->addWidget(boutons);
    if (dialogue.exec() != QDialog::Accepted)
        return;
    reglages.setValue("plateau/x", px->value());
    reglages.setValue("plateau/y", py->value());
    reglages.setValue("plateau/z", pz->value());
    appliquerPlateau();

    const std::vector<int> ids = idsSelectionnes();
    const QLocale lieu;
    auto n = [&lieu](double v, int d = 1) { return lieu.toString(v, 'f', d); };

    QStringList rapport;
    for (const Piece& piece : m_document.pieces())
    {
        if (!ids.empty() && std::find(ids.begin(), ids.end(), piece.id) == ids.end())
            continue;

        const modelisation::Verification v = modelisation::verifier(piece.forme);
        QStringList lignes;
        lignes << QStringLiteral("<b>%1</b> — %2 × %3 × %4 mm")
                      .arg(QString::fromStdString(piece.nom), n(v.dx), n(v.dy), n(v.dz));

        lignes << (v.valide ? tr("✔ Géométrie valide") : tr("✘ Géométrie invalide : l'export STL peut donner un modèle troué"));
        if (v.nbSolides == 0)
            lignes << tr("✘ Aucun solide (surface seule) : impossible à imprimer");
        else if (v.nbSolides > 1)
            lignes << tr("⚠ %1 solides séparés dans cette pièce").arg(v.nbSolides);
        if (v.nbSolides > 0 && v.volume <= 0.0)
            lignes << tr("✘ Volume nul ou négatif : faces à l'envers ou pièce non fermée");
        else if (v.nbSolides > 0)
            lignes << tr("✔ Volume %1 cm³").arg(n(v.volume / 1000.0, 2));

        const double X = px->value(), Y = py->value(), Z = pz->value();

        // Position : le plateau est centré sur l'origine
        const modelisation::Mesures pos = modelisation::mesurer(piece.forme);
        if (pos.boiteValide
            && (pos.xmin < -X / 2.0 - 1e-6 || pos.xmax > X / 2.0 + 1e-6
                || pos.ymin < -Y / 2.0 - 1e-6 || pos.ymax > Y / 2.0 + 1e-6))
            lignes << tr("⚠ Dépasse du plateau à sa position actuelle (déplacez-la ou « Centrer sur le plateau »)");
        if (pos.boiteValide && pos.zmin < -1e-6)
            lignes << tr("⚠ Passe sous le plateau (« Poser sur le plateau »)");
        const bool tient = (v.dx <= X && v.dy <= Y && v.dz <= Z);
        const bool tientTournee = (v.dy <= X && v.dx <= Y && v.dz <= Z);
        if (tient)
            lignes << tr("✔ Tient sur le plateau (%1 × %2 × %3 mm)").arg(n(X, 0), n(Y, 0), n(Z, 0));
        else if (tientTournee)
            lignes << tr("⚠ Tient sur le plateau après une rotation de 90° autour de Z");
        else
            lignes << tr("✘ Trop grand pour le plateau (%1 × %2 × %3 mm)").arg(n(X, 0), n(Y, 0), n(Z, 0));

        rapport << lignes.join("<br>");
    }

    QMessageBox::information(this, tr("Vérifier pour l'impression"), rapport.join("<br><br>"));
}

void MainWindow::surParametresModifies(const Parametrique& parametres)
{
    const std::vector<int> ids = idsSelectionnes();
    if (ids.size() != 1)
        return;
    const Piece* piece = m_document.trouver(ids.front());
    if (!piece || !piece->parametres)
        return;

    TopoDS_Shape nouvelle;
    if (!modelisation::genererParametrique(parametres, nouvelle))
    {
        QMessageBox::warning(this, tr("Esquisse"),
                             tr("Ces valeurs ne donnent pas de forme valide (dimension nulle ou "
                                "profil dégénéré)."));
        rafraichirEnGardantSelection(ids); // remet l'affichage sur les valeurs actuelles
        return;
    }

    // La pièce a pu être déplacée ou tournée depuis sa création : on réapplique son placement.
    const TopoDS_Shape placee = BRepBuilderAPI_Transform(nouvelle, piece->placement, true).Shape();
    m_document.redefinirParametres(ids.front(), placee, std::make_shared<Parametrique>(parametres));
    rafraichirEnGardantSelection(ids);
}

void MainWindow::centrerSurPlateau()
{
    const std::vector<int> ids = idsSelectionnes();
    const Piece* piece = pieceUniqueSelectionnee(this, m_document, ids, m_mode);
    if (!piece)
        return;
    const modelisation::Mesures m = modelisation::mesurer(piece->forme);
    if (!m.boiteValide)
        return;
    appliquerTransformation(ids, -(m.xmin + m.xmax) / 2.0, -(m.ymin + m.ymax) / 2.0, 0.0,
                            0.0, 0.0, 0.0);
    rafraichirEnGardantSelection(ids);
}

void MainWindow::surPositionDemandee(double xmin, double ymin, double zmin)
{
    const std::vector<int> ids = idsSelectionnes();
    const Piece* piece = pieceUniqueSelectionnee(this, m_document, ids, m_mode);
    if (!piece)
        return;
    const modelisation::Mesures m = modelisation::mesurer(piece->forme);
    if (!m.boiteValide)
        return;
    appliquerTransformation(ids, xmin - m.xmin, ymin - m.ymin, zmin - m.zmin, 0.0, 0.0, 0.0);
    rafraichirEnGardantSelection(ids);
}

void MainWindow::surOrientationDemandee(double rx, double ry, double rz)
{
    const std::vector<int> ids = idsSelectionnes();
    const Piece* piece = pieceUniqueSelectionnee(this, m_document, ids, m_mode);
    if (!piece)
        return;

    // Rotation qui amène l'orientation actuelle sur l'orientation demandée
    const gp_Quaternion cible = modelisation::quaternionRotation(rx, ry, rz);
    const gp_Quaternion delta = cible.Multiplied(piece->orientation.Inverted());
    double drx = 0.0, dry = 0.0, drz = 0.0;
    modelisation::anglesEuler(delta, drx, dry, drz);
    appliquerTransformation(ids, 0.0, 0.0, 0.0, drx, dry, drz);
    rafraichirEnGardantSelection(ids);
}

void MainWindow::surDiametreDemande(double diametre)
{
    const std::vector<int> ids = idsSelectionnes();
    if (!pieceUniqueSelectionnee(this, m_document, ids, m_mode))
        return;
    m_document.modifier(ids, [diametre](const TopoDS_Shape& forme) {
        return modelisation::changerDiametre(forme, diametre);
    });
    rafraichirEnGardantSelection(ids);
}

void MainWindow::appliquerTransformation(const std::vector<int>& ids,
                                         double dx, double dy, double dz,
                                         double rx, double ry, double rz)
{
    m_document.transformer(ids, [=](const TopoDS_Shape& forme) {
        return modelisation::matriceTransformation(forme, dx, dy, dz, rx, ry, rz);
    });
    if (rx != 0.0 || ry != 0.0 || rz != 0.0)
        m_document.composerOrientation(ids, modelisation::quaternionRotation(rx, ry, rz));
}

void MainWindow::definirMode(ModeSelection mode)
{
    m_mode = mode;
    m_actionsMode[static_cast<int>(mode)]->setChecked(true);
    m_vue->definirModeSelection(mode);
    surSelectionChangee();
}

void MainWindow::rafraichir()
{
    m_vue->afficherDocument(m_document);

    m_miseAJourListe = true;
    m_liste->clear();
    for (const Piece& piece : m_document.pieces())
    {
        auto* element = new QListWidgetItem(QString::fromStdString(piece.nom), m_liste);
        element->setData(Qt::UserRole, piece.id);
    }
    m_miseAJourListe = false;

    m_modifie = true;
    majTitre();

    m_actionAnnuler->setEnabled(m_document.peutAnnuler());
    m_actionRefaire->setEnabled(m_document.peutRefaire());

    surSelectionChangee();
}

std::vector<int> MainWindow::idsSelectionnes() const
{
    std::vector<int> ids;
    for (const ElementSelectionne& element : m_vue->selection())
        ids.push_back(element.idPiece);

    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

void MainWindow::surSelectionChangee()
{
    const std::vector<ElementSelectionne> selection = m_vue->selection();

    // Reflète la sélection dans la liste des pièces
    m_miseAJourListe = true;
    m_liste->clearSelection();
    for (const ElementSelectionne& element : selection)
    {
        for (int i = 0; i < m_liste->count(); ++i)
        {
            if (m_liste->item(i)->data(Qt::UserRole).toInt() == element.idPiece)
                m_liste->item(i)->setSelected(true);
        }
    }
    m_miseAJourListe = false;

    m_panneau->afficher(selection, m_document);

    // Message d'état
    if (selection.empty())
    {
        statusBar()->showMessage(
            tr("%1 pièce(s). Clic = sélectionner, Maj+clic = ajouter ; "
               "glisser une sélection = la déplacer (mode Pièce) ou l'étirer (mode Face).")
                .arg(static_cast<int>(m_document.pieces().size())));
    }
    else if (selection.size() == 1)
    {
        const Piece* piece = m_document.trouver(selection.front().idPiece);
        const QString nom = piece ? QString::fromStdString(piece->nom) : tr("?");
        statusBar()->showMessage(nom + " : " + selection.front().description);
    }
    else
    {
        statusBar()->showMessage(
            tr("%1 éléments sélectionnés").arg(static_cast<int>(selection.size())));
    }
}

void MainWindow::surSelectionListe()
{
    if (m_miseAJourListe)
        return;

    const QList<QListWidgetItem*> elements = m_liste->selectedItems();
    if (elements.isEmpty())
        return;
    const int id = elements.first()->data(Qt::UserRole).toInt();

    // Choisir une pièce dans la liste implique le mode Pièce.
    if (m_mode != ModeSelection::Piece)
        definirMode(ModeSelection::Piece);

    m_vue->selectionnerPiece(id);
    surSelectionChangee();
}

void MainWindow::ajouterPiece(const TopoDS_Shape& forme, const QString& nom)
{
    m_document.ajouter(forme, nom.toStdString());
    rafraichir();
    m_vue->recadrer();
}

void MainWindow::creerCube()
{
    DialogueDimensions dialogue(tr("Créer un cube"),
                                {tr("Longueur X (mm)"), tr("Largeur Y (mm)"), tr("Hauteur Z (mm)")},
                                {20.0, 20.0, 20.0}, this);
    if (dialogue.exec() != QDialog::Accepted)
        return;

    const QVector<double> v = dialogue.valeurs();
    ajouterPiece(modelisation::creerCube(v[0], v[1], v[2], v[3], v[4], v[5]), tr("Cube"));
}

void MainWindow::creerCylindre()
{
    DialogueDimensions dialogue(tr("Créer un cylindre"),
                                {tr("Rayon (mm)"), tr("Hauteur (mm)")},
                                {10.0, 30.0}, this);
    if (dialogue.exec() != QDialog::Accepted)
        return;

    const QVector<double> v = dialogue.valeurs();
    ajouterPiece(modelisation::creerCylindre(v[0], v[1], v[2], v[3], v[4]), tr("Cylindre"));
}

void MainWindow::creerSphere()
{
    DialogueDimensions dialogue(tr("Créer une sphère"),
                                {tr("Rayon (mm)")},
                                {15.0}, this);
    if (dialogue.exec() != QDialog::Accepted)
        return;

    const QVector<double> v = dialogue.valeurs();
    ajouterPiece(modelisation::creerSphere(v[0], v[1], v[2], v[3]), tr("Sphère"));
}

void MainWindow::supprimerSelection()
{
    const std::vector<int> ids = idsSelectionnes();
    if (ids.empty())
    {
        statusBar()->showMessage(tr("Rien n'est sélectionné."));
        return;
    }

    if (m_mode == ModeSelection::Arete)
    {
        QMessageBox::information(
            this, tr("Supprimer une arête"),
            tr("Une arête ne se supprime pas seule : sélectionnez la face concernée "
               "(mode Face, touche S), ou passez en mode Pièce (touche P) pour supprimer une pièce.\n\n"
               "Pour adoucir une arête, utilisez Congé ou Chanfrein."));
        return;
    }

    if (m_mode == ModeSelection::Face)
    {
        if (ids.size() != 1)
        {
            QMessageBox::information(this, tr("Supprimer une face"),
                                     tr("Les faces à supprimer doivent appartenir à la même pièce."));
            return;
        }

        std::vector<TopoDS_Shape> faces;
        for (const ElementSelectionne& e : m_vue->selection())
        {
            if (!e.forme.IsNull() && e.forme.ShapeType() == TopAbs_FACE)
                faces.push_back(e.forme);
        }

        const Piece* piece = m_document.trouver(ids.front());
        TopoDS_Shape resultat;
        if (!piece || !modelisation::supprimerFaces(piece->forme, faces, resultat))
        {
            QMessageBox::warning(
                this, tr("Supprimer une face"),
                tr("Impossible de refermer la pièce après cette suppression : les faces voisines "
                   "ne peuvent pas se prolonger.\n\n"
                   "Cela fonctionne pour un perçage, une bosse, un congé ou un épaulement, "
                   "pas pour une face d'un simple cube. Pour découper, utilisez plutôt "
                   "Soustraire avec une autre pièce."));
            return;
        }

        m_document.modifier(ids, [resultat](const TopoDS_Shape&) { return resultat; });
        rafraichir();
        statusBar()->showMessage(tr("%1 face(s) supprimée(s).").arg(static_cast<int>(faces.size())));
        return;
    }

    m_document.supprimer(ids);
    rafraichir();
}

void MainWindow::toutSupprimer()
{
    if (m_document.vide())
        return;

    m_document.vider();
    rafraichir();
}

void MainWindow::deplacerSelection()
{
    const std::vector<int> ids = idsSelectionnes();
    if (ids.empty() || m_mode != ModeSelection::Piece)
    {
        QMessageBox::information(
            this, tr("Déplacer / tourner"),
            tr("Sélectionnez d'abord une ou plusieurs pièces (mode Pièce, touche P)."));
        return;
    }

    DialogueTransformation dialogue(this);
    if (dialogue.exec() != QDialog::Accepted)
        return;

    const QVector<double> v = dialogue.valeurs();
    appliquerTransformation(ids, v[0], v[1], v[2], v[3], v[4], v[5]);
    rafraichirEnGardantSelection(ids);
}

void MainWindow::etirerFace()
{
    const std::vector<ElementSelectionne> selection = m_vue->selection();
    if (selection.size() != 1 || !modelisation::estFacePlane(selection.front().forme))
    {
        QMessageBox::information(
            this, tr("Étirer une face"),
            tr("Passez en mode Surface (touche S) et sélectionnez une seule face plane.\n\n"
               "Vous pouvez aussi la faire glisser directement à la souris."));
        return;
    }

    bool valide = false;
    const double distance = QInputDialog::getDouble(
        this, tr("Étirer une face"),
        tr("Distance (mm) : positive pour étirer vers l'extérieur,\n"
           "négative pour rentrer dans la pièce"),
        5.0, -10000.0, 10000.0, 2, &valide);
    if (!valide || distance == 0.0)
        return;

    const int idPiece = selection.front().idPiece;
    const TopoDS_Shape face = selection.front().forme;
    m_document.modifier({idPiece}, [face, distance](const TopoDS_Shape& forme) {
        return modelisation::etirerFace(forme, face, distance);
    });
    rafraichir();
}

void MainWindow::rafraichirEnGardantSelection(const std::vector<int>& ids)
{
    rafraichir();
    if (m_mode == ModeSelection::Piece)
    {
        m_vue->selectionnerPieces(ids);
        surSelectionChangee();
    }
}

void MainWindow::tournerDe(double rx, double ry, double rz)
{
    const std::vector<int> ids = idsSelectionnes();
    if (ids.empty())
    {
        statusBar()->showMessage(tr("Sélectionnez d'abord une pièce."));
        return;
    }

    appliquerTransformation(ids, 0.0, 0.0, 0.0, rx, ry, rz);
    rafraichirEnGardantSelection(ids);
}

void MainWindow::poserSurPlateau()
{
    const std::vector<int> ids = idsSelectionnes();
    if (ids.empty())
    {
        statusBar()->showMessage(tr("Sélectionnez d'abord une pièce."));
        return;
    }

    m_document.transformer(ids, [](const TopoDS_Shape& forme) {
        gp_Trsf descente;
        const modelisation::Mesures m = modelisation::mesurer(forme);
        if (m.boiteValide)
            descente.SetTranslation(gp_Vec(0.0, 0.0, -m.zmin));
        return descente;
    });
    rafraichirEnGardantSelection(ids);
}

void MainWindow::poserFaceAPlat()
{
    const std::vector<ElementSelectionne> selection = m_vue->selection();
    if (selection.size() != 1 || !modelisation::estFacePlane(selection.front().forme))
    {
        QMessageBox::information(
            this, tr("Poser à plat"),
            tr("Passez en mode Surface (touche S) et sélectionnez une seule face plane : "
               "c'est elle qui sera posée sur le plateau."));
        return;
    }

    const int idPiece = selection.front().idPiece;
    const TopoDS_Shape face = selection.front().forme;
    m_document.modifier({idPiece}, [face](const TopoDS_Shape& forme) {
        return modelisation::poserFaceAPlat(forme, face);
    });
    m_document.composerOrientation({idPiece}, modelisation::rotationPoserFaceAPlat(face));
    rafraichir();
    m_vue->recadrer();
}

void MainWindow::annuler()
{
    m_document.annuler();
    rafraichir();
}

void MainWindow::refaire()
{
    m_document.refaire();
    rafraichir();
}

void MainWindow::appliquerPlateau()
{
    QSettings reglages("FreeTable3D", "FreeTable3D");
    m_vue->afficherPlateau(reglages.value("plateau/visible", true).toBool(),
                           reglages.value("plateau/x", 220.0).toDouble(),
                           reglages.value("plateau/y", 220.0).toDouble(),
                           reglages.value("plateau/z", 250.0).toDouble());
}

void MainWindow::reglerPlateau()
{
    QSettings reglages("FreeTable3D", "FreeTable3D");

    QDialog dialogue(this);
    dialogue.setWindowTitle(tr("Plateau de l'imprimante"));
    auto* mise = new QVBoxLayout(&dialogue);

    auto* visible = new QCheckBox(tr("Afficher le volume d'impression"));
    visible->setChecked(reglages.value("plateau/visible", true).toBool());
    mise->addWidget(visible);

    auto* formulaire = new QFormLayout;
    auto champ = [&reglages](const char* cle, double defaut) {
        auto* c = new QDoubleSpinBox;
        c->setRange(1.0, 10000.0);
        c->setDecimals(0);
        c->setSuffix(QStringLiteral(" mm"));
        c->setValue(reglages.value(cle, defaut).toDouble());
        return c;
    };
    auto* px = champ("plateau/x", 220.0);
    auto* py = champ("plateau/y", 220.0);
    auto* pz = champ("plateau/z", 250.0);
    formulaire->addRow(tr("Largeur X"), px);
    formulaire->addRow(tr("Profondeur Y"), py);
    formulaire->addRow(tr("Hauteur maximale Z"), pz);
    mise->addLayout(formulaire);
    mise->addWidget(new QLabel(tr("Le plateau est centré sur l'origine (X = 0, Y = 0).")));

    auto* boutons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(boutons, &QDialogButtonBox::accepted, &dialogue, &QDialog::accept);
    connect(boutons, &QDialogButtonBox::rejected, &dialogue, &QDialog::reject);
    mise->addWidget(boutons);

    if (dialogue.exec() != QDialog::Accepted)
        return;

    reglages.setValue("plateau/visible", visible->isChecked());
    reglages.setValue("plateau/x", px->value());
    reglages.setValue("plateau/y", py->value());
    reglages.setValue("plateau/z", pz->value());
    appliquerPlateau();
}

QString MainWindow::dossierRecuperation()
{
    const QString dossier = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dossier);
    return dossier;
}

void MainWindow::sauvegardeAutomatique()
{
    if (!m_modifie || m_document.vide())
        return;

    // On écrit dans un fichier temporaire, puis on le renomme : un plantage pendant l'écriture
    // ne détruit pas la sauvegarde précédente.
    const QString dossier = dossierRecuperation();
    const QString temporaire = dossier + QStringLiteral("/recuperation.tmp");
    const QString final      = dossier + QStringLiteral("/recuperation.ft3d");
    const QString infos      = dossier + QStringLiteral("/recuperation.info");

    if (!m_document.enregistrer(temporaire.toStdString()))
    {
        QFile::remove(temporaire);
        return;
    }
    QFile::remove(final);
    QFile::rename(temporaire, final);

    // Chemin du projet d'origine (vide s'il n'a jamais été enregistré)
    QFile fichierInfos(infos);
    if (fichierInfos.open(QIODevice::WriteOnly | QIODevice::Truncate))
        fichierInfos.write(m_chemin.toUtf8());
}

void MainWindow::effacerRecuperation()
{
    const QString dossier = dossierRecuperation();
    QFile::remove(dossier + QStringLiteral("/recuperation.ft3d"));
    QFile::remove(dossier + QStringLiteral("/recuperation.tmp"));
    QFile::remove(dossier + QStringLiteral("/recuperation.info"));
}

void MainWindow::proposerRecuperation()
{
    const QString dossier = dossierRecuperation();
    const QString final   = dossier + QStringLiteral("/recuperation.ft3d");
    if (!QFile::exists(final))
        return;

    const QString date = QLocale().toString(QFileInfo(final).lastModified(), QLocale::ShortFormat);
    const auto reponse = QMessageBox::question(
        this, tr("Récupération du travail"),
        tr("FreeTable3D ne s'est pas fermé normalement. Une sauvegarde automatique a été "
           "trouvée (%1).\n\nVoulez-vous la restaurer ?")
            .arg(date),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

    if (reponse != QMessageBox::Yes)
    {
        effacerRecuperation();
        return;
    }

    Document charge;
    if (!charge.charger(final.toStdString()))
    {
        QMessageBox::warning(this, tr("Récupération du travail"),
                             tr("La sauvegarde automatique n'a pas pu être lue."));
        effacerRecuperation();
        return;
    }

    QString cheminOrigine;
    QFile fichierInfos(dossier + QStringLiteral("/recuperation.info"));
    if (fichierInfos.open(QIODevice::ReadOnly))
        cheminOrigine = QString::fromUtf8(fichierInfos.readAll());

    if (m_vue->esquisseActive())
        quitterEsquisse();
    m_document = std::move(charge);
    m_chemin   = cheminOrigine;
    rafraichir();
    m_vue->recadrer();
    m_modifie = true; // à enregistrer : la sauvegarde automatique reste en place jusque-là
    majTitre();
    statusBar()->showMessage(tr("Travail restauré. Pensez à l'enregistrer."), 8000);
}

void MainWindow::majTitre()
{
    const QString nom = m_chemin.isEmpty() ? tr("Sans titre") : QFileInfo(m_chemin).fileName();
    setWindowTitle(QStringLiteral("%1%2 — FreeTable3D").arg(nom, m_modifie ? QStringLiteral(" *") : QString()));
}

// Retourne false si l'utilisateur annule (modifications non enregistrées).
bool MainWindow::confirmerAbandon()
{
    if (!m_modifie || m_document.vide())
        return true;

    const auto reponse = QMessageBox::question(
        this, tr("Projet modifié"), tr("Enregistrer les modifications avant de continuer ?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (reponse == QMessageBox::Cancel)
        return false;
    if (reponse == QMessageBox::Save)
        return enregistrerProjet();
    return true;
}

void MainWindow::dispositionParDefaut()
{
    // Fenêtre 1315 x 920 px ; liste des pièces 150 px à gauche, propriétés 370 px à droite ;
    // la vue 3D prend le reste.
    if (isMaximized() || isFullScreen())
        showNormal();

    // 1315 x 920 px, mais jamais plus de 95 % de l'écran disponible (écrans plus petits)
    int largeur = 1315;
    int hauteur = 920;
    if (const QScreen* ecran = screen())
    {
        const QRect disponible = ecran->availableGeometry();
        largeur = std::min(largeur, disponible.width() * 95 / 100);
        hauteur = std::min(hauteur, disponible.height() * 95 / 100);
    }
    resize(largeur, hauteur);

    QDockWidget* gauche = nullptr;
    QDockWidget* droite = nullptr;
    for (QDockWidget* dock : findChildren<QDockWidget*>())
    {
        if (dockWidgetArea(dock) == Qt::LeftDockWidgetArea)
            gauche = dock;
        else if (dockWidgetArea(dock) == Qt::RightDockWidgetArea)
            droite = dock;
    }
    if (gauche && droite)
        resizeDocks({gauche, droite}, {150, 370}, Qt::Horizontal);
}

void MainWindow::closeEvent(QCloseEvent* evenement)
{
    if (!confirmerAbandon())
    {
        evenement->ignore();
        return;
    }

    // Mémorise la taille et la position de la fenêtre et des panneaux
    QSettings reglages("FreeTable3D", "FreeTable3D");
    reglages.setValue("fenetre/geometrie", saveGeometry());
    reglages.setValue("fenetre/etat", saveState());
    effacerRecuperation(); // fermeture normale : plus besoin de la sauvegarde automatique
    evenement->accept();
}

void MainWindow::nouveauProjet()
{
    if (!confirmerAbandon())
        return;

    if (m_vue->esquisseActive())
        quitterEsquisse();
    m_document = Document();
    m_chemin.clear();
    rafraichir();
    m_modifie = false;
    majTitre();
    effacerRecuperation();
}

void MainWindow::ouvrirProjet()
{
    if (!confirmerAbandon())
        return;

    const QString chemin = QFileDialog::getOpenFileName(
        this, tr("Ouvrir un projet"), QString(), tr("Projets FreeTable3D (*.ft3d *.cao)"));
    if (chemin.isEmpty())
        return;

    Document charge;
    if (!charge.charger(chemin.toStdString()))
    {
        QMessageBox::warning(this, tr("Ouverture impossible"),
                             tr("Le fichier n'a pas pu être lu comme un projet FreeTable3D."));
        return;
    }

    if (m_vue->esquisseActive())
        quitterEsquisse();
    m_document = std::move(charge);
    m_chemin = chemin;
    rafraichir();
    m_vue->recadrer();
    m_modifie = false;
    majTitre();
    effacerRecuperation();
    statusBar()->showMessage(tr("Projet ouvert : %1").arg(chemin));
}

bool MainWindow::enregistrerProjet()
{
    if (m_chemin.isEmpty())
        return enregistrerProjetSous();

    if (!m_document.enregistrer(m_chemin.toStdString()))
    {
        QMessageBox::warning(this, tr("Enregistrement impossible"),
                             tr("Le fichier n'a pas pu être écrit : %1").arg(m_chemin));
        return false;
    }
    m_modifie = false;
    majTitre();
    effacerRecuperation();
    statusBar()->showMessage(tr("Projet enregistré : %1").arg(m_chemin));
    return true;
}

bool MainWindow::enregistrerProjetSous()
{
    QString chemin = QFileDialog::getSaveFileName(
        this, tr("Enregistrer le projet"), m_chemin.isEmpty() ? QStringLiteral("projet.ft3d") : m_chemin,
        tr("Projets FreeTable3D (*.ft3d)"));
    if (chemin.isEmpty())
        return false;
    if (!chemin.endsWith(".ft3d", Qt::CaseInsensitive))
        chemin += ".ft3d";

    m_chemin = chemin;
    return enregistrerProjet();
}

void MainWindow::importerSTEP()
{
    const QString chemin = QFileDialog::getOpenFileName(
        this, tr("Importer un fichier STEP"), QString(),
        tr("Fichiers STEP (*.step *.stp *.STEP *.STP)"));
    if (chemin.isEmpty())
        return;

    TopoDS_Shape forme;
    if (!modelisation::importerSTEP(chemin.toStdString(), forme))
    {
        QMessageBox::warning(this, tr("Import impossible"),
                             tr("Le fichier n'a pas pu être lu comme un fichier STEP valide."));
        return;
    }
    ajouterPiece(forme, tr("Import"));
}

void MainWindow::exporterSTL()
{
    if (m_document.vide())
    {
        QMessageBox::information(this, tr("Rien à exporter"),
                                 tr("Le document est vide : créez ou importez une pièce d'abord."));
        return;
    }

    // Pièces sélectionnées (toute la pièce, même si une face ou une arête est sélectionnée)
    const std::vector<int> idsSel = idsSelectionnes();

    QDialog dialogue(this);
    dialogue.setWindowTitle(tr("Exporter en STL"));
    auto* mise = new QVBoxLayout(&dialogue);

    auto* groupe = new QGroupBox(tr("Pièces à exporter"));
    auto* miseGroupe = new QVBoxLayout(groupe);
    auto* toutes = new QRadioButton(tr("Toutes les pièces (%1)").arg(static_cast<int>(m_document.pieces().size())));
    auto* selection = new QRadioButton(tr("Seulement la sélection (%1)").arg(static_cast<int>(idsSel.size())));
    miseGroupe->addWidget(toutes);
    miseGroupe->addWidget(selection);
    selection->setEnabled(!idsSel.empty());
    (idsSel.empty() ? toutes : selection)->setChecked(true);
    mise->addWidget(groupe);

    auto* formulaire = new QFormLayout;
    auto* qualite = new QComboBox;
    qualite->addItem(tr("Brouillon (0,2 mm)"), 0.2);
    qualite->addItem(tr("Standard (0,05 mm)"), 0.05);
    qualite->addItem(tr("Fin (0,01 mm)"), 0.01);
    qualite->addItem(tr("Très fin (0,005 mm) — fichier volumineux"), 0.005);
    qualite->setCurrentIndex(1);
    formulaire->addRow(tr("Finesse du maillage"), qualite);
    mise->addLayout(formulaire);

    auto* boutons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(boutons, &QDialogButtonBox::accepted, &dialogue, &QDialog::accept);
    connect(boutons, &QDialogButtonBox::rejected, &dialogue, &QDialog::reject);
    mise->addWidget(boutons);

    if (dialogue.exec() != QDialog::Accepted)
        return;

    const bool seulementSelection = selection->isChecked();
    const double deflexion = qualite->currentData().toDouble();

    QString chemin = QFileDialog::getSaveFileName(
        this, tr("Exporter en STL"), "piece.stl", tr("Fichiers STL (*.stl)"));
    if (chemin.isEmpty())
        return;
    if (!chemin.endsWith(".stl", Qt::CaseInsensitive))
        chemin += ".stl";

    std::vector<TopoDS_Shape> formes;
    for (const Piece& piece : m_document.pieces())
    {
        if (!seulementSelection
            || std::find(idsSel.begin(), idsSel.end(), piece.id) != idsSel.end())
            formes.push_back(piece.forme);
    }

    if (!modelisation::exporterSTL(modelisation::assembler(formes), chemin.toStdString(), deflexion))
    {
        QMessageBox::warning(this, tr("Export impossible"),
                             tr("L'écriture du fichier STL a échoué."));
        return;
    }
    statusBar()->showMessage(tr("Exporté : %1 pièce(s) dans %2").arg(static_cast<int>(formes.size())).arg(chemin));
}
