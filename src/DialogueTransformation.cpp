#include "DialogueTransformation.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

DialogueTransformation::DialogueTransformation(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Déplacer / tourner"));

    auto* formulaire = new QFormLayout;

    auto ajouterChamp = [&](const QString& nom, double limite) {
        auto* champ = new QDoubleSpinBox;
        champ->setDecimals(2);
        champ->setRange(-limite, limite);
        champ->setValue(0.0);
        formulaire->addRow(nom, champ);
        m_champs.append(champ);
    };

    ajouterChamp(tr("Déplacement X (mm)"), 100000.0);
    ajouterChamp(tr("Déplacement Y (mm)"), 100000.0);
    ajouterChamp(tr("Déplacement Z (mm)"), 100000.0);
    ajouterChamp(tr("Rotation autour de X (°)"), 360.0);
    ajouterChamp(tr("Rotation autour de Y (°)"), 360.0);
    ajouterChamp(tr("Rotation autour de Z (°)"), 360.0);

    auto* boutons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(boutons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(boutons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* principal = new QVBoxLayout(this);
    principal->addLayout(formulaire);
    principal->addWidget(boutons);
}

QVector<double> DialogueTransformation::valeurs() const
{
    QVector<double> resultat;
    for (const QDoubleSpinBox* champ : m_champs)
        resultat.append(champ->value());
    return resultat;
}
