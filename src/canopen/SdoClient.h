#ifndef SDOCLIENT_H
#define SDOCLIENT_H

#include "../can/CanFrame.h"

#include <QByteArray>
#include <QObject>
#include <QTimer>

#include <array>
#include <functional>

/**
 * @brief CiA 301 SDO istemcisi (expedited ve segmented aktarım).
 *
 * Aynı anda tek bir aktarım yürütülür. Gönderim, SetSendFunction ile verilen
 * fonksiyon üzerinden yapılır; alınan çerçeveler HandleFrame ile beslenir.
 * Blok aktarım (block transfer) bu sürümde desteklenmez.
 */
class SdoClient : public QObject
{
    Q_OBJECT

public:
    /// Çerçeve gönderim fonksiyonu tipi; başarısızsa errorText doldurulur.
    using SendFunction = std::function<bool(const CanFrame &frame, QString &errorText)>;

    /**
     * @brief SDO istemcisini oluşturur.
     * @param parent Qt sahiplik zinciri için üst nesne.
     */
    explicit SdoClient(QObject *parent = nullptr);

    /// Çerçeve gönderimi için kullanılacak fonksiyonu ayarlar.
    void SetSendFunction(SendFunction function);

    /// Yanıt bekleme süresini (ms) ayarlar; varsayılan 1000 ms.
    void SetTimeoutMs(int timeoutMs);

    /// Bir aktarım sürüyorsa true döndürür.
    bool IsBusy() const;

    /**
     * @brief Uzak düğümden bir nesneyi okumayı (upload) başlatır.
     * @param nodeId Hedef düğüm (1..127).
     * @param index Nesne index'i.
     * @param subIndex Alt index.
     * @param errorText Başlatılamazsa açıklama.
     * @return İstek gönderildiyse true; sonuç UploadFinished/TransferFailed ile bildirilir.
     */
    bool StartUpload(quint8 nodeId, quint16 index, quint8 subIndex, QString &errorText);

    /**
     * @brief Uzak düğümdeki bir nesneye yazmayı (download) başlatır.
     * @param nodeId Hedef düğüm (1..127).
     * @param index Nesne index'i.
     * @param subIndex Alt index.
     * @param data Yazılacak little-endian veri; 4 bayta kadar expedited, fazlası segmented.
     * @param errorText Başlatılamazsa açıklama.
     * @return İstek gönderildiyse true; sonuç DownloadFinished/TransferFailed ile bildirilir.
     */
    bool StartDownload(quint8 nodeId, quint16 index, quint8 subIndex, const QByteArray &data, QString &errorText);

    /// Süren aktarımı abort göndererek iptal eder.
    void Cancel();

    /**
     * @brief Alınan bir çerçeveyi işler; ilgili SDO yanıtı değilse yok sayar.
     * @param frame Alınan çerçeve.
     */
    void HandleFrame(const CanFrame &frame);

signals:
    /// Okuma tamamlandığında yayılır.
    void UploadFinished(quint8 nodeId, quint16 index, quint8 subIndex, const QByteArray &data);

    /// Yazma tamamlandığında yayılır.
    void DownloadFinished(quint8 nodeId, quint16 index, quint8 subIndex);

    /// Aktarım abort, zaman aşımı veya protokol hatasıyla bittiğinde yayılır.
    void TransferFailed(quint8 nodeId, quint16 index, quint8 subIndex, const QString &reason);

private:
    enum class State
    {
        Idle,
        UploadInitiate,
        UploadSegment,
        DownloadInitiate,
        DownloadSegment
    };

    /// 8 baytlık SDO isteğini gönderir ve zaman aşımı sayacını yeniden başlatır.
    bool SendRequest(const std::array<quint8, 8> &payload, QString &errorText);

    /// Bir sonraki yazma segmentini gönderir.
    void SendNextDownloadSegment();

    /// Upload segment isteği gönderir.
    void RequestNextUploadSegment();

    /// Abort çerçevesi gönderir.
    void SendAbort(quint32 abortCode);

    /// Aktarımı hatayla bitirir ve TransferFailed yayar.
    void Fail(const QString &reason);

    /// Zaman aşımında abort gönderip aktarımı hatayla bitirir.
    void HandleTimeout();

    /// Yanıttaki index/subindex'in süren aktarımla eşleşip eşleşmediğini döndürür.
    bool MatchesAddress(const CanFrame &frame) const;

    SendFunction m_send;
    QTimer m_timeoutTimer;
    State m_state = State::Idle;
    quint8 m_nodeId = 0;
    quint16 m_index = 0;
    quint8 m_subIndex = 0;
    bool m_toggle = false;
    QByteArray m_buffer;
    int m_offset = 0;
    int m_expectedSize = -1;
};

#endif // SDOCLIENT_H
