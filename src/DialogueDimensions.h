#pragma once

// Petite boîte de dialogue qui demande les dimensions d'une primitive,
// suivies automatiquement de sa position (X, Y, Z).

#include <QDialog>
#include <QDoubleSpinBox>
#include <QStringList>
#include <QVector>

class DialogueDimensions : public QDialog
{
    Q_OBJECT

public:
    DialogueDimensions(const QString& titre,
                       const QStringList& dimensions,
                       const QVector<double>& valeursInitiales,
                       QWidget* parent = nullptr);

    // Valeurs saisies : d'abord les dimensions, puis la position X, Y, Z.
    QVector<double> valeurs() const;

private:
    QVector<QDoubleSpinBox*> m_champs;
};
