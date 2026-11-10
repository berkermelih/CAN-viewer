#ifndef ICANINTERFACE_H
#define ICANINTERFACE_H

#include "CanFrame.h"

#include <QObject>
#include <QString>
#include <QVector>

/// Bir CAN kanalını açmak için gereken ayarlar.
struct CanBusSettings
{
    QString channel;          ///< Kanal tanımlayıcısı (PCAN için kanal handle değeri, ör. "81")
    int bitrate = 500000;     ///< Nominal (arbitration) bit hızı, bit/s
    bool fd = false;          ///< true ise kanal CAN FD modunda açılır
    int dataBitrate = 2000000; ///< CAN FD veri fazı bit hızı, bit/s (yalnızca fd = true iken)
};

/**
 * @brief CAN donanım arayüzleri için soyut temel sınıf.
 *
 * Uygulamanın geri kalanı yalnızca bu arayüzü bilir; PCAN, sanal (loopback,
 * yalnızca test için) veya ileride eklenecek başka arayüzler bu sınıftan türetilir.
 * Alınan çerçeveler FramesReceived sinyali ile toplu olarak bildirilir.
 */
class ICanInterface : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Arayüz nesnesini oluşturur.
     * @param parent Qt sahiplik zinciri için üst nesne.
     */
    explicit ICanInterface(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    /// Sanal yıkıcı; türetilmiş sınıflar açık kanalı kapatmalıdır.
    ~ICanInterface() override = default;

    /**
     * @brief CAN kanalını verilen ayarlarla açar.
     * @param settings Kanal, bit hızı ve CAN FD ayarları.
     * @param errorText Hata durumunda okunabilir açıklama yazılır.
     * @return Kanal açıldıysa true.
     */
    virtual bool Open(const CanBusSettings &settings, QString &errorText) = 0;

    /// Kanal CAN FD modunda açıldıysa true döndürür.
    virtual bool IsFdMode() const = 0;

    /// Kanal açıksa kapatır ve alım işlemlerini durdurur.
    virtual void Close() = 0;

    /// Kanalın açık olup olmadığını döndürür.
    virtual bool IsOpen() const = 0;

    /**
     * @brief Bir CAN çerçevesini bus'a gönderir.
     * @param frame Gönderilecek çerçeve.
     * @param errorText Hata durumunda okunabilir açıklama yazılır.
     * @return Çerçeve gönderim kuyruğuna alındıysa true.
     */
    virtual bool WriteFrame(const CanFrame &frame, QString &errorText) = 0;

    /// Arayüzün kullanıcıya gösterilecek adını döndürür.
    virtual QString Name() const = 0;

signals:
    /**
     * @brief Bir veya daha fazla çerçeve alındığında yayılır.
     * @param frames Alınan çerçeveler (geliş sırasıyla).
     */
    void FramesReceived(const QVector<CanFrame> &frames);

    /**
     * @brief Çalışma sırasında bir hata oluştuğunda yayılır.
     * @param text Okunabilir hata açıklaması.
     */
    void ErrorOccurred(const QString &text);

    /**
     * @brief Bus durumu değiştiğinde yayılır (ör. "Bus OK", "Bus Off").
     * @param statusText Okunabilir bus durumu.
     */
    void BusStatusChanged(const QString &statusText);
};

#endif // ICANINTERFACE_H
