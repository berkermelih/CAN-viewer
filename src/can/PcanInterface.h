#ifndef PCANINTERFACE_H
#define PCANINTERFACE_H

#include "ICanInterface.h"

#include <QLibrary>
#include <QList>
#include <QTimer>

#if defined(Q_OS_WIN)
#define PCAN_CALL __stdcall
#else
#define PCAN_CALL
#endif

/// Bilgisayara bağlı (algılanan) bir PCAN kanalının bilgileri.
struct PcanChannelInfo
{
    quint16 handle = 0;      ///< PCAN-Basic kanal handle değeri (ör. 0x51 = PCAN_USBBUS1)
    QString deviceName;      ///< Donanım adı (ör. "PCAN-USB FD"); bilinmiyorsa boş
    int channelNumber = 1;   ///< Cihaz üzerindeki kanal numarası (1'den başlar)
    quint32 deviceId = 0;    ///< Kullanıcı tarafından atanabilen cihaz numarası
    bool fdCapable = false;  ///< Kanal CAN FD destekliyor mu
    bool occupied = false;   ///< Kanal başka bir uygulama tarafından kullanılıyor mu

    /// Port listesinde gösterilecek metni döndürür (ör. "PCAN-USB FD · Kanal 1").
    QString DisplayName() const;
};

/**
 * @brief PEAK PCAN donanımı için CAN arayüzü.
 *
 * PCAN-Basic kütüphanesi (Windows: PCANBasic.dll, Linux: libpcanbasic.so)
 * çalışma zamanında QLibrary ile yüklenir; derleme için PEAK başlık dosyaları
 * gerekmez. Alım kuyruğu bloklamayan bir zamanlayıcı ile periyodik okunur.
 * Klasik CAN (CAN_Initialize/Read/Write) ve CAN FD (CAN_InitializeFD/ReadFD/WriteFD)
 * desteklenir. CAN XL PCAN-Basic tarafından sunulmadığı için desteklenmez.
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
     * @param settings Kanal handle değeri (onluk metin), bit hızı ve CAN FD ayarları.
     * @param errorText Hata durumunda açıklama (DLL bulunamadı, kanal meşgul vb.).
     * @return Kanal başlatıldıysa true.
     */
    bool Open(const CanBusSettings &settings, QString &errorText) override;

    /// Kanalı PCAN-Basic üzerinden kapatır ve zamanlayıcıları durdurur.
    void Close() override;

    /// Kanalın açık olup olmadığını döndürür.
    bool IsOpen() const override;

    /// Kanal CAN FD modunda açıldıysa true döndürür.
    bool IsFdMode() const override;

    /**
     * @brief Çerçeveyi CAN_Write (klasik mod) veya CAN_WriteFD (FD modu) ile gönderir.
     * @param frame Gönderilecek çerçeve; FD çerçevesi yalnızca FD modunda gönderilebilir.
     * @param errorText Hata durumunda PCAN hata metni.
     * @return Gönderim başarılıysa true.
     */
    bool WriteFrame(const CanFrame &frame, QString &errorText) override;

    /// "PCAN (USBBUS1)" biçiminde ad döndürür.
    QString Name() const override;

    /**
     * @brief Bilgisayara bağlı PCAN kanallarını algılar.
     *
     * Önce PCAN_ATTACHED_CHANNELS sorgulanır; sürücü bunu desteklemiyorsa
     * PCAN-USB kanalları (USBBUS1..16) tek tek PCAN_CHANNEL_CONDITION ile yoklanır.
     * @param errorText Kütüphane yüklenemezse açıklama; aksi halde boş bırakılır.
     * @return Algılanan kanallar (boş olabilir).
     */
    static QList<PcanChannelInfo> DetectChannels(QString &errorText);

    /// Klasik CAN için desteklenen bit hızlarını (bit/s) büyükten küçüğe döndürür.
    static QList<int> SupportedBitrates();

    /// CAN FD nominal (arbitration) fazı için desteklenen bit hızlarını döndürür.
    static QList<int> SupportedFdBitrates();

    /// CAN FD veri fazı için desteklenen bit hızlarını döndürür.
    static QList<int> SupportedFdDataBitrates();

