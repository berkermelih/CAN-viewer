#ifndef PCANINTERFACE_H
#define PCANINTERFACE_H

#include "ICanInterface.h"

#include <QLibrary>
#include <QList>
#include <QPair>
#include <QTimer>

#if defined(Q_OS_WIN)
#define PCAN_CALL __stdcall
#else
#define PCAN_CALL
#endif

/**
 * @brief PEAK PCAN donanımı için CAN arayüzü.
 *
 * PCAN-Basic kütüphanesi (Windows: PCANBasic.dll, Linux: libpcanbasic.so)
 * çalışma zamanında QLibrary ile yüklenir; derleme için PEAK başlık dosyaları
 * gerekmez. Alım kuyruğu bloklamayan bir zamanlayıcı ile periyodik okunur.
 * Bu sürüm yalnızca klasik CAN destekler (CAN FD yok).
 */
class PcanInterface : public ICanInterface
{
    Q_OBJECT

public:
    /**
     * @brief PCAN arayüz nesnesini oluşturur (kütüphane henüz yüklenmez).
     * @param parent Qt sahiplik zinciri için üst nesne.
     */
    explicit PcanInterface(QObject *parent = nullptr);

    /// Açık kanalı kapatır.
    ~PcanInterface() override;

    /**
     * @brief PCAN kütüphanesini yükler ve kanalı başlatır.
     * @param channel Kanal adı (ör. "PCAN_USBBUS1"), bkz. AvailableChannels().
     * @param bitrate Bit hızı (bit/s), bkz. SupportedBitrates().
     * @param errorText Hata durumunda açıklama (DLL bulunamadı, kanal meşgul vb.).
     * @return Kanal başlatıldıysa true.
     */
    bool Open(const QString &channel, int bitrate, QString &errorText) override;

    /// Kanalı PCAN-Basic üzerinden kapatır ve zamanlayıcıları durdurur.
    void Close() override;

    /// Kanalın açık olup olmadığını döndürür.
    bool IsOpen() const override;

    /**
     * @brief Çerçeveyi CAN_Write ile gönderir.
     * @param frame Gönderilecek çerçeve.
     * @param errorText Hata durumunda PCAN hata metni.
     * @return Gönderim başarılıysa true.
     */
    bool WriteFrame(const CanFrame &frame, QString &errorText) override;

    /// "PCAN" adını ve açık kanalı döndürür.
    QString Name() const override;

    /**
     * @brief Desteklenen PCAN-USB kanallarını döndürür.
     * @return (kanal adı, görünen ad) çiftleri; ör. ("PCAN_USBBUS1", "PCAN-USB 1").
     */
    static QList<QPair<QString, QString>> AvailableChannels();

    /// Desteklenen bit hızlarını (bit/s) büyükten küçüğe döndürür.
    static QList<int> SupportedBitrates();

private:
    /// PCAN-Basic TPCANMsg yapısının birebir karşılığı.
    struct PcanMessage
    {
        quint32 id;
        quint8 messageType;
        quint8 length;
        quint8 data[8];
    };

    /// PCAN-Basic TPCANTimestamp yapısının birebir karşılığı.
    struct PcanTimestamp
    {
        quint32 millis;
        quint16 millisOverflow;
        quint16 micros;
    };

    typedef quint32(PCAN_CALL *InitializeFunction)(quint16, quint16, quint8, quint32, quint16);
    typedef quint32(PCAN_CALL *UninitializeFunction)(quint16);
    typedef quint32(PCAN_CALL *ReadFunction)(quint16, PcanMessage *, PcanTimestamp *);
    typedef quint32(PCAN_CALL *WriteFunction)(quint16, PcanMessage *);
    typedef quint32(PCAN_CALL *GetStatusFunction)(quint16);
    typedef quint32(PCAN_CALL *GetErrorTextFunction)(quint32, quint16, char *);

    /**
     * @brief PCAN-Basic kütüphanesini yükler ve gerekli fonksiyonları çözümler.
     * @param errorText Kütüphane veya fonksiyon bulunamazsa açıklama.
     * @return Tüm fonksiyonlar çözümlendiyse true.
     */
    bool LoadPcanLibrary(QString &errorText);

    /**
     * @brief PCAN durum kodunu PCAN-Basic'in hata metnine çevirir.
     * @param status TPCANStatus değeri.
     */
    QString ErrorText(quint32 status) const;

    /**
     * @brief Bus durum kodunu kısa okunabilir metne çevirir (Bus OK, Bus Off ...).
     * @param status CAN_GetStatus dönüş değeri.
     */
    QString BusStatusText(quint32 status) const;

    /// Alım kuyruğunu boşalana kadar okur ve FramesReceived yayar.
    void PollReceiveQueue();

    /// Bus durumunu sorgular; değiştiyse BusStatusChanged yayar.
    void PollBusStatus();

    QLibrary m_library;
    InitializeFunction m_initialize = nullptr;
    UninitializeFunction m_uninitialize = nullptr;
    ReadFunction m_read = nullptr;
    WriteFunction m_write = nullptr;
    GetStatusFunction m_getStatus = nullptr;
    GetErrorTextFunction m_getErrorText = nullptr;

    QString m_channelName;
    quint16 m_channelHandle = 0;
    bool m_open = false;
    quint32 m_lastBusStatus = 0xFFFFFFFFu;
    bool m_hasFirstTimestamp = false;
    quint64 m_firstTimestampUs = 0;
    QTimer m_receiveTimer;
    QTimer m_statusTimer;
};

#endif // PCANINTERFACE_H
