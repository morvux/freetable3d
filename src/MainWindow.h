#pragma once

#include <vector>

#include <QList>
#include <QLockFile>
#include <memory>
#include <QMainWindow>

#include "Document.h"
#include "Modelisation.h"
#include "ViewerWidget.h"

class QAction;
class QCloseEvent;
class QListWidget;
class PanneauProprietes;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* evenement) override;


private slots:
    void nouveauProjet();
    void ouvrirProjet();
    bool enregistrerProjet();
    bool enregistrerProjetSous();
    void importerSTEP();
    void exporterSTL();
    void exporterSTEP();
    void creerCube();
    void creerCylindre();
    void creerSphere();
    void supprimerSelection();
    void toutSupprimer();
    void deplacerSelection();
    void etirerFace();
    void tournerDe(double rx, double ry, double rz);
    void poserSurPlateau();
    void percerTrou();
    void aPropos();
    void copierSelection();
    void collerSelection();
    void dupliquerSelection();
    void symetrieSelection();
    void eviderSelection();
    void reglerPlateau();
    void verifierImpression();
    void nouvelleEsquisse();
    void extruderEsquisse();
    void quitterEsquisse();
    void congeOuChanfrein(bool chanfrein);
    void operationBooleenne(modelisation::OperationBooleenne operation);
    void centrerSurPlateau();
    void poserFaceAPlat();
    void annuler();
    void refaire();
    void surDimensionsDemandees(double x, double y, double z);
    void surPositionDemandee(double xmin, double ymin, double zmin);
    void surOrientationDemandee(double rx, double ry, double rz);
    void surDiametreDemande(double diametre);
    void surParametresModifies(const Parametrique& parametres);
    void surSelectionChangee();
    void surSelectionListe();

private:
    void creerMenus();
    void dispositionParDefaut();
    void creerPanneauPieces();
    void creerPanneaux();
    void majTitre();

    // Sauvegarde automatique et récupération après un arrêt anormal
    static QString dossierRecuperation();
    void           sauvegardeAutomatique();
    void           effacerRecuperation();
    void           proposerRecuperation();
    void appliquerPlateau();
    bool confirmerAbandon();
    void majEtatEsquisse(bool actif);
    void appliquerTransformation(const std::vector<int>& ids,
                                 double dx, double dy, double dz,
                                 double rx, double ry, double rz);
    void definirMode(ModeSelection mode);
    void ajouterPiece(const TopoDS_Shape& forme, const QString& nom);

    // Reconstruit l'affichage et la liste des pièces à partir du document.
    void rafraichir();

    // Comme rafraichir(), mais en mode Pièce les pièces indiquées restent sélectionnées,
    // pour enchaîner plusieurs opérations sur la même pièce.
    void rafraichirEnGardantSelection(const std::vector<int>& ids);

    // Identifiants (sans doublon) des pièces concernées par la sélection.
    std::vector<int> idsSelectionnes() const;

    Document      m_document;
    std::vector<Piece> m_presspapiers; // pièces copiées (Ctrl+C)
    int                m_nbCollages = 0; // décale chaque collage successif
    ViewerWidget* m_vue   = nullptr;
    QListWidget*  m_liste = nullptr;
    PanneauProprietes* m_panneau = nullptr;

    QAction* m_actionAnnuler = nullptr;
    QAction* m_actionRefaire = nullptr;
    QAction* m_actionsMode[3] = {nullptr, nullptr, nullptr};

    QAction* m_actionNouvelleEsquisse = nullptr;
    QList<QAction*> m_actionsEnEsquisse; // actifs seulement pendant une esquisse
    int      m_idPieceEsquisse = 0;      // pièce portant la face d'esquisse (0 = plan de base)

    QString m_chemin;        // fichier .ft3d courant (vide = projet jamais enregistré)
    bool    m_modifie = false;

    std::unique_ptr<QLockFile> m_verrou; // une seule instance gère la sauvegarde automatique
    class QTimer*              m_minuteurAuto = nullptr;

    ModeSelection m_mode = ModeSelection::Piece;
    bool          m_miseAJourListe = false;
};
