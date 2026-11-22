#include "Icones.h"

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>

namespace
{

const QColor kTrait(0xdc, 0xe3, 0xec);
const QColor kAccent(0xff, 0x8a, 0x3d);
const QColor kFond(0x4a, 0x55, 0x66);

// Dessine l'icône « nom » dans un carré de 64 × 64.
bool dessiner(QPainter& p, const QString& nom)
{
    QPen trait(kTrait, 3.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen accent(kAccent, 3.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(trait);
    p.setBrush(Qt::NoBrush);

    auto cube = [&](const QColor& dessus) {
        const QPolygonF haut({{32, 8}, {54, 20}, {32, 32}, {10, 20}});
        p.setBrush(dessus);
        p.drawPolygon(haut);
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(QPolygonF({{10, 20}, {10, 44}, {32, 56}, {54, 44}, {54, 20}}));
        p.drawLine(32, 32, 32, 56);
    };

    if (nom == "cube")
        cube(kFond);
    else if (nom == "cylindre")
    {
        p.drawEllipse(QRectF(14, 8, 36, 14));
        p.drawLine(14, 15, 14, 48);
        p.drawLine(50, 15, 50, 48);
        p.drawArc(QRectF(14, 41, 36, 14), 180 * 16, 180 * 16);
    }
    else if (nom == "sphere")
    {
        p.setBrush(kFond);
        p.drawEllipse(QRectF(10, 10, 44, 44));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(10, 24, 44, 16), 180 * 16, 180 * 16);
    }
    else if (nom == "annuler" || nom == "refaire")
    {
        p.save();
        if (nom == "refaire")
        {
            p.translate(64, 0);
            p.scale(-1, 1);
        }
        p.drawArc(QRectF(14, 18, 36, 32), 20 * 16, 190 * 16);
        p.drawPolyline(QPolygonF({{20, 12}, {14, 24}, {27, 26}}));
        p.restore();
    }
    else if (nom == "piece")
    {
        cube(kAccent);
    }
    else if (nom == "face")
    {
        cube(kFond);
        p.setPen(accent);
        p.setBrush(QColor(0xff, 0x8a, 0x3d, 150));
        p.drawPolygon(QPolygonF({{10, 20}, {32, 32}, {32, 56}, {10, 44}}));
    }
    else if (nom == "arete")
    {
        cube(kFond);
        p.setPen(QPen(kAccent, 5.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(32, 32, 32, 56);
    }
    else if (nom == "rotation")
    {
        p.drawArc(QRectF(12, 12, 40, 40), 30 * 16, 280 * 16);
        p.setPen(accent);
        p.drawPolyline(QPolygonF({{52, 14}, {52, 28}, {38, 28}}));
    }
    else if (nom == "plateau")
    {
        p.setBrush(kFond);
        p.drawPolygon(QPolygonF({{8, 42}, {32, 52}, {56, 42}, {32, 32}}));
        p.setBrush(Qt::NoBrush);
        p.setPen(accent);
        p.drawPolyline(QPolygonF({{32, 32}, {32, 10}}));
        p.drawPolyline(QPolygonF({{24, 18}, {32, 10}, {40, 18}}));
    }
    else if (nom == "faceaplat")
    {
        p.drawLine(6, 54, 58, 54);
        p.setPen(accent);
        p.setBrush(QColor(0xff, 0x8a, 0x3d, 150));
        p.drawRect(QRectF(14, 30, 36, 20));
    }
    else if (nom == "conge")
    {
        p.drawPolyline(QPolygonF({{10, 52}, {10, 12}}));
        p.drawPolyline(QPolygonF({{10, 52}, {54, 52}}));
        p.setPen(accent);
        QPainterPath arc;
        arc.moveTo(10, 12);
        arc.lineTo(10, 12);
        p.drawArc(QRectF(10, 12, 40, 40), 90 * 16, -90 * 16);
    }
    else if (nom == "chanfrein")
    {
        p.drawPolyline(QPolygonF({{10, 52}, {10, 12}}));
        p.drawPolyline(QPolygonF({{10, 52}, {54, 52}}));
        p.setPen(accent);
        p.drawLine(10, 14, 50, 52);
    }
    else if (nom == "trou")
    {
        cube(kFond);
        p.setPen(accent);
        p.setBrush(QColor(0x1b, 0x20, 0x28));
        p.drawEllipse(QRectF(24, 14, 16, 12));
    }
    else if (nom == "union" || nom == "soustraire" || nom == "intersection")
    {
        QPainterPath a, b;
        a.addEllipse(QRectF(6, 14, 32, 32));
        b.addEllipse(QRectF(26, 14, 32, 32));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xff, 0x8a, 0x3d, 190));
        if (nom == "union")
            p.drawPath(a.united(b));
        else if (nom == "soustraire")
            p.drawPath(a.subtracted(b));
        else
            p.drawPath(a.intersected(b));
        p.setPen(trait);
        p.setBrush(Qt::NoBrush);
        p.drawPath(a);
        p.drawPath(b);
    }
    else if (nom == "esquisse")
    {
        p.drawPolyline(QPolygonF({{10, 54}, {16, 38}, {44, 10}, {54, 20}, {26, 48}, {10, 54}}));
        p.drawLine(38, 16, 48, 26);
    }
    else if (nom == "rectangle")
    {
        p.setPen(accent);
        p.drawRect(QRectF(10, 16, 44, 32));
    }
    else if (nom == "cercle")
    {
        p.setPen(accent);
        p.drawEllipse(QRectF(10, 10, 44, 44));
    }
    else if (nom == "polygone")
    {
        p.setPen(accent);
        p.drawPolygon(QPolygonF({{32, 8}, {56, 26}, {47, 54}, {17, 54}, {8, 26}}));
    }
    else if (nom == "dupliquer")
    {
        p.setBrush(kFond);
        p.drawRect(QRectF(8, 8, 32, 32));
        p.setPen(accent);
        p.drawRect(QRectF(24, 24, 32, 32));
    }
    else if (nom == "symetrie")
    {
        p.setPen(QPen(kAccent, 3, Qt::DashLine));
        p.drawLine(32, 6, 32, 58);
        p.setPen(trait);
        p.setBrush(kFond);
        p.drawPolygon(QPolygonF({{8, 48}, {8, 16}, {26, 48}}));
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(QPolygonF({{56, 48}, {56, 16}, {38, 48}}));
    }
    else if (nom == "evider")
    {
        cube(kFond);
        p.setPen(accent);
        p.setBrush(QColor(0x1b, 0x20, 0x28));
        p.drawPolygon(QPolygonF({{32, 18}, {44, 25}, {32, 32}, {20, 25}}));
    }
    else if (nom == "effacer")
    {
        p.drawRect(QRectF(12, 22, 40, 28));
        p.drawLine(8, 22, 56, 22);
        p.drawLine(26, 14, 38, 14);
        p.drawLine(26, 14, 26, 22);
        p.drawLine(38, 14, 38, 22);
    }
    else if (nom == "extruder")
    {
        p.drawPolyline(QPolygonF({{14, 54}, {50, 54}, {50, 36}, {14, 36}, {14, 54}}));
        p.setPen(accent);
        p.drawLine(32, 30, 32, 8);
        p.drawPolyline(QPolygonF({{22, 18}, {32, 8}, {42, 18}}));
    }
    else if (nom == "quitter")
    {
        p.setPen(accent);
        p.drawLine(16, 16, 48, 48);
        p.drawLine(48, 16, 16, 48);
    }
    else if (nom == "supprimer")
    {
        p.drawRect(QRectF(16, 22, 32, 32));
        p.drawLine(10, 22, 54, 22);
        p.drawLine(26, 14, 38, 14);
        p.drawLine(26, 14, 26, 22);
        p.drawLine(38, 14, 38, 22);
        p.drawLine(26, 30, 26, 46);
        p.drawLine(38, 30, 38, 46);
    }
    else if (nom == "dessus")
    {
        p.setBrush(kFond);
        p.drawRect(QRectF(12, 12, 40, 40));
        p.setPen(accent);
        p.drawRect(QRectF(12, 12, 40, 40));
    }
    else if (nom == "devant")
    {
        p.setBrush(kFond);
        p.drawRect(QRectF(12, 12, 40, 40));
        p.setPen(accent);
        p.setBrush(Qt::NoBrush);
        p.drawLine(12, 52, 52, 52);
    }
    else if (nom == "cote")
    {
        p.setBrush(kFond);
        p.drawRect(QRectF(12, 12, 40, 40));
        p.setPen(accent);
        p.drawLine(52, 12, 52, 52);
    }
    else if (nom == "iso")
    {
        cube(kFond);
    }
    else if (nom == "recadrer")
    {
        p.drawPolyline(QPolygonF({{10, 22}, {10, 10}, {22, 10}}));
        p.drawPolyline(QPolygonF({{42, 10}, {54, 10}, {54, 22}}));
        p.drawPolyline(QPolygonF({{54, 42}, {54, 54}, {42, 54}}));
        p.drawPolyline(QPolygonF({{22, 54}, {10, 54}, {10, 42}}));
        p.setPen(accent);
        p.drawRect(QRectF(24, 24, 16, 16));
    }
    else if (nom == "grille")
    {
        for (int i = 0; i < 4; ++i)
        {
            const double t = 10 + i * 15;
            p.drawLine(QPointF(t, 10), QPointF(t, 54));
            p.drawLine(QPointF(10, t), QPointF(54, t));
        }
    }
    else
        return false;
    return true;
}

} // namespace

QIcon icones::creer(const QString& nom)
{
    QIcon icone;
    for (int taille : {24, 32, 48})
    {
        QPixmap image(taille * 2, taille * 2); // rendu x2 : net sur écran HiDPI
        image.fill(Qt::transparent);
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(taille * 2 / 64.0, taille * 2 / 64.0);
        const bool ok = dessiner(p, nom);
        p.end();
        if (!ok)
            return QIcon();
        image.setDevicePixelRatio(2.0);
        icone.addPixmap(image);
    }
    return icone;
}
