#include "DialogueDimensions.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

DialogueDimensions::DialogueDimensions(const QString& titre,
                                       const QStringList& dimensions,
                                       const QVector<double>& valeursInitiales,
                                       QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(titre);

    auto* formulaire = new QFormLayout;

    auto ajouterChamp = [&](const QString& nom, double valeur, double minimum) {
        auto* champ = new QDoubleSpinBox;
        champ->setDecimals(2);
        champ->setRange(minimum, 100000.0);
        champ->setValue(valeur);
        formulaire->addRow(nom, champ);
        m_champs.append(champ);
    };

    for (int i = 0; i < dimensions.size(); ++i)
        ajouterChamp(dimensions[i], valeursInitiales.value(i, 10.0), 0.01);

    ajouterChamp(tr("Position X (mm)"), 0.0, -100000.0);
    ajouterChamp(tr("Position Y (mm)"), 0.0, -100000.0);
    ajouterChamp(tr("Position Z (mm)"), 0.0, -100000.0);

    auto* boutons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(boutons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(boutons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* principal = new QVBoxLayout(this);
    principal->addLayout(formulaire);
    principal->addWidget(boutons);
}

QVector<double> DialogueDimensions::valeurs() const
{
    QVector<double> resultat;
    for (const QDoubleSpinBox* champ : m_champs)
        resultat.append(champ->value());
    return resultat;
}
