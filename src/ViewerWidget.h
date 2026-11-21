#pragma once

// Widget Qt qui affiche le document en 3D avec OpenCascade.
// Souris : clic gauche = sélection (ou rotation en glissant), Maj+clic =
// ajouter/retirer de la sélection, clic droit ou milieu = déplacement,
// molette = zoom.

#include <vector>

#include <QPoint>
#include <QString>
#include <QWidget>

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <TopoDS_Shape.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

#include "Document.h"
#include "GrilleAxes.h"

// Ce que le clic sélectionne : la pièce entière, une de ses faces ou une de ses arêtes.
enum class ModeSelection
{
    Piece,
    Face,
    Arete
};

// Un élément actuellement sélectionné.
struct ElementSelectionne
{
    int          idPiece = 0;   // pièce à laquelle appartient l'élément
    TopoDS_Shape forme;         // la pièce, la face ou l'arête elle-même
    QString      description;   // ex. « Face plane · aire 400,00 mm² »
};

class ViewerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ViewerWidget(QWidget* parent = nullptr);

    // Reconstruit l'affichage à partir du document (la sélection est effacée).
    void afficherDocument(const Document& document);

    // Ajuste le zoom pour voir toutes les pièces (ou la zone de travail si le document est vide).
    void recadrer();

    // Oriente la caméra (dessus, face, côté, isométrique...) puis recadre.
    void orienterVue(V3d_TypeOfOrientation orientation);

    // Affiche ou masque la grille millimétrique et les axes.
    void afficherGrille(bool visible);

    // Choisit ce que le clic sélectionne. Efface la sélection en cours.
    void definirModeSelection(ModeSelection mode);

    // Éléments actuellement sélectionnés.
    std::vector<ElementSelectionne> selection() const;

    // Sélectionne une pièce entière (mode Pièce uniquement). N'émet pas de signal.
    void selectionnerPiece(int idPiece);

signals:
    // Émis quand l'utilisateur change la sélection à la souris.
    void selectionChangee();

protected:
    // OpenCascade dessine directement dans la fenêtre : Qt ne doit pas le faire.
    QPaintEngine* paintEngine() const override { return nullptr; }

    void paintEvent(QPaintEvent* evenement) override;
    void resizeEvent(QResizeEvent* evenement) override;
    void showEvent(QShowEvent* evenement) override;
    void mousePressEvent(QMouseEvent* evenement) override;
    void mouseMoveEvent(QMouseEvent* evenement) override;
    void mouseReleaseEvent(QMouseEvent* evenement) override;
    void wheelEvent(QWheelEvent* evenement) override;

private:
    struct ObjetAffiche
    {
        int               idPiece;
        Handle(AIS_Shape) objet;
    };

    // Resynchronise la vue OpenCascade avec la taille réelle de la fenêtre native.
    void ajusterVue();

    // Numéro de mode de sélection OpenCascade correspondant au mode courant.
    int modeOcct() const;

    // Id de la pièce qui possède cet objet affiché (0 si inconnu).
    int idPieceDe(const Handle(AIS_InteractiveObject)& objet) const;

    Handle(V3d_Viewer)             m_viewer;
    Handle(V3d_View)               m_vue;
    Handle(AIS_InteractiveContext) m_contexte;
    Handle(GrilleAxes)             m_grille;
    std::vector<ObjetAffiche>      m_objets;
    ModeSelection                  m_mode = ModeSelection::Piece;
    QPoint                         m_dernierePosition;
    QPoint                         m_positionPression;
    bool                           m_premierAffichage = true;
};
