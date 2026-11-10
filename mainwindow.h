#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "src/can/CanFrame.h"

#include <QElapsedTimer>
#include <QMainWindow>
#include <QTimer>

#include <memory>

class CanOpenPanel;
class FilterPanel;
class FrameFilterProxyModel;
class ICanInterface;
class QAction;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTableView;
class QTabWidget;
class RxMessageModel;
class TraceModel;
class TxMessageModel;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

/**
 * @brief Canbus Analyzer ana penceresi.
 *
 * Bağlantı araç çubuğu, Mesajlar (Rx/Tx), Trace ve CANopen/EDS sekmeleri,
 * filtre paneli ve durum çubuğunu bir araya getirir. CAN arayüzü (PCAN veya
 * sanal) bağlantı anında oluşturulur; tüm gönderimler SendFrame() üzerinden
 * yapılır ve Trace'e kaydedilir.
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

private:
    /// Menüler, araç çubuğu, sekmeler, paneller ve durum çubuğunu oluşturur.
    void BuildUi();

    /// Bağlantı araç çubuğunu (marka, kanal, bit hızı, bağlan) oluşturur.
    void BuildToolBar();

    /// Menü çubuğunu oluşturur.
    void BuildMenus();

    /**
     * @brief Rx ve Tx tablolarını içeren "Mesajlar" sekmesini oluşturur.
     * @return Sekme widget'ı.
     */
    QWidget *BuildMessagesPage();

    /**
     * @brief Kronolojik çerçeve kaydını gösteren "Trace" sekmesini oluşturur.
     * @return Sekme widget'ı.
     */
    QWidget *BuildTracePage();

    /// Durum çubuğu etiketlerini oluşturur.
    void BuildStatusBar();

    /**
     * @brief Tablo görünümlerine ortak görsel ayarları uygular.
     * @param view Ayarlanacak tablo.
     */
    void ConfigureTableView(QTableView *view) const;

    /**
     * @brief Açık/koyu temayı uygular ve tercihi kaydeder.
     * @param dark true ise koyu tema.
     */
    void SetDarkTheme(bool dark);

    /// Tema renklerine bağlı model ve panel renklerini günceller.
    void ApplyThemeColors();

    /// Bağlıysa bağlantıyı keser, değilse seçili kanala bağlanır.
    void ToggleConnection();

    /// Seçili kanal ve bit hızıyla CAN arayüzünü açar.
    void ConnectToBus();

    /// CAN arayüzünü kapatır ve periyodik gönderimleri durdurur.
    void DisconnectFromBus();

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

    /// Yeni gönderim mesajı ekleme diyaloğunu açar.
    void AddTxMessage();

    /// Seçili gönderim mesajını düzenleme diyaloğunu açar.
    void EditTxMessage();

    /// Seçili gönderim mesajlarını siler.
    void RemoveTxMessages();

    /// Seçili gönderim mesajlarını birer kez gönderir.
    void SendSelectedTxMessages();

    /// Zamanı gelen periyodik mesajları gönderir (1 ms zamanlayıcıdan çağrılır).
    void ProcessPeriodicTx();

    /// Rx/Tx sayaçlarını, filtre ve trace bilgisini durum çubuğunda günceller.
    void UpdateStatistics();

    /// Rx tablosunu ve sayaçları sıfırlar.
    void ClearReceiveTable();

    /// Trace kaydını siler ve zamanı sıfırlar.
    void ClearTrace();

    /// Trace'in (filtre uygulanmışsa görünen) satırlarını CSV dosyasına kaydeder.
    void ExportTrace();

    /// Trace için kayıt başlangıcına göre şimdiki zamanı (µs) döndürür.
    qint64 TraceTimeUs() const;

    /// Seçili Tx satır numaralarını (küçükten büyüğe) döndürür.
    QList<int> SelectedTxRows() const;

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

    QTabWidget *m_tabs = nullptr;
    QTableView *m_rxView = nullptr;
    QTableView *m_txView = nullptr;
    QTableView *m_traceView = nullptr;
    QComboBox *m_channelCombo = nullptr;
    QComboBox *m_bitrateCombo = nullptr;
    QAction *m_connectAction = nullptr;
    QAction *m_darkThemeAction = nullptr;
    QWidget *m_connectButton = nullptr;
    QCheckBox *m_traceAutoScrollCheck = nullptr;
    QCheckBox *m_traceFilterCheck = nullptr;
    QPushButton *m_tracePauseButton = nullptr;
    QLabel *m_traceCountLabel = nullptr;

    QLabel *m_connectionLabel = nullptr;
    QLabel *m_busStatusLabel = nullptr;
    QLabel *m_rxCountLabel = nullptr;
    QLabel *m_txCountLabel = nullptr;
    QLabel *m_filterLabel = nullptr;

    QTimer m_txTimer;
    QTimer m_statisticsTimer;
    QElapsedTimer m_clock;
    qint64 m_traceStartUs = 0;
    quint64 m_rxTotal = 0;
    quint64 m_txTotal = 0;
    quint64 m_rxAtLastStatistics = 0;
};

#endif // MAINWINDOW_H
