#include "PanneauProprietes.h"

#include <algorithm>
#include <cmath>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QStringList>
#include <QPixmap>
#include <QVBoxLayout>

#include "Modelisation.h"

namespace
{

// Nombre à 2 décimales, au format de la langue ; évite l'affichage « -0,00 ».
QString nombre(double valeur, int decimales = 2)
{
    if (std::abs(valeur) < 0.5 * std::pow(10.0, -decimales))
        valeur = 0.0;
    return QLocale().toString(valeur, 'f', decimales);
}

QLabel* creerEtiquette()
{
    auto* etiquette = new QLabel(QStringLiteral("—"));
    etiquette->setWordWrap(true);
    etiquette->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return etiquette;
}

QDoubleSpinBox* creerChamp(const QString& prefixe, const QString& suffixe,
                           double min, double max)
{
    auto* champ = new QDoubleSpinBox;
    champ->setRange(min, max);
    champ->setDecimals(2);
    champ->setPrefix(prefixe);
    champ->setSuffix(suffixe);
    champ->setKeyboardTracking(false);
    champ->setEnabled(false);
    return champ;
}

QWidget* ligne(QDoubleSpinBox* const champs[3])
{
    auto* w = new QWidget;
    auto* l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(4);
    for (int i = 0; i < 3; ++i)
        l->addWidget(champs[i]);
    return w;
}

} // namespace

