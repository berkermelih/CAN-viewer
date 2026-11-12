#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "src/can/CanFrame.h"
#include "src/can/PcanInterface.h"

#include <QElapsedTimer>
#include <QList>
#include <QMainWindow>
#include <QTimer>

#include <memory>

class CanOpenPanel;
class FilterPanel;
class FrameFilterProxyModel;
class ICanInterface;
class QAction;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QDockWidget;
class QLabel;
class QMenu;
class QPushButton;
class QTableView;
class RxMessageModel;
class TraceModel;
class ThemeSwitch;
class TxMessageDialog;
class TxMessageModel;
class UpdateChecker;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

/**
 * @brief Canbus Analyzer ana penceresi.
 *
 * Bağlantı araç çubuğu, Mesajlar (Rx/Tx), Trace ve CANopen/EDS panelleri,
 * filtre paneli ve durum çubuğunu bir araya getirir. Paneller birer
 * QDockWidget'tır: sekme olarak üst üste durabilir, sekmesinden sürüklenerek
 * ayrı pencereye alınabilir veya ekran bölünerek yan yana / alt alta
 * yerleştirilebilir; düzen oturumlar arasında saklanır. PCAN portları
 * otomatik algılanır. Tüm gönderimler SendFrame() üzerinden yapılır ve
 * Trace'e kaydedilir. Gönderim listesi .cantx dosyasına kaydedilip açılabilir.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Ana pencereyi ve tüm alt bileşenleri oluşturur.
     * @param parent Üst widget.
     */
    explicit MainWindow(QWidget *parent = nullptr);

    /// Açık bağlantıyı kapatır ve UI nesnesini siler.
    ~MainWindow() override;

