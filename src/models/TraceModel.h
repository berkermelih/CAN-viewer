#ifndef TRACEMODEL_H
#define TRACEMODEL_H

#include "../can/CanFrame.h"

#include <QAbstractTableModel>
#include <QColor>
#include <QHash>
#include <QTimer>
#include <QVector>

#include <deque>
#include <functional>

/// Trace kaydındaki çerçevenin yönü.
enum class TraceDirection
{
    Rx, ///< Bus'tan alınan
    Tx  ///< Bu uygulamadan gönderilen
};

/**
 * @brief Tüm Rx/Tx çerçevelerinin kronolojik kaydı (PCAN-View "Trace").
 *
 * Gelen çerçeveler önce bir bekleme kuyruğuna alınır ve 50 ms'de bir toplu
 * olarak tabloya eklenir; böylece yoğun bus yükünde arayüz akıcı kalır.
 * Kayıt sayısı üst sınırı aşınca en eski kayıtlar silinir. Zaman damgası,
 * Rx ve Tx için ortak olan uygulama saatinden (kayıt başlangıcına göre) alınır.
 */
class TraceModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    /// Tablo sütunları.
    enum Column
    {
        ColumnNumber,
        ColumnTime,
        ColumnDelta,
        ColumnDirection,
        ColumnId,
        ColumnType,
        ColumnDlc,
        ColumnData,
        ColumnInterpretation,
        ColumnTotal
    };

    /// Çerçeve için açıklama üreten fonksiyon tipi.
    using InterpreterFunction = std::function<QString(const CanFrame &frame)>;

    /**
     * @brief Boş trace modelini oluşturur.
     * @param parent Qt sahiplik zinciri için üst nesne.
     */
    explicit TraceModel(QObject *parent = nullptr);

    /// Kayıt sayısını döndürür.
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /// Sütun sayısını döndürür.
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    /// Hücre verisini döndürür (görüntü, renk, hizalama).
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /// Sütun başlıklarını döndürür.
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /**
     * @brief Çerçeveleri kayda eklenmek üzere kuyruğa alır (duraklatılmışsa yok sayar).
     * @param frames Çerçeveler.
     * @param direction Rx veya Tx.
     * @param timeUs Uygulama saatine göre zaman (µs).
     */
    void AppendFrames(const QVector<CanFrame> &frames, TraceDirection direction, qint64 timeUs);

    /// Kaydı duraklatır veya sürdürür.
    void SetPaused(bool paused);

    /// Kaydın duraklatılmış olup olmadığını döndürür.
    bool IsPaused() const;

    /// Tüm kayıtları ve bekleyen çerçeveleri siler; numaralandırma ve zaman sıfırlanır.
    void Clear();

    /**
     * @brief Tutulacak en fazla kayıt sayısını ayarlar.
     * @param maximumRows Sınır aşılınca en eski kayıtlar silinir.
     */
    void SetMaximumRows(int maximumRows);

    /// Açıklama sütunu için yorumlayıcı fonksiyonu ayarlar.
    void SetInterpreter(InterpreterFunction interpreter);

    /**
     * @brief Tx yönü ve sayaçlar için kullanılacak vurgu renklerini ayarlar.
     * @param rxColor Rx yön metni rengi.
     * @param txColor Tx yön metni rengi.
     */
    void SetDirectionColors(const QColor &rxColor, const QColor &txColor);

    /**
     * @brief ID ve veri sütunlarının gösterim biçimini ayarlar (CSV her zaman hex yazılır).
     * @param idFormat Hex veya onluk ID.
     * @param dataFormat Hex, onluk veya ASCII veri.
     */
    void SetDisplayFormat(IdFormat idFormat, DataFormat dataFormat);

    /**
     * @brief Satırdaki çerçeveye işaretçi döndürür.
     * @return Satır geçersizse nullptr.
     */
    const CanFrame *FramePointerAt(int row) const;

    /// CSV dosyası için başlık satırını döndürür.
    static QString CsvHeader();

    /**
     * @brief Satırı CSV satırına çevirir (ayraç ';').
     * @param row Geçerli bir satır numarası olmalıdır.
     */
    QString CsvLine(int row) const;

    /// Kuyruktaki çerçeveleri tabloya ekler ve sınırı aşan eski kayıtları siler.
    void FlushPending();

private:
    struct Entry
    {
        CanFrame frame;
        TraceDirection direction = TraceDirection::Rx;
        qint64 timeUs = 0;
        qint64 deltaUs = -1; ///< Aynı ID'nin bir önceki kaydına göre süre
        quint64 number = 0;
    };

    std::deque<Entry> m_entries;
    QVector<Entry> m_pending;
    QHash<quint64, qint64> m_lastTimeByKey;
    InterpreterFunction m_interpreter;
    QColor m_rxColor;
    QColor m_txColor;
    IdFormat m_idFormat = IdFormat::Hex;
    DataFormat m_dataFormat = DataFormat::Hex;
    QTimer m_flushTimer;
    quint64 m_nextNumber = 1;
    int m_maximumRows = 100000;
    bool m_paused = false;
};

#endif // TRACEMODEL_H