PanneauProprietes::PanneauProprietes(QWidget* parent)
    : QWidget(parent)
{
    setMinimumWidth(330);

    auto* principal = new QVBoxLayout(this);

    // --- Sélection ---
    auto* groupeSelection = new QGroupBox(tr("Sélection"));
    auto* formulaire = new QFormLayout(groupeSelection);

    m_titre = new QLabel;
    m_titre->setWordWrap(true);
    QFont police = m_titre->font();
    police.setBold(true);
    m_titre->setFont(police);

    const QString axes[3] = {QStringLiteral("X "), QStringLiteral("Y "), QStringLiteral("Z ")};
    for (int i = 0; i < 3; ++i)
    {
        m_dim[i] = creerChamp(axes[i], tr(" mm"), 0.01, 100000.0);
        m_pos[i] = creerChamp(axes[i], tr(" mm"), -100000.0, 100000.0);
        m_ori[i] = creerChamp(axes[i], QStringLiteral("°"), -360.0, 360.0);
    }

    m_diametre = creerChamp(QString(), tr(" mm"), 0.01, 100000.0);
    m_etiquetteDiametre = new QLabel(tr("Diamètre"));
    m_infoDirection = creerEtiquette();
    m_infoDirection->setText(QString());
    m_details = creerEtiquette();

    formulaire->addRow(m_titre);
    formulaire->addRow(tr("Dimensions"), ligne(m_dim));
    formulaire->addRow(tr("Position (coin bas)"), ligne(m_pos));
    formulaire->addRow(tr("Orientation"), ligne(m_ori));
    formulaire->addRow(m_etiquetteDiametre, m_diametre);
    formulaire->addRow(m_infoDirection);
    formulaire->addRow(m_details);
    m_etiquetteDiametre->hide();
    m_diametre->hide();

    // Validation d'une saisie : on émet le triplet complet s'il a changé.
    auto relier = [this](QDoubleSpinBox* const champs[3], int groupe) {
        for (int i = 0; i < 3; ++i)
        {
            connect(champs[i], &QDoubleSpinBox::editingFinished, this, [this, champs, groupe]() {
                const double v[3] = {champs[0]->value(), champs[1]->value(), champs[2]->value()};
                bool change = false;
                for (int k = 0; k < 3; ++k)
                    change = change || std::abs(v[k] - m_valeurs[groupe][k]) > 1e-6;
                if (!change)
                    return;
                for (int k = 0; k < 3; ++k)
                    m_valeurs[groupe][k] = v[k];
                if (groupe == 0)      emit dimensionsDemandees(v[0], v[1], v[2]);
                else if (groupe == 1) emit positionDemandee(v[0], v[1], v[2]);
                else                  emit orientationDemandee(v[0], v[1], v[2]);
            });
        }
    };
    relier(m_dim, 0);
    relier(m_pos, 1);
    relier(m_ori, 2);

    connect(m_diametre, &QDoubleSpinBox::editingFinished, this, [this]() {
        const double d = m_diametre->value();
        if (std::abs(d - m_valeurDiametre) < 1e-6)
            return;
        m_valeurDiametre = d;
        emit diametreDemande(d);
    });
    principal->addWidget(groupeSelection);

    // --- Actions de placement ---
    auto* groupeActions = new QGroupBox(tr("Placement"));
    auto* grille = new QGridLayout(groupeActions);

    m_boutonPlateau = new QPushButton(tr("Poser sur le plateau"));
    m_boutonPlateau->setToolTip(tr("Descend la pièce pour que son point le plus bas touche le plateau (B)"));
    m_boutonFaceAPlat = new QPushButton(tr("Poser la face à plat"));
    m_boutonFaceAPlat->setToolTip(tr("Sélectionnez une face plane (mode Face, touche S) : elle sera posée sur le plateau (L)"));
    m_boutonCentrer = new QPushButton(tr("Centrer sur le plateau"));
    m_boutonCentrer->setToolTip(tr("Place le centre de la pièce à X = 0, Y = 0 (la hauteur ne change pas)"));
    m_boutonRaz = new QPushButton(tr("Orientation à 0°"));
    m_boutonRaz->setToolTip(tr("Remet la pièce dans son orientation d'origine (X, Y, Z = 0°)"));

    grille->addWidget(m_boutonPlateau, 0, 0);
    grille->addWidget(m_boutonFaceAPlat, 0, 1);
    grille->addWidget(m_boutonCentrer, 1, 0);
    grille->addWidget(m_boutonRaz, 1, 1);
    for (QPushButton* bouton : {m_boutonPlateau, m_boutonFaceAPlat, m_boutonCentrer, m_boutonRaz})
        bouton->setEnabled(false);

    connect(m_boutonPlateau, &QPushButton::clicked, this, &PanneauProprietes::poserSurPlateauDemande);
    connect(m_boutonFaceAPlat, &QPushButton::clicked, this, &PanneauProprietes::poserFaceAPlatDemande);
    connect(m_boutonCentrer, &QPushButton::clicked, this, &PanneauProprietes::centrerDemande);
    connect(m_boutonRaz, &QPushButton::clicked, this, [this]() { emit orientationDemandee(0.0, 0.0, 0.0); });
    principal->addWidget(groupeActions);

    // --- Esquisse et extrusion (pièces paramétriques) ---
    auto* groupeParam = new QGroupBox(tr("Esquisse et extrusion"));
    m_miseParametres = new QFormLayout(groupeParam);
    m_groupeParametres = groupeParam;
    groupeParam->hide();
    principal->addWidget(groupeParam);

    // --- Réglages de la souris ---
    auto* groupeReglages = new QGroupBox(tr("Réglages de la souris"));
    auto* reglages = new QFormLayout(groupeReglages);

    // Les valeurs par défaut correspondent à celles de ViewerWidget (1 mm, 15°, axe Z).
    auto* aimantationGrille = new QComboBox;
    aimantationGrille->addItem(tr("Libre"), 0.0);
    aimantationGrille->addItem(tr("1 mm"), 1.0);
    aimantationGrille->addItem(tr("5 mm"), 5.0);
    aimantationGrille->addItem(tr("10 mm"), 10.0);
    aimantationGrille->setCurrentIndex(1);
    connect(aimantationGrille, &QComboBox::currentIndexChanged, this,
            [this, aimantationGrille](int index) {
                emit aimantationGrilleChangee(aimantationGrille->itemData(index).toDouble());
            });
    reglages->addRow(tr("Aimantation à la grille"), aimantationGrille);

    auto* aimantationAngle = new QComboBox;
    aimantationAngle->addItem(tr("Libre"), 0.0);
    aimantationAngle->addItem(tr("5°"), 5.0);
    aimantationAngle->addItem(tr("15°"), 15.0);
    aimantationAngle->addItem(tr("45°"), 45.0);
    aimantationAngle->addItem(tr("90°"), 90.0);
    aimantationAngle->setCurrentIndex(2);
    connect(aimantationAngle, &QComboBox::currentIndexChanged, this,
            [this, aimantationAngle](int index) {
                emit aimantationAngleChangee(aimantationAngle->itemData(index).toDouble());
            });
    reglages->addRow(tr("Aimantation des angles"), aimantationAngle);

    auto* axeRotation = new QComboBox;
    axeRotation->addItem(tr("X"));
    axeRotation->addItem(tr("Y"));
    axeRotation->addItem(tr("Z (vertical)"));
    axeRotation->setCurrentIndex(2);
    connect(axeRotation, &QComboBox::currentIndexChanged, this, [this](int index) {
        const AxeRotation axes[3] = {AxeRotation::X, AxeRotation::Y, AxeRotation::Z};
        if (index >= 0 && index < 3)
            emit axeRotationChange(axes[index]);
    });
    reglages->addRow(tr("Axe de la rotation (touche R)"), axeRotation);
    principal->addWidget(groupeReglages);

    // Image au bas du panneau, avec la mention de la licence
    {
        const qreal   ratio = devicePixelRatioF();
        const int     largeur = 200;
        QPixmap       image(QStringLiteral(":/logo.png"));
        if (!image.isNull())
        {
            image = image.scaledToWidth(qRound(largeur * ratio), Qt::SmoothTransformation);
            image.setDevicePixelRatio(ratio);

            auto* etiquetteImage = new QLabel;
            etiquetteImage->setPixmap(image);
            etiquetteImage->setAlignment(Qt::AlignHCenter | Qt::AlignBottom); // collée au texte dessous
            // L'image occupe l'espace libre sous les réglages, alignée en bas ; si la fenêtre est
            // très basse, elle est rognée sans agrandir le panneau.
            etiquetteImage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
            principal->addWidget(etiquetteImage, 1);
        }
        else
        {
            principal->addStretch(1);
        }

        auto* etiquetteLicence = new QLabel(QStringLiteral("GNU/GPL"));
        etiquetteLicence->setAlignment(Qt::AlignCenter);
        QFont police = etiquetteLicence->font();
        police.setBold(true);
        police.setPointSizeF(police.pointSizeF() + 2.0);
        etiquetteLicence->setFont(police);
        principal->addWidget(etiquetteLicence);
    }

    viderTextes();
}

