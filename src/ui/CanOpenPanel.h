#ifndef CANOPENPANEL_H
#define CANOPENPANEL_H

#include "../canopen/CanOpenProtocol.h"
#include "../canopen/PdoDecoder.h"
#include "../eds/ObjectDictionary.h"

#include <QHash>
#include <QList>
#include <QPair>
#include <QWidget>

#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QModelIndex;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSortFilterProxyModel;
class QSpinBox;
class QStandardItem;
class QStandardItemModel;
class QTreeView;
class SdoClient;

/**
 * @brief EDS görüntüleme ve CANopen işlemleri paneli.
 *
 * EDS veya DCF nesne ağacını gösterir; seçili nesne için SDO okuma/yazma yapar,
 * NMT komutları ve SYNC gönderir. "Parametre seti" bölümü cihazdaki tüm
 * ayarları sırayla okuyup DCF dosyasına kaydeder (upload) veya kayıtlı bir
 * DCF'teki ParameterValue değerlerini cihaza yazar (download). EDS'teki PDO
 * eşlemeleri, Rx tablosundaki çerçeveleri yorumlamak için InterpretFrame() ile
 * dışarıya sunulur.
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
     * @brief EDS veya DCF dosyasını açar, ağacı ve PDO çözücüyü günceller.
     *
     * DCF'te [DeviceComissioning] NodeID varsa hedef düğüm numarası buna ayarlanır.
     * @param filePath .eds veya .dcf dosyasının yolu.
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

    /// Tema değiştiğinde ağaçtaki başlık renklerini günceller.
    void ApplyThemeColors();

signals:
    /// EDS veya düğüm numarası değiştiğinde, yani çerçeve yorumları değiştiğinde yayılır.
    void InterpretationChanged();

    /**
     * @brief NMT / SYNC kutusundan veya SDO kutusundan (Oku, Yaz, Kaydet) bir çerçeve gönderildiğinde yayılır.
     *
     * Ana pencere bu çerçeveyi Mesajlar panelindeki Tx (gönderim) tablosunda gösterir.
     * @param frame Gönderilen çerçeve.
     * @param description Tx tablosunda açıklama olarak gösterilecek metin (ör. "NMT Start → Node 2").
     */
    void ManualFrameSent(const CanFrame &frame, const QString &description);

