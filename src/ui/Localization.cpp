#include "Localization.h"

#include <QApplication>
#include <QHash>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>

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
    // Kısayol tuş adları (QKeySequence::toString: Esc, Space, Ctrl, Shift, Del ...) İngilizce kalır;
    // kaynak metin aynen döndürülür ki qtbase_tr.qm "Vazgeç", "Boşluk", "Kontrol" yazmasın.
    if (qstrcmp(context, "QShortcut") == 0) {
        return QString::fromLatin1(sourceText);
    }
    return ButtonTexts().value(QByteArray(sourceText));
}

bool StandardButtonTranslator::isEmpty() const
{
    return false;
}

AppLanguage SavedLanguage()
{
    const QSettings settings;
    return settings.value(QStringLiteral("appearance/language")).toString() == QLatin1String("en") ? AppLanguage::English
                                                                                                  : AppLanguage::Turkish;
}

void SaveLanguage(AppLanguage language)
{
    QSettings settings;
    settings.setValue(QStringLiteral("appearance/language"),
                      language == AppLanguage::English ? QStringLiteral("en") : QStringLiteral("tr"));
}

QString LanguageNativeName(AppLanguage language)
{
    // Dil adları çevrilmez; her dil kendi adıyla gösterilir.
    return language == AppLanguage::English ? QStringLiteral("English") : QStringLiteral("Türkçe");
}

AppLanguage InstallTranslations(QApplication &application)
{
    if (SavedLanguage() == AppLanguage::English) {
        auto *appTranslator = new QTranslator(&application);
        if (appTranslator->load(QStringLiteral(":/i18n/canbus_en.qm"))) {
            QCoreApplication::installTranslator(appTranslator);
            return AppLanguage::English;
        }
        delete appTranslator; // çeviri yoksa Türkçe ile devam edilir
    }

    // Varsayılan QLocale değiştirilmez (sayı biçimleri etkilenmesin); yalnızca çeviri dosyası Türkçe seçilir.
    const QLocale turkish(QLocale::Turkish, QLocale::Turkey);
    // Önce dağıtım klasörü (windeployqt: translations/qt_tr.qm), sonra Qt kurulumu (qtbase_tr.qm) denenir.
    auto *qtTranslator = new QTranslator(&application);
    const QStringList directories = {QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
                                     QLibraryInfo::path(QLibraryInfo::TranslationsPath)};
    const QStringList baseNames = {QStringLiteral("qt"), QStringLiteral("qtbase")};
    bool loaded = false;
    for (const QString &directory : directories) {
        for (const QString &baseName : baseNames) {
            if (!loaded && qtTranslator->load(turkish, baseName, QStringLiteral("_"), directory)) {
                QCoreApplication::installTranslator(qtTranslator);
                loaded = true;
            }
        }
    }
    if (!loaded) {
        delete qtTranslator;
    }

    // En son kurulan çevirmen önce sorulur: standart düğmeler her durumda Türkçe olur.
    QCoreApplication::installTranslator(new StandardButtonTranslator(&application));
    return AppLanguage::Turkish;
}