void PanneauProprietes::definirTriplet(QDoubleSpinBox* const champs[3], int groupe, const double v[3])
{
    for (int i = 0; i < 3; ++i)
    {
        const QSignalBlocker blocage(champs[i]);
        champs[i]->setValue(v[i]);
        m_valeurs[groupe][i] = champs[i]->value();
    }
}

void PanneauProprietes::viderParametres()
{
    while (m_miseParametres->count() > 0)
    {
        QLayoutItem* element = m_miseParametres->takeAt(0);
        if (QWidget* w = element->widget())
        {
            w->hide();
            w->deleteLater(); // peut être appelé depuis le signal d'un de ces widgets
        }
        delete element;
    }
    m_groupeParametres->hide();
}

void PanneauProprietes::afficherParametres(const std::shared_ptr<const Parametrique>& parametres)
{
    viderParametres();
    if (!parametres)
        return;

    m_parametres = *parametres;
    m_groupeParametres->show();

    // Un champ qui, une fois validé, met à jour la copie de travail puis signale le changement.
    auto champ = [this](double valeur, double min, double max, const QString& suffixe, auto ecrire) {
        auto* c = new QDoubleSpinBox;
        c->setRange(min, max);
        c->setDecimals(2);
        c->setSuffix(suffixe);
        c->setKeyboardTracking(false);
        c->setValue(valeur);
        connect(c, &QDoubleSpinBox::editingFinished, this, [this, c, valeur, ecrire]() {
            if (std::abs(c->value() - valeur) < 1e-6)
                return;
            ecrire(m_parametres, c->value());
            emit parametresModifies(m_parametres);
        });
        return c;
    };
    const QString mm = tr(" mm");

    m_miseParametres->addRow(tr("Hauteur d'extrusion"),
                             champ(m_parametres.hauteur, -10000.0, 10000.0, mm,
                                   [](Parametrique& p, double v) { p.hauteur = v; }));

    for (std::size_t i = 0; i < m_parametres.esquisse.profils.size(); ++i)
    {
        const Profil& profil = m_parametres.esquisse.profils[i];
        auto* titre = new QLabel;
        QFont police = titre->font();
        police.setBold(true);
        titre->setFont(police);

        auto ligne = [&](const QString& nom, std::size_t j, double min) {
            m_miseParametres->addRow(
                nom, champ(profil.v[j], min, 100000.0, mm, [i, j](Parametrique& p, double v) {
                    p.esquisse.profils[i].v[j] = v;
                }));
        };

        if (profil.type == TypeProfil::Rectangle && profil.v.size() >= 4)
        {
            titre->setText(tr("Rectangle %1").arg(i + 1));
            m_miseParametres->addRow(titre);
            ligne(tr("  position u"), 0, -100000.0);
            ligne(tr("  position v"), 1, -100000.0);
            ligne(tr("  largeur"), 2, 0.01);
            ligne(tr("  hauteur"), 3, 0.01);
        }
        else if (profil.type == TypeProfil::Cercle && profil.v.size() >= 3)
        {
            titre->setText(tr("Cercle %1").arg(i + 1));
            m_miseParametres->addRow(titre);
            ligne(tr("  centre u"), 0, -100000.0);
            ligne(tr("  centre v"), 1, -100000.0);
            ligne(tr("  rayon"), 2, 0.01);
        }
        else if (profil.type == TypeProfil::Polygone)
        {
            titre->setText(tr("Polygone %1").arg(i + 1));
            m_miseParametres->addRow(titre);
            for (std::size_t k = 0; k + 1 < profil.v.size(); k += 2)
            {
                ligne(tr("  sommet %1 u").arg(k / 2 + 1), k, -100000.0);
                ligne(tr("  sommet %1 v").arg(k / 2 + 1), k + 1, -100000.0);
            }
        }
        else
        {
            delete titre;
        }
    }
    m_miseParametres->addRow(new QLabel(tr("(u, v) : coordonnées dans le plan de l'esquisse d'origine")));
}

