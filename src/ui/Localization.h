#ifndef LOCALIZATION_H
#define LOCALIZATION_H

#include <QString>
#include <QTranslator>

class QApplication;

/// Uygulamanın arayüz dilleri. Kaynak koddaki metinler Türkçedir; diğer diller çeviri dosyasından gelir.
enum class AppLanguage
{
    Turkish, ///< Varsayılan; kaynak metinler
    English  ///< translations/canbus_en.ts
};

/**
 * @brief Qt'nin standart düğme metinlerini (Save, Cancel, Yes, No ...) Türkçeye çeviren çevirmen.
 *
 * Qt'nin kendi Türkçe çevirisi (qtbase_tr.qm) bulunamasa veya eksik olsa da onay
 * pencerelerindeki düğmelerin Türkçe görünmesini garanti eder. Kısayol tuş adlarını
 * ("QShortcut" bağlamı: Esc, Space, Ctrl+S ...) ise qtbase_tr.qm'ye bırakmaz, İngilizce tutar.
 * Yalnızca Türkçe dilde kurulur.
 */
class StandardButtonTranslator : public QTranslator
{
    Q_OBJECT

public:
    /**
     * @brief Çevirmeni oluşturur.
     * @param parent Qt sahiplik zinciri için üst nesne.
     */
    explicit StandardButtonTranslator(QObject *parent = nullptr);

    /**
     * @brief Standart düğme metni ise Türkçe karşılığını, kısayol tuş adı ise İngilizce aslını döndürür.
     * @param context Çeviri bağlamı; yalnızca Qt sınıflarının ('Q' ile başlayan) bağlamları çevrilir.
     * @param sourceText İngilizce kaynak metin.
     * @param disambiguation Ayırt edici not (kullanılmaz).
     * @param n Çoğul sayısı (kullanılmaz).
     * @return Türkçe metin, "QShortcut" bağlamında kaynak metnin kendisi; tanınmayan metin için boş
     *         (Qt bir sonraki çevirmene geçer).
     */
    QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr,
                      int n = -1) const override;

    /// Çevirmen her zaman kullanılabilir olduğundan false döndürür.
    bool isEmpty() const override;
};

/// QSettings'te kayıtlı arayüz dilini döndürür (kayıt yoksa Türkçe).
AppLanguage SavedLanguage();

/**
 * @brief Arayüz dili tercihini kaydeder; bir sonraki açılışta uygulanır.
 * @param language Yeni dil.
 */
void SaveLanguage(AppLanguage language);

/**
 * @brief Dilin kendi adını döndürür (menüde gösterilir: "Türkçe", "English").
 * @param language Dil.
 */
QString LanguageNativeName(AppLanguage language);

/**
 * @brief Kayıtlı dile göre çevirileri uygulamaya kurar.
 *
 * Türkçe: Qt'nin qtbase_tr.qm çevirisi (varsa) ve StandardButtonTranslator.
 * İngilizce: uygulamaya gömülü :/i18n/canbus_en.qm (Qt'nin kendi metinleri zaten İngilizcedir).
 * @param application Çevirilerin kurulacağı uygulama.
 * @return Kurulan dil (çeviri dosyası yüklenemezse Türkçe).
 */
AppLanguage InstallTranslations(QApplication &application);

#endif // LOCALIZATION_H
