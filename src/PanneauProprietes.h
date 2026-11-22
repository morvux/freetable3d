#pragma once

// Panneau « Propriétés » : dimensions, position, orientation, diamètres... de ce qui
// est sélectionné, plus des boutons d'orientation et les réglages d'aimantation.

#include <memory>
#include <vector>

#include <QWidget>

#include "Document.h"
#include "Esquisse.h"
#include "ViewerWidget.h"

class QLabel;
class QFormLayout;
class QDoubleSpinBox;
class QPushButton;

class PanneauProprietes : public QWidget
{
    Q_OBJECT

public:
    explicit PanneauProprietes(QWidget* parent = nullptr);

    // Met à jour l'affichage pour la sélection courante.
    void afficher(const std::vector<ElementSelectionne>& selection, const Document& document);

signals:
    // Valeurs saisies à la main (validées par Entrée ou en quittant le champ).
    void dimensionsDemandees(double x, double y, double z);
    void positionDemandee(double xmin, double ymin, double zmin); // coin bas
    void orientationDemandee(double rx, double ry, double rz);     // angles absolus
    void diametreDemande(double diametre);
    // Esquisse ou hauteur d'une pièce paramétrique modifiée : nouvelle définition complète.
    void parametresModifies(const Parametrique& parametres);

    // Boutons de placement
    void poserSurPlateauDemande();
    void poserFaceAPlatDemande();
    void centrerDemande();

    // Réglages
    void aimantationGrilleChangee(double pasMm);
    void aimantationAngleChangee(double pasDegres);
    void axeRotationChange(AxeRotation axe);

private:
    void viderTextes();
    void afficherParametres(const std::shared_ptr<const Parametrique>& parametres);
    void viderParametres();

    QLabel* m_titre = nullptr;
    QDoubleSpinBox* m_dim[3] = {};
    QDoubleSpinBox* m_pos[3] = {};
    QDoubleSpinBox* m_ori[3] = {};
    QDoubleSpinBox* m_diametre = nullptr;
    QLabel* m_etiquetteDiametre = nullptr;
    QLabel* m_infoDirection = nullptr;
    QLabel* m_details = nullptr;
    QWidget*     m_groupeParametres = nullptr;
    QFormLayout* m_miseParametres = nullptr;
    Parametrique m_parametres; // copie de travail affichée dans le panneau
    QPushButton* m_boutonPlateau = nullptr;
    QPushButton* m_boutonFaceAPlat = nullptr;
    QPushButton* m_boutonCentrer = nullptr;
    QPushButton* m_boutonRaz = nullptr;

    // Dernières valeurs affichées : on n'émet que si l'utilisateur a vraiment changé quelque chose.
    double m_valeurs[3][3] = {};
    double m_valeurDiametre = 0.0;

    void definirTriplet(QDoubleSpinBox* const champs[3], int groupe, const double v[3]);
};