void PanneauProprietes::viderTextes()
{
    const double zeros[3] = {0.0, 0.0, 0.0};
    const double un[3] = {1.0, 1.0, 1.0};
    definirTriplet(m_dim, 0, un);
    definirTriplet(m_pos, 1, zeros);
    definirTriplet(m_ori, 2, zeros);
    for (int i = 0; i < 3; ++i)
    {
        m_dim[i]->setEnabled(false);
        m_pos[i]->setEnabled(false);
        m_ori[i]->setEnabled(false);
    }
    for (QPushButton* bouton : {m_boutonPlateau, m_boutonFaceAPlat, m_boutonCentrer, m_boutonRaz})
        bouton->setEnabled(false);
    m_etiquetteDiametre->hide();
    m_diametre->hide();
    m_infoDirection->setText(QString());
    m_details->setText(QString());
}

void PanneauProprietes::afficher(const std::vector<ElementSelectionne>& selection,
                                 const Document& document)
{
    viderTextes();
    viderParametres();

    if (selection.empty())
    {
        m_titre->setText(tr("Aucune sélection"));
        return;
    }
    if (selection.size() > 1)
    {
        m_titre->setText(tr("%1 éléments sélectionnés").arg(static_cast<int>(selection.size())));
        return;
    }

    const ElementSelectionne& element = selection.front();
    const modelisation::Mesures mesures = modelisation::mesurer(element.forme);

    const Piece* piece = document.trouver(element.idPiece);
    const QString nom = piece ? QString::fromStdString(piece->nom) : tr("?");

    const bool pieceEntiere = mesures.type != TopAbs_FACE && mesures.type != TopAbs_EDGE;
    switch (mesures.type)
    {
    case TopAbs_FACE: m_titre->setText(tr("%1 · face (lecture seule)").arg(nom)); break;
    case TopAbs_EDGE: m_titre->setText(tr("%1 · arête (lecture seule)").arg(nom)); break;
    default:          m_titre->setText(nom); break;
    }

    // Dimensions et position (boîte englobante alignée sur les axes) : modifiables pour une pièce
    if (mesures.boiteValide)
    {
        const double dim[3] = {mesures.xmax - mesures.xmin, mesures.ymax - mesures.ymin,
                               mesures.zmax - mesures.zmin};
        const double pos[3] = {mesures.xmin, mesures.ymin, mesures.zmin};
        definirTriplet(m_dim, 0, dim);
        definirTriplet(m_pos, 1, pos);
        for (int i = 0; i < 3; ++i)
        {
            m_dim[i]->setEnabled(pieceEntiere);
            m_pos[i]->setEnabled(pieceEntiere);
        }
    }

    // Orientation de la pièce : modifiable
    if (piece)
    {
        double angles[3] = {0.0, 0.0, 0.0};
        modelisation::anglesEuler(piece->orientation, angles[0], angles[1], angles[2]);
        definirTriplet(m_ori, 2, angles);
        for (int i = 0; i < 3; ++i)
            m_ori[i]->setEnabled(pieceEntiere);
    }

    // Boutons de placement : une pièce (ou une face plane pour « à plat »)
    if (pieceEntiere)
    {
        m_boutonPlateau->setEnabled(true);
        m_boutonCentrer->setEnabled(true);
        m_boutonRaz->setEnabled(true);
    }
    else if (mesures.type == TopAbs_FACE && mesures.aDirection)
    {
        m_boutonFaceAPlat->setEnabled(true);
    }

    QStringList direction;
    if (mesures.aDirection)
    {
        const gp_Vec& d = mesures.direction;
        if (mesures.type == TopAbs_FACE)
        {
            const double z = std::max(-1.0, std::min(1.0, d.Z()));
            const double angleVerticale = std::acos(z) * 180.0 / 3.14159265358979323846;
            direction << tr("Normale : (%1 ; %2 ; %3), %4° avec la verticale")
                             .arg(nombre(d.X()), nombre(d.Y()), nombre(d.Z()), nombre(angleVerticale, 1));
        }
        else
        {
            direction << tr("Direction : (%1 ; %2 ; %3)")
                             .arg(nombre(d.X()), nombre(d.Y()), nombre(d.Z()));
        }
    }
    m_infoDirection->setText(direction.join('\n'));

    // Pièce paramétrique : esquisse et hauteur modifiables
    if (pieceEntiere && piece && piece->parametres)
        afficherParametres(piece->parametres);

    // Diamètre modifiable : sphère ou cylindre seul
    if (pieceEntiere && mesures.diametres.size() == 1 && modelisation::diametreModifiable(element.forme))
    {
        const QSignalBlocker blocage(m_diametre);
        m_diametre->setValue(mesures.diametres.front());
        m_valeurDiametre = m_diametre->value();
        m_diametre->setEnabled(true);
        m_etiquetteDiametre->show();
        m_diametre->show();
    }

    // Détails en lecture seule
    QStringList details;
    if (mesures.type == TopAbs_FACE)
        details << tr("Aire : %1 mm²").arg(nombre(mesures.aire));
    else if (mesures.type == TopAbs_EDGE)
        details << tr("Longueur : %1 mm").arg(nombre(mesures.longueur));
    else
    {
        details << tr("Volume : %1 mm³").arg(nombre(mesures.volume));
        details << tr("Surface : %1 mm²").arg(nombre(mesures.aire));
    }
    if (m_diametre->isHidden())
    {
        for (double diametre : mesures.diametres)
            details << tr("Diamètre : Ø %1 mm (rayon %2 mm)")
                           .arg(nombre(diametre), nombre(diametre / 2.0));
    }
    m_details->setText(details.join('\n'));
}
