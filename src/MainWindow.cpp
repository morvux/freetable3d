#include "MainWindow.h"

#include <algorithm>

#include <QAction>
#include <QActionGroup>
#include <QDockWidget>
#include <QFileDialog>
#include <QKeySequence>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>

#include "DialogueDimensions.h"
#include "DialogueTransformation.h"
#include "Modelisation.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("CaoLibre");
    resize(1100, 700);

    m_vue = new ViewerWidget;
    setCentralWidget(m_vue);

    creerPanneauPieces();
    creerMenus();

    connect(m_vue, &ViewerWidget::selectionChangee, this, &MainWindow::surSelectionChangee);

    rafraichir();
}

void MainWindow::creerPanneauPieces()
{
    m_liste = new QListWidget;
    m_liste->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_liste, &QListWidget::itemSelectionChanged, this, &MainWindow::surSelectionListe);

    auto* panneau = new QDockWidget(tr("Pièces"), this);
    panneau->setWidget(m_liste);
    panneau->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::LeftDockWidgetArea, panneau);
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
    barre->setMovable(false);

    // --- Fichier ---
    QMenu* menuFichier = menuBar()->addMenu(tr("&Fichier"));
    action(menuFichier, tr("&Importer un fichier STEP…"), QKeySequence("Ctrl+O"),
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
    QAction* aDeplacer = action(menuEdition, tr("&Déplacer / tourner…"), QKeySequence("M"),
                                [this]() { deplacerSelection(); });
    QAction* aSupprimer = action(menuEdition, tr("&Supprimer la sélection"),
                                 QKeySequence::Delete, [this]() { supprimerSelection(); });
    action(menuEdition, tr("&Tout supprimer"), QKeySequence("Ctrl+Del"),
           [this]() { toutSupprimer(); });

    barre->addAction(m_actionAnnuler);
    barre->addAction(m_actionRefaire);
    barre->addSeparator();

    // --- Créer ---
    QMenu* menuCreer = menuBar()->addMenu(tr("&Créer"));
    QAction* aCube = action(menuCreer, tr("&Cube…"), QKeySequence(),
                            [this]() { creerCube(); });
    QAction* aCylindre = action(menuCreer, tr("C&ylindre…"), QKeySequence(),
                                [this]() { creerCylindre(); });
    QAction* aSphere = action(menuCreer, tr("&Sphère…"), QKeySequence(),
                              [this]() { creerSphere(); });

    barre->addAction(aCube);
    barre->addAction(aCylindre);
    barre->addAction(aSphere);
    barre->addSeparator();

    // --- Sélection : ce que le clic sélectionne ---
    QMenu* menuSelection = menuBar()->addMenu(tr("&Sélection"));
    auto* groupeMode = new QActionGroup(this);
    groupeMode->setExclusive(true);

    const struct
    {
        QString       texte;
        const char*   raccourci;
        ModeSelection mode;
    } modes[] = {
        {tr("&Pièce"), "P", ModeSelection::Piece},
        {tr("&Surface (face)"), "S", ModeSelection::Face},
        {tr("&Arête"), "A", ModeSelection::Arete},
    };

    for (const auto& m : modes)
    {
        QAction* a = menuSelection->addAction(m.texte);
        a->setCheckable(true);
        a->setShortcut(QKeySequence(m.raccourci));
        groupeMode->addAction(a);
        const ModeSelection mode = m.mode;
        connect(a, &QAction::triggered, this, [this, mode]() { definirMode(mode); });
        m_actionsMode[static_cast<int>(m.mode)] = a;
        barre->addAction(a);
    }
    m_actionsMode[static_cast<int>(ModeSelection::Piece)]->setChecked(true);
    barre->addSeparator();
    barre->addAction(aDeplacer);
    barre->addAction(aSupprimer);
    barre->addSeparator();

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

    QAction* aGrille = menuVue->addAction(tr("&Grille et axes"));
    aGrille->setCheckable(true);
    aGrille->setChecked(true);
    aGrille->setShortcut(QKeySequence("G"));
    connect(aGrille, &QAction::toggled, this,
            [this](bool visible) { m_vue->afficherGrille(visible); });

    barre->addAction(aDessus);
    barre->addAction(aFace);
    barre->addAction(aCote);
    barre->addAction(aIso);
    barre->addAction(aRecadrer);
    barre->addAction(aGrille);
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

    // Message d'état
    if (selection.empty())
    {
        statusBar()->showMessage(
            tr("%1 pièce(s). Cliquez pour sélectionner ; Maj+clic pour en ajouter.")
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

    if (m_mode != ModeSelection::Piece)
    {
        QMessageBox::information(
            this, tr("Supprimer une face ou une arête"),
            tr("La suppression d'une face ou d'une arête arrivera à l'étape suivante "
               "(découpe et défeaturing).\n\n"
               "Passez en mode Pièce (touche P) pour supprimer une pièce entière."));
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
    m_document.modifier(ids, [&v](const TopoDS_Shape& forme) {
        return modelisation::transformer(forme, v[0], v[1], v[2], v[3], v[4], v[5]);
    });
    rafraichir();
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

    QString chemin = QFileDialog::getSaveFileName(
        this, tr("Exporter en STL"), "piece.stl", tr("Fichiers STL (*.stl)"));
    if (chemin.isEmpty())
        return;
    if (!chemin.endsWith(".stl", Qt::CaseInsensitive))
        chemin += ".stl";

    std::vector<TopoDS_Shape> formes;
    for (const Piece& piece : m_document.pieces())
        formes.push_back(piece.forme);

    if (!modelisation::exporterSTL(modelisation::assembler(formes), chemin.toStdString()))
    {
        QMessageBox::warning(this, tr("Export impossible"),
                             tr("L'écriture du fichier STL a échoué."));
        return;
    }
    statusBar()->showMessage(tr("Exporté : %1").arg(chemin));
}
