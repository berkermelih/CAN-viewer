#ifndef RXMESSAGEMODEL_H
#define RXMESSAGEMODEL_H

#include "../can/CanFrame.h"

#include <QAbstractTableModel>
#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QTimer>
#include <QVector>

#include <functional>

/**
 * @brief Alınan mesajlar tablosu (PCAN-View "Receive" görünümü).
 *
 * Her (ID, tip) için tek satır tutulur; yeni çerçeve geldikçe veri, sayaç ve
 * periyot güncellenir. Son sütun, ayarlanan yorumlayıcı fonksiyonun (CANopen,
 * PDO) ürettiği açıklamayı gösterir.
 */
class RxMessageModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    /// Tablo sütunları.
    enum Column
    {
        ColumnId,
        ColumnType,
        ColumnDlc,
        ColumnData,
        ColumnCycle,
        ColumnCount,
        ColumnTime,
        ColumnInterpretation,
        ColumnTotal
    };

    /// Çerçeve için açıklama üreten fonksiyon tipi.
    using InterpreterFunction = std::function<QString(const CanFrame &frame)>;

    /// Sıralama için ham değerlerin döndürüldüğü rol.
    static constexpr int SortRole = Qt::UserRole + 1;

    /**
     * @brief Boş modeli oluşturur.
     * @param parent Qt sahiplik zinciri için üst nesne.
     */
    explicit RxMessageModel(QObject *parent = nullptr);

    /// Satır sayısını (farklı ID sayısı) döndürür.
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /// Sütun sayısını döndürür.
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    /// Hücre verisini döndürür (görüntü, hizalama, sıralama rolleri).
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /// Sütun başlıklarını döndürür.
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /**
     * @brief Alınan çerçeveleri tabloya işler.
     * @param frames Alınan çerçeveler; yeni ID'ler için satır eklenir.
     */
    void AddFrames(const QVector<CanFrame> &frames);

    /// Tüm satırları siler.
    void Clear();

    /**
     * @brief Açıklama sütunu için yorumlayıcı fonksiyonu ayarlar.
     * @param interpreter Boş bırakılırsa açıklama sütunu boş kalır.
     */
    void SetInterpreter(InterpreterFunction interpreter);

    /// Mevcut satırların açıklamalarını yeniden hesaplar (ör. EDS yüklendikten sonra).
    void RefreshInterpretations();

    /**
     * @brief Satırdaki son çerçeveyi döndürür.
     * @param row Geçerli bir satır numarası olmalıdır.
     */
    const CanFrame &FrameAt(int row) const;

    /**
     * @brief Satırdaki çerçeveye işaretçi döndürür.
     * @return Satır geçersizse nullptr.
     */
    const CanFrame *FramePointerAt(int row) const;

    /**
     * @brief Verisi yeni değişen satırların veri sütununda kullanılacak rengi ayarlar.
     * @param color Geçersiz renk verilirse vurgu kapatılır.
     */
    void SetHighlightColor(const QColor &color);

    /**
     * @brief Verilen satırları siler (sıra ve tekrarlar önemli değildir).
     * @param rows Kaynak modeldeki satır numaraları.
     */
    void RemoveRows(QList<int> rows);

    /**
     * @brief ID ve veri sütunlarının gösterim biçimini ayarlar.
     * @param idFormat Hex veya onluk ID.
     * @param dataFormat Hex, onluk veya ASCII veri.
     */
    void SetDisplayFormat(IdFormat idFormat, DataFormat dataFormat);

private:
    struct Entry
    {
        CanFrame frame;
        quint64 count = 0;
        qint64 cycleUs = -1;
        qint64 dataChangedMs = -1; ///< Veri baytlarının son değiştiği an (m_clock, ms)
        QString interpretation;
    };

    /// Çerçeve ID'si ve tipinden satır anahtarı üretir.
    static quint32 MakeKey(const CanFrame &frame);

    /// İki çerçevenin DLC ve veri baytlarının aynı olup olmadığını döndürür.
    static bool SameData(const CanFrame &first, const CanFrame &second);

    /// Vurgu süresi dolan satırların yeniden çizilmesi için veri sütununu yeniler.
    void RefreshHighlights();

    QVector<Entry> m_entries;
    QHash<quint32, int> m_rowByKey;
    InterpreterFunction m_interpreter;
    QColor m_highlightColor;
    IdFormat m_idFormat = IdFormat::Hex;
    DataFormat m_dataFormat = DataFormat::Hex;
    QElapsedTimer m_clock;
    QTimer m_highlightTimer;
};

#endif // RXMESSAGEMODEL_H