private:
    /// PCAN-Basic TPCANMsg yapısının birebir karşılığı.
    struct PcanMessage
    {
        quint32 id;
        quint8 messageType;
        quint8 length;
        quint8 data[8];
    };

    /// PCAN-Basic TPCANMsgFD yapısının birebir karşılığı (DLC alanı DLC kodudur).
    struct PcanMessageFd
    {
        quint32 id;
        quint8 messageType;
        quint8 dlc;
        quint8 data[64];
    };

    /// PCAN-Basic TPCANTimestamp yapısının birebir karşılığı.
    struct PcanTimestamp
    {
        quint32 millis;
        quint16 millisOverflow;
        quint16 micros;
    };

    typedef quint32(PCAN_CALL *InitializeFunction)(quint16, quint16, quint8, quint32, quint16);
    typedef quint32(PCAN_CALL *InitializeFdFunction)(quint16, char *);
    typedef quint32(PCAN_CALL *UninitializeFunction)(quint16);
    typedef quint32(PCAN_CALL *ReadFunction)(quint16, PcanMessage *, PcanTimestamp *);
    typedef quint32(PCAN_CALL *ReadFdFunction)(quint16, PcanMessageFd *, quint64 *);
    typedef quint32(PCAN_CALL *WriteFunction)(quint16, PcanMessage *);
    typedef quint32(PCAN_CALL *WriteFdFunction)(quint16, PcanMessageFd *);
    typedef quint32(PCAN_CALL *GetStatusFunction)(quint16);
    typedef quint32(PCAN_CALL *GetErrorTextFunction)(quint32, quint16, char *);
    typedef quint32(PCAN_CALL *ChannelGetValueFunction)(quint16, quint8, void *, quint32);

    /**
     * @brief PCAN-Basic kütüphanesini yükler ve gerekli fonksiyonları çözümler.
     * @param errorText Kütüphane veya fonksiyon bulunamazsa açıklama.
     * @return Klasik CAN fonksiyonlarının tümü çözümlendiyse true (FD fonksiyonları isteğe bağlıdır).
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

    /**
     * @brief Okunan zaman damgasını ilk çerçeveye göre göreli mikrosaniyeye çevirir.
     * @param timeUs Sürücünün verdiği mutlak zaman (µs).
     */
    qint64 RelativeTimestamp(quint64 timeUs);

    /// Bus durumunu sorgular; değiştiyse BusStatusChanged yayar. Donanım sökülmüşse HandleDeviceLost çağırır.
    void PollBusStatus();

    /**
     * @brief Donanımın hâlâ bağlı olup olmadığını denetler.
     *
     * PCAN_CHANNEL_CONDITION sorgulanır; kanal artık "kullanılabilir / kullanımda"
     * değilse veya sürücü geçersiz donanım / başlatılmamış kanal hatası döndürürse
     * donanım sökülmüş kabul edilir.
     * @param lastStatus Son PCAN-Basic çağrısının durum kodu.
     * @return Donanım kayıpsa true.
     */
    bool IsDeviceLost(quint32 lastStatus) const;

    /**
     * @brief Kanalı kapatır ve ConnectionLost sinyalini (bir kez) yayar.
     * @param reason Okunabilir neden.
     */
    void HandleDeviceLost(const QString &reason);

    QLibrary m_library;
    InitializeFunction m_initialize = nullptr;
    InitializeFdFunction m_initializeFd = nullptr;
    UninitializeFunction m_uninitialize = nullptr;
    ReadFunction m_read = nullptr;
    ReadFdFunction m_readFd = nullptr;
    WriteFunction m_write = nullptr;
    WriteFdFunction m_writeFd = nullptr;
    GetStatusFunction m_getStatus = nullptr;
    GetErrorTextFunction m_getErrorText = nullptr;
    ChannelGetValueFunction m_getValue = nullptr;

    QString m_channelName;
    quint16 m_channelHandle = 0;
    bool m_open = false;
    bool m_fdMode = false;
    quint32 m_lastBusStatus = 0xFFFFFFFFu;
    bool m_hasFirstTimestamp = false;
    quint64 m_firstTimestampUs = 0;
    QTimer m_receiveTimer;
    QTimer m_statusTimer;
};

#endif // PCANINTERFACE_H
