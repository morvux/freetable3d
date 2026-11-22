#include "Traduction.h"

#include <QCoreApplication>
#include <QHash>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace traduction
{
namespace
{

// Traducteur fondé sur une table : texte d'origine (français) -> anglais.
// Le contexte est ignoré : un même texte a une seule traduction.
class TraducteurTable : public QTranslator
{
public:
    explicit TraducteurTable(QObject* parent) : QTranslator(parent)
    {
        for (int i = 0; i < kNombreAnglais; ++i)
            m_table.insert(QString::fromUtf8(kTableAnglais[i].francais),
                           QString::fromUtf8(kTableAnglais[i].anglais));
    }

    QString translate(const char* /*contexte*/, const char* texte, const char* /*desambiguation*/,
                      int /*n*/) const override
    {
        if (!texte)
            return QString();
        const auto it = m_table.constFind(QString::fromUtf8(texte));
        return it == m_table.constEnd() ? QString() : it.value(); // vide = pas de traduction
    }

    bool isEmpty() const override { return false; }

private:
    QHash<QString, QString> m_table;
};

} // namespace

QString langueChoisie()
{
    QSettings reglages;
    const QString code = reglages.value(QStringLiteral("langue")).toString();
    if (code == QLatin1String("fr") || code == QLatin1String("en"))
        return code;
    return QLocale::system().language() == QLocale::French ? QStringLiteral("fr")
                                                           : QStringLiteral("en");
}

void enregistrerLangue(const QString& code)
{
    QSettings reglages;
    reglages.setValue(QStringLiteral("langue"), code);
}

void installer(QCoreApplication& application)
{
    if (langueChoisie() == QLatin1String("en"))
    {
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedKingdom));
        application.installTranslator(new TraducteurTable(&application));
        return;
    }

    // Français : boutons et boîtes standard de Qt (Oui, Non, Enregistrer...), si les
    // traductions de Qt sont installées sur le système.
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));
    auto* qt = new QTranslator(&application);
    if (qt->load(QStringLiteral("qtbase_fr"), QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        application.installTranslator(qt);
}

} // namespace traduction
