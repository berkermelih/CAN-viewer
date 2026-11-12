#include "Localization.h"

#include <QApplication>
#include <QHash>
#include <QLibraryInfo>
#include <QLocale>

namespace {

// Qt'nin standart düğme ve QMessageBox metinleri ile Türkçe karşılıkları.
const QHash<QByteArray, QString> &ButtonTexts()
{
    static const QHash<QByteArray, QString> texts = {
        {"OK", QStringLiteral("Tamam")},
        {"Save", QStringLiteral("Kaydet")},
        {"Save All", QStringLiteral("Tümünü kaydet")},
        {"Open", QStringLiteral("Aç")},
        {"&Yes", QStringLiteral("&Evet")},
        {"Yes", QStringLiteral("Evet")},
        {"Yes to &All", QStringLiteral("&Tümüne evet")},
        {"&No", QStringLiteral("&Hayır")},
        {"No", QStringLiteral("Hayır")},
        {"N&o to All", QStringLiteral("Tümüne h&ayır")},
        {"Abort", QStringLiteral("Durdur")},
        {"Retry", QStringLiteral("Yeniden dene")},
        {"Ignore", QStringLiteral("Yok say")},
        {"Close", QStringLiteral("Kapat")},
        {"Cancel", QStringLiteral("İptal")},
        {"&Cancel", QStringLiteral("&İptal")},
        {"Discard", QStringLiteral("Kaydetme")},
        {"Don't Save", QStringLiteral("Kaydetme")},
        {"Close without Saving", QStringLiteral("Kaydetmeden kapat")},
        {"Help", QStringLiteral("Yardım")},
        {"Apply", QStringLiteral("Uygula")},
        {"Reset", QStringLiteral("Sıfırla")},
        {"Restore Defaults", QStringLiteral("Varsayılanlar")},
        {"Show Details...", QStringLiteral("Ayrıntıları göster...")},
        {"Hide Details...", QStringLiteral("Ayrıntıları gizle...")},
    };
    return texts;
}

} // namespace

StandardButtonTranslator::StandardButtonTranslator(QObject *parent)
    : QTranslator(parent)
{
}

QString StandardButtonTranslator::translate(const char *context, const char *sourceText, const char *disambiguation,
                                            int n) const
{
    Q_UNUSED(disambiguation)
    Q_UNUSED(n)
    // Yalnızca Qt sınıflarının bağlamları (QPlatformTheme, QMessageBox, QDialogButtonBox ...) çevrilir.
    if (context == nullptr || sourceText == nullptr || context[0] != 'Q') {
        return QString();
    }
    return ButtonTexts().value(QByteArray(sourceText));
}

bool StandardButtonTranslator::isEmpty() const
{
    return false;
}

void InstallTurkishTranslations(QApplication &application)
{
    // Varsayılan QLocale değiştirilmez (sayı biçimleri etkilenmesin); yalnızca çeviri dosyası Türkçe seçilir.
    const QLocale turkish(QLocale::Turkish, QLocale::Turkey);

    auto *qtTranslator = new QTranslator(&application);
    const QStringList directories = {QLibraryInfo::path(QLibraryInfo::TranslationsPath),
                                     QCoreApplication::applicationDirPath() + QStringLiteral("/translations")};
    for (const QString &directory : directories) {
        if (qtTranslator->load(turkish, QStringLiteral("qtbase"), QStringLiteral("_"), directory)) {
            QCoreApplication::installTranslator(qtTranslator);
            break;
        }
    }

    // En son kurulan çevirmen önce sorulur: standart düğmeler her durumda Türkçe olur.
    QCoreApplication::installTranslator(new StandardButtonTranslator(&application));
}
