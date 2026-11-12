#ifndef LOCALIZATION_H
#define LOCALIZATION_H

#include <QTranslator>

class QApplication;

/**
 * @brief Qt'nin standart düğme metinlerini (Save, Cancel, Yes, No ...) Türkçeye çeviren çevirmen.
 *
 * Qt'nin kendi Türkçe çevirisi (qtbase_tr.qm) bulunamasa veya eksik olsa da onay
 * pencerelerindeki düğmelerin Türkçe görünmesini garanti eder.
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
     * @brief Standart düğme metni ise Türkçe karşılığını döndürür.
     * @param context Çeviri bağlamı (kullanılmaz; metinler yalnızca Qt'ye aittir).
     * @param sourceText İngilizce kaynak metin.
     * @param disambiguation Ayırt edici not (kullanılmaz).
     * @param n Çoğul sayısı (kullanılmaz).
     * @return Türkçe metin; tanınmayan metin için boş (Qt bir sonraki çevirmene geçer).
     */
    QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr,
                      int n = -1) const override;

    /// Çevirmen her zaman kullanılabilir olduğundan false döndürür.
    bool isEmpty() const override;
};

/**
 * @brief Uygulamaya Türkçe çevirileri kurar.
 *
 * Varsa Qt'nin qtbase_tr.qm çevirisini (Qt kurulumu veya uygulama klasöründeki
 * translations/) yükler, ardından standart düğmeler için StandardButtonTranslator'ı kurar.
 * @param application Çevirilerin kurulacağı uygulama.
 */
void InstallTurkishTranslations(QApplication &application);

#endif // LOCALIZATION_H
