#ifndef CANOPENPANEL_H
#define CANOPENPANEL_H

#include "../canopen/CanOpenProtocol.h"
#include "../canopen/PdoDecoder.h"
#include "../eds/ObjectDictionary.h"

#include <QHash>
#include <QWidget>

#include <functional>

class QComboBox;
class QLabel;
class QLineEdit;
class QModelIndex;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QStandardItem;
class QStandardItemModel;
class QTreeView;
class SdoClient;

/**
 * @brief EDS görüntüleme ve CANopen işlemleri paneli.
 *
 * EDS nesne ağacını gösterir; seçili nesne için SDO okuma/yazma yapar,
 * NMT komutları ve SYNC gönderir. EDS'teki PDO eşlemeleri, Rx tablosundaki
 * çerçeveleri yorumlamak için InterpretFrame() ile dışarıya sunulur.
 */
class CanOpenPanel : public QWidget
{
    Q_OBJECT

public:
    /// Çerçeve gönderim fonksiyonu tipi; başarısızsa errorText doldurulur.
    using SendFunction = std::function<bool(const CanFrame &frame, QString &errorText)>;

    /**
     * @brief Paneli oluşturur.
     * @param parent Üst widget.
     */
    explicit CanOpenPanel(QWidget *parent = nullptr);

    /// SDO/NMT çerçevelerini göndermek için kullanılacak fonksiyonu ayarlar.
    void SetSendFunction(SendFunction function);

    /**
     * @brief EDS dosyasını açar, ağacı ve PDO çözücüyü günceller.
     * @param filePath .eds dosyasının yolu.
     * @return Dosya başarıyla yüklendiyse true; hata kullanıcıya gösterilir.
     */
    bool OpenEdsFile(const QString &filePath);

    /// Dosya seçme diyaloğu açar ve seçilen EDS dosyasını yükler.
    void BrowseEdsFile();

    /**
     * @brief Alınan çerçeveleri SDO istemcisine iletir.
     * @param frames Alınan çerçeveler.
     */
    void HandleFrames(const QVector<CanFrame> &frames);

    /**
     * @brief Çerçeveyi CANopen ve (EDS yüklüyse) PDO eşlemesine göre yorumlar.
     * @param frame Yorumlanacak çerçeve.
     * @return Açıklama metni; tanınmazsa boş.
     */
    QString InterpretFrame(const CanFrame &frame) const;

    /// Bağlantı durumuna göre SDO/NMT düğmelerini etkinleştirir.
    void SetConnected(bool connected);

signals:
    /// EDS veya düğüm numarası değiştiğinde, yani çerçeve yorumları değiştiğinde yayılır.
    void InterpretationChanged();

private:
    /// Panel bileşenlerini oluşturur ve yerleştirir.
    void BuildUi();

    /// EDS ağacını nesne sözlüğünden yeniden oluşturur.
    void PopulateTree();

    /// Ağaçta seçilen girdinin index/sub/tip bilgisini SDO alanlarına aktarır.
    void HandleTreeSelection(const QModelIndex &current);

    /// SDO alanlarındaki hedefi okur (upload).
    void ReadSelectedObject();

    /// SDO alanlarındaki değeri hedefe yazar (download).
    void WriteSelectedObject();

    /**
     * @brief NMT komutunu seçili hedefe gönderir.
     * @param command Gönderilecek NMT komutu.
     */
    void SendNmtCommand(CanOpen::NmtCommand command);

    /// SYNC çerçevesi gönderir.
    void SendSync();

    /**
     * @brief SDO alanlarındaki index ve subindex'i çözer.
     * @return Değerler geçerliyse true; değilse günlüğe hata yazılır.
     */
    bool ReadSdoTarget(quint16 &index, quint8 &subIndex);

    /// Okuma tamamlandığında değeri gösterir ve ağaçtaki "Okunan" sütununu günceller.
    void HandleUploadFinished(quint8 nodeId, quint16 index, quint8 subIndex, const QByteArray &data);

    /// Yazma tamamlandığında günlüğe bilgi yazar.
    void HandleDownloadFinished(quint8 nodeId, quint16 index, quint8 subIndex);

    /// SDO hatasını günlüğe yazar.
    void HandleTransferFailed(quint8 nodeId, quint16 index, quint8 subIndex, const QString &reason);

    /// PDO çözücüyü güncel EDS ve düğüm numarasıyla yeniden oluşturur.
    void RebuildPdoDecoder();

    /// Zaman damgalı bir satırı günlüğe ekler.
    void AppendLog(const QString &text);

    /// SDO düğmelerini bağlantı ve meşguliyet durumuna göre etkinleştirir.
    void UpdateButtonStates();

    /// (index, sub) için "Okunan değer" hücresi anahtarı üretir.
    static quint32 ValueKey(quint16 index, quint8 subIndex);

    /// "0x1018:01" biçiminde adres metni üretir.
    static QString AddressText(quint16 index, quint8 subIndex);

    ObjectDictionary m_dictionary;
    PdoDecoder m_pdoDecoder;
    SdoClient *m_sdoClient = nullptr;
    SendFunction m_send;
    bool m_connected = false;

    QStandardItemModel *m_treeModel = nullptr;
    QTreeView *m_treeView = nullptr;
    QLabel *m_fileLabel = nullptr;
    QSpinBox *m_nodeIdSpin = nullptr;
    QLineEdit *m_indexEdit = nullptr;
    QLineEdit *m_subIndexEdit = nullptr;
    QComboBox *m_dataTypeCombo = nullptr;
    QLineEdit *m_valueEdit = nullptr;
    QPushButton *m_readButton = nullptr;
    QPushButton *m_writeButton = nullptr;
    QSpinBox *m_nmtTargetSpin = nullptr;
    QList<QPushButton *> m_busButtons;
    QPlainTextEdit *m_log = nullptr;
    QHash<quint32, QStandardItem *> m_valueItems;
};

#endif // CANOPENPANEL_H