private:
    /// Panel bileşenlerini oluşturur ve yerleştirir.
    void BuildUi();

    /**
     * @brief EDS ağacını nesne sözlüğünden yeniden oluşturur.
     *
     * Nesneler CiA 301 index alanlarına göre başlıklar altında gruplanır:
     * dosya/cihaz bilgisi, veri tipleri, temel (communication profile),
     * vendor specific (manufacturer) ve device specific (device profile).
     */
    void PopulateTree();

    /// Başlık satırlarını varsayılan açık/kapalı durumlarına getirir.
    void RestoreDefaultExpansion();

    /// Başlık satırlarının ilk sütununu tüm satır genişliğine yayar (filtreleme sonrası da çağrılır).
    void ApplyGroupSpans();

    /**
     * @brief Ağaç arama metnini uygular; eşleşen satırların üst başlıkları açılır.
     * @param text Index veya ad içinde aranacak metin.
     */
    void ApplySearch(const QString &text);

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
     * @brief Hedef düğüme "save" imzasını (73 61 76 65) SDO expedited download ile yazar.
     *
     * Kaydet (Vendor): 2105h:00, Kaydet (parametre): 1010h:01. Örneğin Node 2 için
     * COB-ID 602, veri 23 10 10 01 73 61 76 65 gönderilir; yanıt günlüğe yazılır.
     * @param index Hedef nesne index'i.
     * @param subIndex Hedef subindex.
     * @param name Günlükte gösterilecek işlem adı.
     */
    void SendSaveCommand(quint16 index, quint8 subIndex, const QString &name);

    /**
     * @brief Elle başlatılan bir SDO aktarımının çerçevelerini Tx tablosuna bildirmeye başlar.
     *
     * Aktarım bitene (veya hata verene) kadar SDO istemcisinin gönderdiği her çerçeve
     * ManualFrameSent ile yayılır; ilk çerçeve bu açıklamayı, sonrakiler "· devam" ekini alır.
     * Parametre seti (toplu aktarım) çerçeveleri bildirilmez.
     * @param description Tx tablosunda gösterilecek açıklama (ör. "SDO oku 0x2100:00 → Node 1").
     */
    void BeginSdoTxReport(const QString &description);

    /// SDO çerçevelerinin Tx tablosuna bildirilmesini sonlandırır.
    void EndSdoTxReport();

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

    /// Toplu SDO aktarımındaki (parametre seti) tek bir adım.
    struct BatchItem
    {
        quint16 index = 0;
        quint8 subIndex = 0;
        quint16 dataType = 0;
        bool write = false; ///< true: download (yaz), false: upload (oku)
        QByteArray data;    ///< Yazılacak little-endian veri
        QString note;       ///< Günlükte gösterilecek ek açıklama (ör. "PDO devre dışı")
    };

    /// Cihazdaki parametreleri sırayla okuyup DCF dosyasına kaydetmeyi başlatır.
    void UploadParameterSet();

    /// Kayıtlı bir DCF dosyasındaki değerleri cihaza yazmayı başlatır.
    void DownloadParameterSet();

    /**
     * @brief Yüklü EDS'ten cihazdan okunacak parametrelerin listesini çıkarır.
     * @param includeReadOnly false ise yalnızca hem okunabilir hem yazılabilir (rw) girdiler alınır.
     * @return DCF'e yazılabilir tipteki girdiler için okuma adımları.
     */
    QVector<BatchItem> BuildUploadPlan(bool includeReadOnly) const;

    /**
     * @brief DCF değerlerinden CiA 301 sırasına uygun yazma planı oluşturur.
     *
     * Sıra: PDO'lar devre dışı (COB-ID bit 31) → PDO eşlemeleri (sub0=0, girdiler,
     * sub0=n) → diğer parametreler → PDO COB-ID'leri son değerleriyle → isteğe bağlı 1010h:01 "save".
     * @param source ParameterValue içeren sözlük (DCF).
     * @param nodeId $NODEID ifadeleri için düğüm numarası.
     * @param plan Oluşturulan adımlar.
     * @param skipped Yazılamayan / kodlanamayan değer sayısı.
     */
    void BuildDownloadPlan(const ObjectDictionary &source, quint8 nodeId, QVector<BatchItem> &plan, int &skipped);

    /**
     * @brief Toplu aktarımı verilen planla başlatır.
     * @param plan Adımlar.
     * @param upload true ise okuma (sonunda DCF yazılır), false ise yazma.
     */
    void StartBatch(const QVector<BatchItem> &plan, bool upload);

    /// Toplu aktarımın sıradaki (m_batchPosition) adımını başlatır; adım kalmadıysa FinishBatch çağırır.
    void StartNextBatchItem();

    /**
     * @brief Bir adım bittiğinde sonucu işler ve sıradakine geçer.
     * @param success Adım başarılı mı.
     * @param reason Başarısızsa nedeni.
     */
    void AdvanceBatch(bool success, const QString &reason);

    /**
     * @brief Toplu aktarımı bitirir; okuma tamamlandıysa DCF dosyasını yazar.
     * @param aborted Kullanıcı iptali, bağlantı kopması veya yanıt yokluğu nedeniyle yarıda kaldıysa true.
     * @param reason Yarıda kalma nedeni.
     */
    void FinishBatch(bool aborted, const QString &reason);

    /// Süren toplu aktarımı iptal eder.
    void CancelBatch();

    /// Toplu aktarım sürüyorsa true döndürür.
    bool IsBatchActive() const;

    /// Erişim tipi cihazdan okunabilir bir nesneyi gösteriyorsa true döndürür.
    static bool IsReadableAccess(const QString &accessType);

    /// Erişim tipi cihaza yazılabilir bir nesneyi gösteriyorsa true döndürür.
    static bool IsWritableAccess(const QString &accessType);

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
    QSortFilterProxyModel *m_treeProxy = nullptr;
    QTreeView *m_treeView = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QList<QPair<QStandardItem *, bool>> m_groupItems; ///< Başlık öğesi ve varsayılan açık olma durumu
    QLabel *m_fileLabel = nullptr;
    QSpinBox *m_nodeIdSpin = nullptr;
    QLineEdit *m_indexEdit = nullptr;
    QLineEdit *m_subIndexEdit = nullptr;
    QComboBox *m_dataTypeCombo = nullptr;
    QLineEdit *m_valueEdit = nullptr;
    QPushButton *m_readButton = nullptr;
    QPushButton *m_writeButton = nullptr;
    QPushButton *m_saveVendorButton = nullptr;
    QPushButton *m_saveParameterButton = nullptr;
    QSpinBox *m_nmtTargetSpin = nullptr;
    QLabel *m_nmtTargetHint = nullptr;
    QString m_sdoTxDescription; ///< Boş değilse elle başlatılan SDO çerçeveleri Tx tablosuna bildirilir
    int m_sdoTxFrameCount = 0;  ///< Bu aktarımda bildirilen çerçeve sayısı
    QList<QPushButton *> m_busButtons;
    QPlainTextEdit *m_log = nullptr;
    QHash<quint32, QStandardItem *> m_valueItems;

    // Parametre seti (toplu SDO)
    QPushButton *m_uploadSetButton = nullptr;
    QPushButton *m_downloadSetButton = nullptr;
    QPushButton *m_cancelBatchButton = nullptr;
    QCheckBox *m_includeReadOnlyCheck = nullptr;
    QCheckBox *m_storeCheck = nullptr;
    QProgressBar *m_batchProgress = nullptr;
    QLabel *m_batchStatusLabel = nullptr;
    QVector<BatchItem> m_batch;
    int m_batchPosition = -1;
    bool m_batchUpload = false;
    bool m_batchCancelled = false;
    int m_batchFailures = 0;
    int m_batchConsecutiveTimeouts = 0;
    QHash<quint32, QString> m_batchValues;
    QString m_batchTargetPath;
};

#endif // CANOPENPANEL_H
