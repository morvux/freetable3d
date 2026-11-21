#pragma once

// Boîte de dialogue « Déplacer / tourner » : déplacement en mm et rotation
// en degrés autour des axes X, Y, Z (passant par le centre de la pièce).

#include <QDialog>
#include <QDoubleSpinBox>
#include <QVector>

class DialogueTransformation : public QDialog
{
    Q_OBJECT

public:
    explicit DialogueTransformation(QWidget* parent = nullptr);

    // Ordre : déplacement X, Y, Z (mm) puis rotation X, Y, Z (degrés).
    QVector<double> valeurs() const;

private:
    QVector<QDoubleSpinBox*> m_champs;
};
