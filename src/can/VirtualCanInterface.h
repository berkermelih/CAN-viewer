#ifndef VIRTUALCANINTERFACE_H
#define VIRTUALCANINTERFACE_H

#include "ICanInterface.h"

#include <QElapsedTimer>

/**
 * @brief Donanım gerektirmeyen sanal (loopback) CAN arayüzü.
 *
 * Gönderilen her çerçeve, olay döngüsünün bir sonraki turunda alınmış gibi
 * geri bildirilir. Yalnızca testlerde ve donanımsız geliştirmede kullanılır;
 * uygulamanın port listesinde gösterilmez. Gerçek bir CANopen düğümünü simüle etmez.
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
     * @brief Sanal kanalı açar; ayarlardan yalnızca CAN FD modu dikkate alınır.
     * @return Her zaman true.
     */
    bool Open(const CanBusSettings &settings, QString &errorText) override;

    /// Kanal CAN FD modunda açıldıysa true döndürür.
    bool IsFdMode() const override;

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

private:
    /// Bekleyen çerçeveleri FramesReceived sinyaliyle yayar.
    void FlushPendingFrames();

    bool m_open = false;
    bool m_fd = false;
    bool m_flushScheduled = false;
    QElapsedTimer m_clock;
    QVector<CanFrame> m_pendingFrames;
};

#endif // VIRTUALCANINTERFACE_H
