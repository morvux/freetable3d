#pragma once

#include <vector>

#include <QMainWindow>

#include "Document.h"
#include "ViewerWidget.h"

class QAction;
class QListWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void importerSTEP();
    void exporterSTL();
    void creerCube();
    void creerCylindre();
    void creerSphere();
    void supprimerSelection();
    void toutSupprimer();
    void deplacerSelection();
    void annuler();
    void refaire();
    void surSelectionChangee();
    void surSelectionListe();

private:
    void creerMenus();
    void creerPanneauPieces();
    void definirMode(ModeSelection mode);
    void ajouterPiece(const TopoDS_Shape& forme, const QString& nom);

    // Reconstruit l'affichage et la liste des pièces à partir du document.
    void rafraichir();

    // Identifiants (sans doublon) des pièces concernées par la sélection.
    std::vector<int> idsSelectionnes() const;

    Document      m_document;
    ViewerWidget* m_vue   = nullptr;
    QListWidget*  m_liste = nullptr;

    QAction* m_actionAnnuler = nullptr;
    QAction* m_actionRefaire = nullptr;
    QAction* m_actionsMode[3] = {nullptr, nullptr, nullptr};

    ModeSelection m_mode = ModeSelection::Piece;
    bool          m_miseAJourListe = false;
};
