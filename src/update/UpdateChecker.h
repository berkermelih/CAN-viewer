#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVersionNumber>

class QNetworkAccessManager;
class QNetworkReply;

/// GitHub'daki bir yayının (release) özeti.
struct ReleaseInfo
{
    QString tag;            ///< Etiket (ör. "v1.1.0")
    QString name;           ///< Yayın başlığı
    QString notes;          ///< Yayın notları (Markdown)
    QVersionNumber version; ///< Etiketten çözülen sürüm
    QUrl pageUrl;           ///< Yayın sayfası
    QUrl downloadUrl;       ///< İlk .zip ekinin indirme adresi (yoksa boş)
};

/**
 * @brief GitHub Releases üzerinden yeni sürüm olup olmadığını denetler.
 *
 * "https://api.github.com/repos/<sahip>/<depo>/releases/latest" adresinden en son
 * yayın okunur ve etiketi AppInfo::Version ile karşılaştırılır. İstek
 * eşzamansızdır; sonuç sinyallerle bildirilir. Hiçbir kullanıcı bilgisi gönderilmez.
 */
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Denetleyiciyi oluşturur.
     * @param parent Qt sahiplik zinciri için üst nesne.
     */
    explicit UpdateChecker(QObject *parent = nullptr);

    /// Denetimi başlatır; süren bir denetim varsa yok sayılır.
    void CheckForUpdates();

    /// Bu derlemenin sürümünü döndürür.
    static QVersionNumber CurrentVersion();

signals:
    /// Daha yeni bir yayın bulunduğunda yayılır.
    void UpdateAvailable(const ReleaseInfo &release);

    /// Kullanılan sürüm güncel olduğunda yayılır.
    void UpToDate(const ReleaseInfo &latest);

    /// Depoda henüz yayın yoksa yayılır.
    void NoReleases();

    /// Ağ veya yanıt hatasında yayılır.
    void CheckFailed(const QString &reason);

private:
    /**
     * @brief GitHub yanıtını işler.
     * @param reply Tamamlanan istek.
     */
    void HandleReply(QNetworkReply *reply);

    QNetworkAccessManager *m_network = nullptr;
    bool m_busy = false;
};

#endif // UPDATECHECKER_H