protected:
    /**
     * @brief Kapatmadan önce kaydedilmemiş gönderim listesini sorar, pencere düzenini saklar.
     * @param event Kapatma olayı; kullanıcı iptal ederse yok sayılır.
     */
    void closeEvent(QCloseEvent *event) override;

    /**
     * @brief Panel sekmelerine çift tıklanınca paneli ayrı pencereye alır.
     * @param watched Olayın hedefi (panel sekme çubuğu).
     * @param event Olay.
     * @return Olay işlendiyse true.
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Menüler, araç çubuğu, paneller ve durum çubuğunu oluşturur.
    void BuildUi();

    /// Tablo eylemlerini (kes, kopyala, yapıştır, sil, biçim ...) ve kısayollarını oluşturur.
    void BuildActions();

    /// Bağlantı araç çubuğunu (marka, port, mod, bit hızları, bağlan) oluşturur.
    void BuildToolBar();

    /// Menü çubuğunu oluşturur.
    void BuildMenus();

    /// Mesajlar, Trace, CANopen/EDS ve Filtre panellerini dock olarak oluşturur.
    void BuildDocks();

    /**
     * @brief Sürüklenebilir, ayrılabilir bir panel (dock) oluşturur.
     * @param title Sekme / başlık metni.
     * @param objectName Düzen kaydı için benzersiz ad.
     * @param content Panel içeriği.
     */
    QDockWidget *CreateDock(const QString &title, const QString &objectName, QWidget *content);

    /**
     * @brief Rx ve Tx tablolarını içeren "Mesajlar" panelini oluşturur.
     * @return Panel widget'ı.
     */
    QWidget *BuildMessagesPage();

    /**
     * @brief Kronolojik çerçeve kaydını gösteren "Trace" panelini oluşturur.
     * @return Panel widget'ı.
     */
    QWidget *BuildTracePage();

    /// Durum çubuğu etiketlerini oluşturur.
    void BuildStatusBar();

    /**
     * @brief Tablo görünümlerine ortak görsel ayarları uygular.
     * @param view Ayarlanacak tablo.
     */
    void ConfigureTableView(QTableView *view) const;

    /// Mesajlar, Trace, CANopen ve Filtre panellerini tek sekme grubunda toplar (varsayılan düzen).
    void ApplyTabbedLayout();

    /// Ekranı böler: solda Mesajlar, ortada üstte Trace / altta CANopen/EDS, sağda Filtre.
    void ApplySplitLayout();

    /**
     * @brief Ana penceredeki sekme grubunda olan panellerin başlığını daraltır, diğerlerinde gösterir.
     *
     * Başlık widget'ı (DockTitleBar) değiştirilmez; yalnızca yüksekliği ayarlanır. Bu nedenle
     * sürükleme sırasında da güvenle çağrılabilir.
     */
    void UpdateDockTitleBars();

    /// Panel sekme çubuklarına çift tıklama süzgecini kurar (yeni oluşan sekme çubukları dahil).
    void InstallTabBarFilters();

    /**
     * @brief Sekme metnine karşılık gelen paneli bulur.
     * @param title Sekme / panel başlığı.
     * @return Panel; bulunamazsa nullptr.
     */
    QDockWidget *DockForTitle(const QString &title) const;

    /**
     * @brief Paneli imlecin bulunduğu yerde ayrı pencere olarak açar.
     *
     * Pencere başlığından sürüklenip ana pencereye bırakılarak veya başlığına çift
     * tıklanarak yeniden birleştirilebilir.
     * @param dock Ayrılacak panel.
     */
    void FloatDock(QDockWidget *dock);

    /// Kayıtlı pencere boyutu ve panel düzenini geri yükler; yoksa varsayılan düzeni uygular.
    void RestoreWindowLayout();

    /**
     * @brief Açık/koyu temayı uygular ve tercihi kaydeder.
     * @param dark true ise koyu tema.
     */
    void SetDarkTheme(bool dark);

    /// Tema renklerine bağlı model, panel ve ikon renklerini günceller.
    void ApplyThemeColors();

    /// Eylem ve düğme ikonlarını güncel tema renkleriyle yeniden atar.
    void RefreshActionIcons();

    /// Bağlı PCAN kanallarını algılar ve port listesini günceller (bağlı değilken periyodik).
    void RefreshPorts();

    /// Seçili porta göre CAN FD seçeneğini etkinleştirir / devre dışı bırakır.
    void UpdateModeAvailability();

    /// Seçili moda (CAN CC / CAN FD) göre bit hızı listelerini doldurur.
    void UpdateBitrateCombos();

    /// Bağlıysa bağlantıyı keser, değilse seçili porta bağlanır.
    void ToggleConnection();

    /// Seçili port, mod ve bit hızlarıyla CAN arayüzünü açar.
    void ConnectToBus();

    /// CAN arayüzünü kapatır ve periyodik gönderimleri durdurur.
    void DisconnectFromBus();

    /**
     * @brief Bağlantı beklenmedik şekilde koptuğunda (ör. adaptör söküldü) durumu günceller ve uyarı gösterir.
     *
     * Uygulama kapanmaz; periyodik gönderim durur, port listesi yeniden taranır.
     * @param reason Kopma nedeni.
     */
    void HandleConnectionLost(const QString &reason);

    /// Bağlantı durumuna göre araç çubuğu, panel ve etiketleri günceller.
    void UpdateConnectionState();

    /**
     * @brief Arayüzden gelen çerçeveleri Rx tablosuna, Trace'e ve CANopen paneline iletir.
     * @param frames Alınan çerçeveler.
     */
    void HandleFramesReceived(const QVector<CanFrame> &frames);

    /**
     * @brief Çerçeveyi açık CAN arayüzü üzerinden gönderir ve Trace'e kaydeder.
     * @param frame Gönderilecek çerçeve.
     * @param errorText Hata durumunda açıklama.
     * @return Gönderildiyse true.
     */
    bool SendFrame(const CanFrame &frame, QString &errorText);

    /**
     * @brief CANopen panelinden (NMT / SYNC) gönderilen çerçeveyi Tx tablosuna işler.
     *
     * Aynı çerçeve listede varsa sayacı artırılır; yoksa açıklamasıyla birlikte
     * elle gönderim (periyot 0) satırı olarak eklenir.
     * @param frame Gönderilen çerçeve.
     * @param description Açıklama sütununa yazılacak metin.
     */
    void RecordManualTx(const CanFrame &frame, const QString &description);

    /**
     * @brief Tx diyaloğunu seçili adaptörün yeteneklerine göre ayarlar (CAN FD desteği).
     * @param dialog Açılacak diyalog.
     */
    void ConfigureTxDialog(TxMessageDialog &dialog) const;

    /// Yeni gönderim mesajı ekleme diyaloğunu açar.
    void AddTxMessage();

    /// Seçili gönderim mesajını düzenleme diyaloğunu açar.
    void EditTxMessage();

    /// Seçili gönderim mesajlarını siler.
    void RemoveTxMessages();

    /// Seçili gönderim mesajlarını birer kez gönderir.
    void SendSelectedTxMessages();

    /// Seçili gönderim mesajlarını panoya kopyalar (metin + uygulama içi biçim).
    void CopyTxMessages();

    /// Seçili gönderim mesajlarını panoya kopyalar ve listeden siler.
    void CutTxMessages();

    /// Panodaki gönderim mesajlarını seçili satırın altına ekler.
    void PasteTxMessages();

    /// Onay aldıktan sonra tüm gönderim listesini siler.
    void ClearTxMessages();

    /// Zamanı gelen periyodik mesajları gönderir (1 ms zamanlayıcıdan çağrılır).
    void ProcessPeriodicTx();

    /// Seçili Rx satırlarını sekmeyle ayrılmış metin olarak panoya kopyalar.
    void CopyRxRows();

    /// Seçili Rx satırlarını tablodan siler (ID yeniden gelirse tekrar görünür).
    void RemoveRxRows();

    /// Seçili Trace satırlarını sekmeyle ayrılmış metin olarak panoya kopyalar.
    void CopyTraceRows();

    /**
     * @brief Tablonun seçili satırlarını başlık satırıyla birlikte sekmeyle ayrılmış metne çevirir.
     * @param view Kaynak tablo.
     * @return Panoya yazılacak metin; seçim yoksa boş.
     */
    QString SelectedRowsText(const QTableView *view) const;

    /**
     * @brief Rx tablosu sağ tık menüsünü gösterir.
     * @param position Tablo görünüm alanındaki tıklama noktası.
     */
    void ShowRxContextMenu(const QPoint &position);

    /**
     * @brief Tx tablosu sağ tık menüsünü gösterir (PCAN-View düzeni).
     * @param position Tablo görünüm alanındaki tıklama noktası.
     */
    void ShowTxContextMenu(const QPoint &position);

    /**
     * @brief Trace tablosu sağ tık menüsünü gösterir.
     * @param position Tablo görünüm alanındaki tıklama noktası.
     */
    void ShowTraceContextMenu(const QPoint &position);

    /**
     * @brief "CAN ID biçimi" ve "Veri biçimi" alt menülerini ekler.
     * @param menu Hedef menü.
     */
    void AddFormatMenus(QMenu *menu);

    /**
     * @brief Tüm tablolarda ID ve veri gösterim biçimini değiştirir ve kaydeder.
     * @param idFormat ID biçimi.
     * @param dataFormat Veri biçimi.
     */
    void SetDisplayFormat(IdFormat idFormat, DataFormat dataFormat);

    /// Gönderim listesini dosyadan açar (kaydedilmemiş değişiklik varsa önce sorar).
    void OpenTxList();

    /**
     * @brief Verilen gönderim listesi dosyasını yükler.
     * @param filePath .cantx dosyası.
     * @return Yüklendiyse true; hata kullanıcıya gösterilir.
     */
    bool LoadTxListFile(const QString &filePath);

    /**
     * @brief Gönderim listesini mevcut dosyasına kaydeder; dosya yoksa "Farklı kaydet" açılır.
     * @return Kaydedildiyse true.
     */
    bool SaveTxList();

    /**
     * @brief Gönderim listesini yeni bir dosyaya kaydeder.
     * @return Kaydedildiyse true.
     */
    bool SaveTxListAs();

    /**
     * @brief Gönderim listesini verilen yola yazar ve son kullanılanlara ekler.
     * @param filePath Hedef dosya.
     * @return Yazıldıysa true; hata kullanıcıya gösterilir.
     */
    bool WriteTxListFile(const QString &filePath);

    /**
     * @brief Kaydedilmemiş gönderim listesi varsa kaydet / vazgeç / iptal sorar.
     * @return Devam edilebilirse true, kullanıcı iptal ettiyse false.
     */
    bool ConfirmDiscardTxChanges();

    /**
     * @brief Dosyayı "Son kullanılan listeler" başına ekler.
     * @param filePath Dosya yolu.
     */
    void AddRecentTxList(const QString &filePath);

    /// "Son kullanılan listeler" alt menüsünü yeniden oluşturur.
    void RebuildRecentMenu();

    /// Pencere başlığını açık gönderim listesinin adıyla günceller.
    void UpdateWindowTitle();

    /// Rx/Tx sayaçlarını, filtre ve trace bilgisini durum çubuğunda günceller.
    void UpdateStatistics();

    /// Rx tablosunu ve sayaçları sıfırlar.
    void ClearReceiveTable();

    /// Trace kaydını siler ve zamanı sıfırlar.
    void ClearTrace();

    /// Trace'in (filtre uygulanmışsa görünen) satırlarını CSV dosyasına kaydeder.
    void ExportTrace();

    /// CANopen/EDS panelini öne getirir ve EDS dosya seçme diyaloğunu açar.
    void BrowseEdsFile();

    /// Trace için kayıt başlangıcına göre şimdiki zamanı (µs) döndürür.
    qint64 TraceTimeUs() const;

    /// Seçili Tx satır numaralarını (küçükten büyüğe) döndürür.
    QList<int> SelectedTxRows() const;

    /// GitHub'da daha yeni bir sürüm olup olmadığını denetler ve sonucu gösterir.
    void CheckForUpdates();

    /// Hakkında penceresini gösterir.
    void ShowAboutDialog();

    Ui::MainWindow *ui;

    std::unique_ptr<ICanInterface> m_can;
    RxMessageModel *m_rxModel = nullptr;
    FrameFilterProxyModel *m_rxProxy = nullptr;
    TxMessageModel *m_txModel = nullptr;
    TraceModel *m_traceModel = nullptr;
    FrameFilterProxyModel *m_traceProxy = nullptr;
    FilterPanel *m_filterPanel = nullptr;
    CanOpenPanel *m_canOpenPanel = nullptr;

    QDockWidget *m_messagesDock = nullptr;
    QDockWidget *m_traceDock = nullptr;
    QDockWidget *m_canOpenDock = nullptr;
    QDockWidget *m_filterDock = nullptr;

    QTableView *m_rxView = nullptr;
    QTableView *m_txView = nullptr;
    QTableView *m_traceView = nullptr;
    QComboBox *m_portCombo = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_bitrateCombo = nullptr;
    QComboBox *m_dataBitrateCombo = nullptr;
    QLabel *m_dataBitrateLabel = nullptr;
    QAction *m_dataBitrateLabelAction = nullptr;
    QAction *m_dataBitrateComboAction = nullptr;
    QAction *m_connectAction = nullptr;
    QAction *m_darkThemeAction = nullptr;
    QWidget *m_connectButton = nullptr;
    QCheckBox *m_traceAutoScrollCheck = nullptr;
    QCheckBox *m_traceFilterCheck = nullptr;
    QPushButton *m_tracePauseButton = nullptr;
    QLabel *m_traceCountLabel = nullptr;
    QPushButton *m_txOpenButton = nullptr;
    QPushButton *m_txSaveButton = nullptr;
    ThemeSwitch *m_themeSwitch = nullptr;
    UpdateChecker *m_updateChecker = nullptr;
    QTimer m_dockCheckTimer;

    // Tablo eylemleri
    QAction *m_txNewAction = nullptr;
    QAction *m_txEditAction = nullptr;
    QAction *m_txSendAction = nullptr;
    QAction *m_txCutAction = nullptr;
    QAction *m_txCopyAction = nullptr;
    QAction *m_txPasteAction = nullptr;
    QAction *m_txDeleteAction = nullptr;
    QAction *m_txClearAction = nullptr;
    QAction *m_rxCopyAction = nullptr;
    QAction *m_rxDeleteAction = nullptr;
    QAction *m_clearRxAction = nullptr;
    QAction *m_traceCopyAction = nullptr;
    QAction *m_openTxListAction = nullptr;
    QAction *m_saveTxListAction = nullptr;
    QAction *m_saveTxListAsAction = nullptr;
    QAction *m_exportTraceAction = nullptr;
    QAction *m_openEdsAction = nullptr;
    QMenu *m_recentMenu = nullptr;

    QLabel *m_connectionLabel = nullptr;
    QLabel *m_busStatusLabel = nullptr;
    QLabel *m_rxCountLabel = nullptr;
    QLabel *m_txCountLabel = nullptr;
    QLabel *m_filterLabel = nullptr;

    QList<PcanChannelInfo> m_ports;
    QString m_portError;
    QTimer m_portScanTimer;

    QString m_txListPath;
    bool m_txListModified = false;
    IdFormat m_idFormat = IdFormat::Hex;
    DataFormat m_dataFormat = DataFormat::Hex;

    QTimer m_txTimer;
    QTimer m_statisticsTimer;
    QElapsedTimer m_clock;
    qint64 m_traceStartUs = 0;
    quint64 m_rxTotal = 0;
    quint64 m_txTotal = 0;
    quint64 m_rxAtLastStatistics = 0;
};

#endif // MAINWINDOW_H
