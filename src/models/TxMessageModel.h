#ifndef TXMESSAGEMODEL_H
#define TXMESSAGEMODEL_H

#include "../can/CanFrame.h"

#include <QAbstractTableModel>
#include <QString>
#include <QVector>

/// Gönderim listesindeki tek bir mesaj.
struct TxMessage
{
    CanFrame frame;
    int periodMs = 0;            ///< 0: yalnızca elle gönderim, >0: periyodik (cycle time)
    bool periodicEnabled = true; ///< Periyodik gönderim etkin mi (false: "Paused")
    quint64 count = 0;           ///< Gönderilme sayısı
    QString comment;
    qint64 nextDueMs = 0;        ///< Bir sonraki periyodik gönderim zamanı (iç kullanım)
};

/**
 * @brief Gönderim (Transmit) mesaj listesi modeli.
 *
 * ID sütunundaki onay kutusu periyodik gönderimi açar/kapatır. Periyodik
 * zamanlama TakeDueRows() ile dışarıdaki bir zamanlayıcıdan yürütülür.
 */
class TxMessageModel : public QAbstractTableModel
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
        ColumnPeriod,
        ColumnCount,
        ColumnComment,
        ColumnTotal
    };

    /**
     * @brief Boş modeli oluşturur.
     * @param parent Qt sahiplik zinciri için üst nesne.
     */
    explicit TxMessageModel(QObject *parent = nullptr);

    /// Mesaj sayısını döndürür.
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /// Sütun sayısını döndürür.
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    /// Hücre verisini döndürür (görüntü, onay kutusu, hizalama).
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /// Sütun başlıklarını döndürür.
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /// ID sütununu onay kutulu yapar.
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    /// Onay kutusu değişikliğini (periyodik gönderim aç/kapat) uygular.
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    /**
     * @brief Listeye yeni mesaj ekler.
     * @return Eklenen satır numarası.
     */
    int AddMessage(const TxMessage &message);

    /**
     * @brief Birden çok mesajı verilen satırdan itibaren ekler (yapıştırma için).
     * @param row Ekleme konumu; geçersizse sona eklenir.
     * @param messages Eklenecek mesajlar.
     * @return İlk eklenen satır; liste boşsa -1.
     */
    int InsertMessages(int row, const QVector<TxMessage> &messages);

    /**
     * @brief Tüm listeyi verilen mesajlarla değiştirir (dosyadan açma).
     * @param messages Yeni liste.
     */
    void SetMessages(const QVector<TxMessage> &messages);

    /// Tüm mesajları siler.
    void Clear();

    /// Listedeki tüm mesajları döndürür (dosyaya kaydetme için).
    const QVector<TxMessage> &Messages() const;

    /**
     * @brief ID ve veri sütunlarının gösterim biçimini ayarlar.
     * @param idFormat Hex veya onluk ID.
     * @param dataFormat Hex, onluk veya ASCII veri.
     */
    void SetDisplayFormat(IdFormat idFormat, DataFormat dataFormat);

    /**
     * @brief Var olan mesajı günceller (sayaç korunur).
     * @param row Satır numarası.
     * @param message Yeni içerik.
     */
    void UpdateMessage(int row, const TxMessage &message);

    /// Verilen satırdaki mesajı siler.
    void RemoveMessage(int row);

    /// Verilen satırdaki mesajı döndürür (row geçerli olmalıdır).
    const TxMessage &MessageAt(int row) const;

    /// Mesajın gönderildiğini kaydeder (sayaç artırılır).
    void MarkSent(int row);

    /**
     * @brief Zamanı gelmiş periyodik mesajların satırlarını döndürür ve zamanlarını ilerletir.
     * @param nowMs Monoton saat (ms).
     */
    QVector<int> TakeDueRows(qint64 nowMs);

    /**
     * @brief Tüm periyodik mesajların zamanlamasını verilen andan başlatır.
     * @param nowMs Monoton saat (ms).
     */
    void ResetSchedule(qint64 nowMs);

    /// Tüm gönderim sayaçlarını sıfırlar.
    void ResetCounters();

signals:
    /// Liste içeriği (mesaj ekleme, silme, düzenleme, duraklatma) değiştiğinde yayılır; sayaç değişimleri hariç.
    void ContentChanged();

private:
    QVector<TxMessage> m_messages;
    IdFormat m_idFormat = IdFormat::Hex;
    DataFormat m_dataFormat = DataFormat::Hex;
};

#endif // TXMESSAGEMODEL_H
