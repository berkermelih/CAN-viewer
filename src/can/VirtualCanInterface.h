#ifndef VIRTUALCANINTERFACE_H
#define VIRTUALCANINTERFACE_H

#include "ICanInterface.h"

#include <QElapsedTimer>

/**
 * @brief Donanım gerektirmeyen sanal (loopback) CAN arayüzü.
 *
 * Gönderilen her çerçeve, olay döngüsünün bir sonraki turunda alınmış gibi
 * geri bildirilir. Arayüzü ve filtreleri donanımsız denemek için kullanılır;
 * gerçek bir CANopen düğümünü simüle etmez.
 */
class VirtualCanInterface : public ICanInterface
{
    Q_OBJECT

public:
    /**
     * @brief Sanal arayüzü oluşturur.
     * @param parent Qt sahiplik zinciri için üst nesne.
     */
    explicit VirtualCanInterface(QObject *parent = nullptr);

    /**
     * @brief Sanal kanalı açar; kanal adı ve bit hızı yalnızca bilgi amaçlıdır.
     * @return Her zaman true.
     */
    bool Open(const QString &channel, int bitrate, QString &errorText) override;

    /// Sanal kanalı kapatır; bekleyen çerçeveler atılır.
    void Close() override;

    /// Kanalın açık olup olmadığını döndürür.
    bool IsOpen() const override;

    /**
     * @brief Çerçeveyi geri döngü kuyruğuna ekler.
     * @return Kanal açıksa true.
     */
    bool WriteFrame(const CanFrame &frame, QString &errorText) override;

    /// "Sanal (loopback)" adını döndürür.
    QString Name() const override;

    /// Kanal seçim listesinde sanal arayüzü temsil eden ad.
    static QString ChannelName();

private:
    /// Bekleyen çerçeveleri FramesReceived sinyaliyle yayar.
    void FlushPendingFrames();

    bool m_open = false;
    bool m_flushScheduled = false;
    QElapsedTimer m_clock;
    QVector<CanFrame> m_pendingFrames;
};

#endif // VIRTUALCANINTERFACE_H
