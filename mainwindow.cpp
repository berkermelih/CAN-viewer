#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "src/AppInfo.h"
#include "src/can/ICanInterface.h"
#include "src/models/FrameFilterProxyModel.h"
#include "src/models/RxMessageModel.h"
#include "src/models/TraceModel.h"
#include "src/models/TxListFile.h"
#include "src/models/TxMessageModel.h"
#include "src/ui/CanOpenPanel.h"
#include "src/ui/DockTitleBar.h"
#include "src/ui/FilterPanel.h"
#include "src/ui/Localization.h"
#include "src/ui/Theme.h"
#include "src/ui/ThemeSwitch.h"
#include "src/ui/TxMessageDialog.h"
#include "src/update/UpdateChecker.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTabBar>
#include <QTableView>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

// Pencere düzeni kaydının sürümü; panel yapısı değişirse artırılır.
// 3: ayrı pencereler artık birbirine birleşmez; birleşmiş pencere içerebilen eski kayıtlar (Qt 6.11'de
// geri yüklenince bozuk grup kurulup sürüklemede çöküyordu) yok sayılır.
constexpr int WindowStateVersion = 3;
// "Son kullanılan listeler" menüsündeki en fazla dosya sayısı.
constexpr int MaxRecentFiles = 6;

// Bit hızını "500 kbit/s" / "2 Mbit/s" biçiminde metne çevirir.
QString BitrateText(int bitrate)
{
    if (bitrate >= 1000000 && bitrate % 1000000 == 0) {
        return QStringLiteral("%1 Mbit/s").arg(bitrate / 1000000);
    }
    return QStringLiteral("%1 kbit/s").arg(bitrate / 1000);
}

// Biçim değerlerini QSettings metnine çevirir / geri okur.
QString IdFormatKey(IdFormat format)
{
    return format == IdFormat::Decimal ? QStringLiteral("dec") : QStringLiteral("hex");
}

QString DataFormatKey(DataFormat format)
{
    switch (format) {
    case DataFormat::Decimal: return QStringLiteral("dec");
    case DataFormat::Ascii: return QStringLiteral("ascii");
    default: return QStringLiteral("hex");
    }
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_clock.start();

    m_rxModel = new RxMessageModel(this);
    m_txModel = new TxMessageModel(this);
    m_traceModel = new TraceModel(this);

    const QSettings settings;
    m_idFormat = settings.value(QStringLiteral("view/idFormat")).toString() == QLatin1String("dec") ? IdFormat::Decimal
                                                                                                    : IdFormat::Hex;
    const QString dataKey = settings.value(QStringLiteral("view/dataFormat")).toString();
    m_dataFormat = dataKey == QLatin1String("dec")     ? DataFormat::Decimal
                   : dataKey == QLatin1String("ascii") ? DataFormat::Ascii
                                                       : DataFormat::Hex;

    BuildUi();

    const auto interpreter = [this](const CanFrame &frame) { return m_canOpenPanel->InterpretFrame(frame); };
    m_rxModel->SetInterpreter(interpreter);
    m_traceModel->SetInterpreter(interpreter);
    m_canOpenPanel->SetSendFunction([this](const CanFrame &frame, QString &errorText) {
        return SendFrame(frame, errorText);
    });
    connect(m_canOpenPanel, &CanOpenPanel::ManualFrameSent, this, &MainWindow::RecordManualTx);
    connect(m_canOpenPanel, &CanOpenPanel::InterpretationChanged, m_rxModel, &RxMessageModel::RefreshInterpretations);
    connect(m_canOpenPanel, &CanOpenPanel::InterpretationChanged, m_traceView->viewport(),
            [this]() { m_traceView->viewport()->update(); });
    connect(m_filterPanel, &FilterPanel::FilterChanged, this, [this]() {
        m_rxProxy->RefreshFilter();
        m_traceProxy->RefreshFilter();
        UpdateStatistics();
    });
    connect(m_txModel, &TxMessageModel::ContentChanged, this, [this]() {
        m_txListModified = true;
        UpdateWindowTitle();
    });

    m_txTimer.setInterval(1);
    m_txTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_txTimer, &QTimer::timeout, this, &MainWindow::ProcessPeriodicTx);

    m_statisticsTimer.setInterval(1000);
    connect(&m_statisticsTimer, &QTimer::timeout, this, &MainWindow::UpdateStatistics);
    m_statisticsTimer.start();

    // PCAN portları bağlı değilken periyodik olarak yeniden algılanır (tak-çıkar).
    m_portScanTimer.setInterval(2000);
    connect(&m_portScanTimer, &QTimer::timeout, this, &MainWindow::RefreshPorts);
    m_portScanTimer.start();

    SetDisplayFormat(m_idFormat, m_dataFormat);
    ApplyThemeColors();
    RefreshPorts();
    UpdateConnectionState();
    UpdateStatistics();
    UpdateWindowTitle();
    RestoreWindowLayout();
}

MainWindow::~MainWindow()
{
    if (m_can) {
        m_can->Close();
    }
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!ConfirmDiscardTxChanges()) {
        event->ignore();
        return;
    }
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/state"), saveState(WindowStateVersion));
    event->accept();

    // Dil değişikliği için istenen yeniden başlatma: kapanış onaylandıktan sonra yeni örnek açılır.
    if (m_restartRequested) {
        m_restartRequested = false;
        QStringList arguments = QCoreApplication::arguments();
        if (!arguments.isEmpty()) {
            arguments.removeFirst();
        }
        QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments);
    }
}

void MainWindow::ChangeLanguage(AppLanguage language)
{
    if (language == SavedLanguage()) {
        return;
    }
    SaveLanguage(language);
    const auto answer = QMessageBox::question(
        this, tr("Dil"),
        tr("Arayüz dili \"%1\" olarak ayarlandı. Değişiklik uygulama yeniden başlatıldığında geçerli olur.\n\n"
           "Şimdi yeniden başlatılsın mı?")
            .arg(LanguageNativeName(language)));
    if (answer != QMessageBox::Yes) {
        return;
    }
    m_restartRequested = true;
    if (!close()) {
        m_restartRequested = false; // kullanıcı kaydedilmemiş değişiklik sorusunda iptal etti
    }
}

void MainWindow::BuildUi()
{
    setWindowIcon(Theme::CreateAppIcon());
    resize(1400, 860);

    m_canOpenPanel = new CanOpenPanel(this);
    m_filterPanel = new FilterPanel(this);

    BuildActions();
    BuildDocks();

    // Rx: her ID için tek satır, sıralanabilir
    m_rxProxy = new FrameFilterProxyModel(
        &m_filterPanel->Filter(), [this](int row) { return m_rxModel->FramePointerAt(row); }, this);
    m_rxProxy->setSourceModel(m_rxModel);
    m_rxProxy->setSortRole(RxMessageModel::SortRole);
    m_rxView->setModel(m_rxProxy);
    m_rxView->sortByColumn(RxMessageModel::ColumnId, Qt::AscendingOrder);
    m_rxView->horizontalHeader()->setSectionResizeMode(RxMessageModel::ColumnInterpretation, QHeaderView::Stretch);
    m_rxView->setColumnWidth(RxMessageModel::ColumnId, 100);
    m_rxView->setColumnWidth(RxMessageModel::ColumnType, 116);
    m_rxView->setColumnWidth(RxMessageModel::ColumnDlc, 50);
    m_rxView->setColumnWidth(RxMessageModel::ColumnData, 330);
    m_rxView->setColumnWidth(RxMessageModel::ColumnCycle, 110);
    m_rxView->setColumnWidth(RxMessageModel::ColumnCount, 90);
    m_rxView->setColumnWidth(RxMessageModel::ColumnTime, 100);

    // Trace: kronolojik, sıralama yok; filtre isteğe bağlı
    m_traceProxy = new FrameFilterProxyModel(
        &m_filterPanel->Filter(), [this](int row) { return m_traceModel->FramePointerAt(row); }, this);
    m_traceProxy->setSourceModel(m_traceModel);
    m_traceProxy->SetFilterActive(false);
    m_traceView->setModel(m_traceProxy);
    m_traceView->horizontalHeader()->setSectionResizeMode(TraceModel::ColumnInterpretation, QHeaderView::Stretch);
    m_traceView->setColumnWidth(TraceModel::ColumnNumber, 80);
    m_traceView->setColumnWidth(TraceModel::ColumnTime, 110);
    m_traceView->setColumnWidth(TraceModel::ColumnDelta, 90);
    m_traceView->setColumnWidth(TraceModel::ColumnDirection, 50);
    m_traceView->setColumnWidth(TraceModel::ColumnId, 100);
    m_traceView->setColumnWidth(TraceModel::ColumnType, 116);
    m_traceView->setColumnWidth(TraceModel::ColumnDlc, 50);
    m_traceView->setColumnWidth(TraceModel::ColumnData, 330);
    connect(m_traceProxy, &QAbstractItemModel::rowsInserted, this, [this]() {
        if (m_traceAutoScrollCheck->isChecked()) {
            m_traceView->scrollToBottom();
        }
    });

    BuildToolBar();
    BuildMenus();
    BuildStatusBar();
}

void MainWindow::BuildActions()
{
    // Tablolara özgü eylemler yalnızca ilgili tablo odaktayken kısayol olarak çalışır.
    const auto makeAction = [this](const QString &text, const QList<QKeySequence> &shortcuts,
                                   void (MainWindow::*slot)()) {
        auto *action = new QAction(text, this);
        action->setShortcuts(shortcuts);
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        action->setShortcutVisibleInContextMenu(true);
        connect(action, &QAction::triggered, this, slot);
        return action;
    };

    m_txNewAction = makeAction(tr("Yeni mesaj..."), {QKeySequence(Qt::Key_Insert)}, &MainWindow::AddTxMessage);
    m_txEditAction = makeAction(tr("Mesajı düzenle..."), {QKeySequence(Qt::Key_Return), QKeySequence(Qt::Key_Enter)},
                                &MainWindow::EditTxMessage);
    m_txSendAction = makeAction(tr("Şimdi gönder"), {QKeySequence(Qt::Key_Space)}, &MainWindow::SendSelectedTxMessages);
    m_txCutAction = makeAction(tr("Kes"), {QKeySequence::Cut}, &MainWindow::CutTxMessages);
    m_txCopyAction = makeAction(tr("Kopyala"), {QKeySequence::Copy}, &MainWindow::CopyTxMessages);
    m_txPasteAction = makeAction(tr("Yapıştır"), {QKeySequence::Paste}, &MainWindow::PasteTxMessages);
    m_txDeleteAction = makeAction(tr("Sil"), {QKeySequence::Delete}, &MainWindow::RemoveTxMessages);
    m_txClearAction = makeAction(tr("Tx tablosunu temizle"), {QKeySequence(Qt::SHIFT | Qt::Key_Escape)},
                                 &MainWindow::ClearTxMessages);
    // Rx'teki Esc gibi pencere genelinde çalışır (Bus menüsünde de bulunur); silmeden önce onay sorulur.
    // Eylem Tx tablosuna da ekli olduğundan Mesajlar paneli ayrı penceredeyken de çalışır.
    m_txClearAction->setShortcutContext(Qt::WindowShortcut);
    m_txClearAction->setToolTip(tr("Gönderim listesindeki tüm mesajları sil (Shift+Esc)"));
    addAction(m_txClearAction);

    m_rxCopyAction = makeAction(tr("Kopyala"), {QKeySequence::Copy}, &MainWindow::CopyRxRows);
    m_rxDeleteAction = makeAction(tr("Satırı sil"), {QKeySequence::Delete}, &MainWindow::RemoveRxRows);
    m_traceCopyAction = makeAction(tr("Kopyala"), {QKeySequence::Copy}, &MainWindow::CopyTraceRows);

    // Pencere genelinde geçerli eylemler
    m_clearRxAction = new QAction(tr("Rx tablosunu temizle"), this);
    m_clearRxAction->setShortcut(QKeySequence(Qt::Key_Escape));
    m_clearRxAction->setShortcutVisibleInContextMenu(true);
    m_clearRxAction->setToolTip(tr("Alınan mesajlar tablosunu temizle (Esc)"));
    connect(m_clearRxAction, &QAction::triggered, this, &MainWindow::ClearReceiveTable);
    addAction(m_clearRxAction);

    m_openTxListAction = new QAction(tr("Gönderim listesini aç..."), this);
    m_openTxListAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(m_openTxListAction, &QAction::triggered, this, &MainWindow::OpenTxList);
    m_saveTxListAction = new QAction(tr("Gönderim listesini kaydet"), this);
    m_saveTxListAction->setShortcut(QKeySequence::Save);
    connect(m_saveTxListAction, &QAction::triggered, this, &MainWindow::SaveTxList);
    m_saveTxListAsAction = new QAction(tr("Gönderim listesini farklı kaydet..."), this);
    m_saveTxListAsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(m_saveTxListAsAction, &QAction::triggered, this, &MainWindow::SaveTxListAs);
    m_exportTraceAction = new QAction(tr("Trace'i CSV olarak kaydet..."), this);
    m_exportTraceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(m_exportTraceAction, &QAction::triggered, this, &MainWindow::ExportTrace);
    m_openEdsAction = new QAction(tr("EDS dosyası aç..."), this);
    m_openEdsAction->setShortcut(QKeySequence::Open);
    connect(m_openEdsAction, &QAction::triggered, this, &MainWindow::BrowseEdsFile);
}

QDockWidget *MainWindow::CreateDock(const QString &title, const QString &objectName, QWidget *content)
{
    auto *dock = new QDockWidget(title, this);
    dock->setObjectName(objectName);
    dock->setWidget(content);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                      | QDockWidget::DockWidgetClosable);
    // Başlık çubuğu bir kez atanır ve bir daha değiştirilmez (bkz. DockTitleBar); sekmedeyken yalnızca daraltılır.
    auto *titleBar = new DockTitleBar(dock);
    dock->setTitleBarWidget(titleBar);
    connect(titleBar, &DockTitleBar::DockRequested, this, [this, dock]() {
        // Panel sinyal içinde yeniden yerleştirileceğinden işlem olay döngüsüne bırakılır.
        QTimer::singleShot(0, this, [this, dock]() { DockIntoMainWindow(dock); });
    });
    const auto schedule = [this]() { QTimer::singleShot(0, this, &MainWindow::UpdateDockTitleBars); };
    connect(dock, &QDockWidget::topLevelChanged, this, schedule);
    connect(dock, &QDockWidget::dockLocationChanged, this, schedule);
    connect(dock, &QDockWidget::visibilityChanged, this, schedule);
    return dock;
}

void MainWindow::BuildDocks()
{
    // Merkez widget gizlenir; tüm alanı paneller paylaşır ve serbestçe bölünebilir.
    centralWidget()->hide();
    // GroupedDragging bilerek kapalı: açıkken Qt ayrı pencereleri birbirine birleştirir
    // (QDockWidgetGroupWindow). Birleştirme yalnızca ana pencerede (bölme / sekme) yapılır. Bu seçenek
    // kapalıyken Qt sekmeden sürükleyip ayırmayı sunmadığından o davranış eventFilter'da yapılır.
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);
    connect(this, &QMainWindow::tabifiedDockWidgetActivated, this,
            [this]() { QTimer::singleShot(0, this, &MainWindow::UpdateDockTitleBars); });
    // Sürükle-bırak sırasında her durum değişimi sinyal üretmediği için başlıklar ayrıca periyodik denetlenir.
    m_dockCheckTimer.setInterval(400);
    connect(&m_dockCheckTimer, &QTimer::timeout, this, &MainWindow::UpdateDockTitleBars);
    m_dockCheckTimer.start();

    m_messagesDock = CreateDock(tr("Mesajlar"), QStringLiteral("MessagesDock"), BuildMessagesPage());
    m_traceDock = CreateDock(tr("Trace"), QStringLiteral("TraceDock"), BuildTracePage());
    // CANopen paneli kaydırılabilir alanda tutulur; böylece bölünmüş düzende küçültülebilir.
    auto *canOpenScroll = new QScrollArea(this);
    canOpenScroll->setWidget(m_canOpenPanel);
    canOpenScroll->setWidgetResizable(true);
    canOpenScroll->setFrameShape(QFrame::NoFrame);
    m_canOpenDock = CreateDock(tr("CANopen / EDS"), QStringLiteral("CanOpenDock"), canOpenScroll);

    m_filterDock = CreateDock(tr("Filtre"), QStringLiteral("FilterDock"), m_filterPanel);
    m_filterDock->setMinimumWidth(300);
    addDockWidget(Qt::RightDockWidgetArea, m_filterDock);
    addDockWidget(Qt::LeftDockWidgetArea, m_messagesDock);
    addDockWidget(Qt::LeftDockWidgetArea, m_traceDock);
    addDockWidget(Qt::LeftDockWidgetArea, m_canOpenDock);

    ApplyTabbedLayout();
}

void MainWindow::ApplyTabbedLayout()
{
    // Filtre dahil tüm paneller tek bir sekme grubunda toplanır.
    const QList<QDockWidget *> docks = {m_messagesDock, m_traceDock, m_canOpenDock, m_filterDock};
    for (QDockWidget *dock : docks) {
        dock->setFloating(false);
        removeDockWidget(dock);
    }
    addDockWidget(Qt::LeftDockWidgetArea, m_messagesDock);
    tabifyDockWidget(m_messagesDock, m_traceDock);
    tabifyDockWidget(m_messagesDock, m_canOpenDock);
    tabifyDockWidget(m_messagesDock, m_filterDock);
    for (QDockWidget *dock : docks) {
        dock->show();
    }
    m_messagesDock->raise();
    UpdateDockTitleBars();
}

void MainWindow::ApplySplitLayout()
{
    const QList<QDockWidget *> docks = {m_messagesDock, m_traceDock, m_canOpenDock, m_filterDock};
    for (QDockWidget *dock : docks) {
        dock->setFloating(false);
        removeDockWidget(dock);
    }
    addDockWidget(Qt::LeftDockWidgetArea, m_messagesDock);
    splitDockWidget(m_messagesDock, m_traceDock, Qt::Horizontal);
    splitDockWidget(m_traceDock, m_canOpenDock, Qt::Vertical);
    addDockWidget(Qt::RightDockWidgetArea, m_filterDock);
    for (QDockWidget *dock : docks) {
        dock->show();
    }
    UpdateDockTitleBars();
    // Boyutlar, yerleşim hesaplandıktan sonra uygulanır (aksi halde panel en küçük boyutları baskın gelir).
    QTimer::singleShot(0, this, [this]() {
        const int available = width() - (m_filterDock->isHidden() ? 0 : 320);
        resizeDocks({m_messagesDock, m_traceDock}, {available / 2, available / 2}, Qt::Horizontal);
        const int column = m_traceDock->height() + m_canOpenDock->height();
        resizeDocks({m_traceDock, m_canOpenDock}, {column * 2 / 5, column * 3 / 5}, Qt::Vertical);
    });
}

void MainWindow::UpdateDockTitleBars()
{
    // Panel sürüklenirken başlık boyutu değiştirilmez (sürükleme durumu ve Qt'nin bırakma yerleşimi
    // bozulmasın); fare bırakılınca zamanlayıcı başlıkları günceller.
    if (QGuiApplication::mouseButtons() & Qt::LeftButton) {
        return;
    }
    InstallTabBarFilters();

    // Başlık widget'ı hiçbir zaman değiştirilmez (değiştirmek Qt'nin ayrı pencereyi çerçevesiz kurmasına ve
    // sürükleme durumunun bozulmasına yol açıyordu); yalnızca sekme grubundayken daraltılır.
    const QList<QDockWidget *> docks = {m_messagesDock, m_traceDock, m_canOpenDock, m_filterDock};
    for (QDockWidget *dock : docks) {
        auto *titleBar = dock != nullptr ? qobject_cast<DockTitleBar *>(dock->titleBarWidget()) : nullptr;
        if (titleBar == nullptr) {
            continue;
        }
        // Ana penceredeki sekme grubunda sekme zaten başlıktır; ayrı pencerede başlık her zaman
        // görünür kalır ki tutulup taşınabilsin.
        const bool insideMainWindow = !dock->isFloating() && dock->window() == this;
        titleBar->SetCollapsed(insideMainWindow && !tabifiedDockWidgets(dock).isEmpty());
    }
}

void MainWindow::InstallTabBarFilters()
{
    // Panel sekme çubuklarını QMainWindow gerektikçe oluşturur; yeni olanlara süzgeç kurulur.
    const QList<QTabBar *> tabBars = findChildren<QTabBar *>(Qt::FindDirectChildrenOnly);
    for (QTabBar *tabBar : tabBars) {
        if (!tabBar->property("dockTabFilter").toBool()) {
            tabBar->setProperty("dockTabFilter", true);
            tabBar->installEventFilter(this);
        }
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (auto *tabBar = qobject_cast<QTabBar *>(watched); tabBar != nullptr && tabBar->parentWidget() == this) {
        if (HandleDockTabBarEvent(tabBar, event)) {
            return true;
        }
    } else if (auto *dock = qobject_cast<QDockWidget *>(watched);
               dock != nullptr && event->type() == QEvent::MouseButtonRelease && dock->property("tabDragGrab").toBool()) {
        // Sekmeden başlatılan sürükleme bitti: Qt bırakmayı işledikten sonra yakalama serbest bırakılır.
        dock->setProperty("tabDragGrab", false);
        dock->removeEventFilter(this);
        QTimer::singleShot(0, dock, [dock]() {
            if (QWidget::mouseGrabber() == dock) {
                dock->releaseMouse();
            }
        });
    }
    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::HandleDockTabBarEvent(QTabBar *tabBar, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        const int tab = mouse->button() == Qt::LeftButton ? tabBar->tabAt(mouse->position().toPoint()) : -1;
        m_tabDragBar = tab >= 0 ? tabBar : nullptr;
        m_tabDragTitle = tab >= 0 ? tabBar->tabText(tab) : QString();
        return false; // sekme seçimi Qt'de kalır
    }
    case QEvent::MouseMove: {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        if (m_tabDragBar != tabBar || !(mouse->buttons() & Qt::LeftButton)) {
            return false;
        }
        // Sekme çubuğundan belirgin biçimde uzaklaşınca panel ayrı pencereye çıkar ve sürükleme sürer;
        // çubuk boyunca yatay hareket sekmede kalır.
        const int margin = QApplication::startDragDistance() * 2;
        if (tabBar->rect().adjusted(-margin, -margin, margin, margin).contains(mouse->position().toPoint())) {
            return false;
        }
        QDockWidget *dock = DockForTitle(m_tabDragTitle);
        m_tabDragBar = nullptr;
        if (dock == nullptr) {
            return false;
        }
        StartDockDragFromTab(tabBar, dock, mouse->globalPosition().toPoint());
        return true;
    }
    case QEvent::MouseButtonRelease:
        m_tabDragBar = nullptr;
        return false;
    case QEvent::MouseButtonDblClick: {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            const int tab = tabBar->tabAt(mouse->position().toPoint());
            if (QDockWidget *dock = DockForTitle(tabBar->tabText(tab)); tab >= 0 && dock != nullptr) {
                m_tabDragBar = nullptr;
                FloatDock(dock);
                return true;
            }
        }
        return false;
    }
    default:
        return false;
    }
}

void MainWindow::StartDockDragFromTab(QTabBar *tabBar, QDockWidget *dock, const QPoint &cursor)
{
    // Sekme çubuğunun basılı durumu sıfırlanır (fare yakalaması panele geçecek). Panel ayrılınca Qt
    // sekme çubuğunu kaldırabileceği için bu, ayırmadan önce yapılır.
    QMouseEvent release(QEvent::MouseButtonRelease, tabBar->mapFromGlobal(QPointF(cursor)), QPointF(cursor),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(tabBar, &release);

    const QSize dockedSize = dock->size();
    dock->setFloating(true);
    auto *titleBar = qobject_cast<DockTitleBar *>(dock->titleBarWidget());
    if (titleBar != nullptr) {
        titleBar->SetCollapsed(false);
    }
    QSize size(qMax(520, dockedSize.width() * 2 / 3), qMax(360, dockedSize.height() * 2 / 3));
    if (const QScreen *screen = QGuiApplication::screenAt(cursor)) {
        size = size.boundedTo(screen->availableGeometry().size());
    }
    // İmleç yeni pencerenin başlık çubuğunda, sol tarafta durur.
    const int titleHeight = titleBar != nullptr ? titleBar->sizeHint().height() : 24;
    const QPoint grabOffset(qMin(80, size.width() / 3), titleHeight / 2);
    dock->resize(size);
    dock->move(cursor - grabOffset);
    dock->show();
    dock->raise();
    if (QLayout *layout = dock->layout()) {
        layout->activate();
    }

    // Sürükleme Qt'ye devredilir: fare panele yönlendirilir ve başlıkta basılmış gibi bir olay
    // gönderilir. Sonraki hareketlerle Qt paneli taşır, ana pencerede bölme/sekme yerini gösterir.
    dock->setProperty("tabDragGrab", true);
    dock->installEventFilter(this);
    dock->grabMouse();
    QMouseEvent press(QEvent::MouseButtonPress, dock->mapFromGlobal(QPointF(cursor)), QPointF(cursor),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(dock, &press);
}

QDockWidget *MainWindow::DockForTitle(const QString &title) const
{
    const QList<QDockWidget *> docks = {m_messagesDock, m_traceDock, m_canOpenDock, m_filterDock};
    for (QDockWidget *dock : docks) {
        if (dock != nullptr && dock->windowTitle() == title) {
            return dock;
        }
    }
    return nullptr;
}

void MainWindow::FloatDock(QDockWidget *dock)
{
    const QSize dockedSize = dock->size();
    dock->setFloating(true);
    // Ayrı pencerede başlık hemen görünsün (çift tıklama sırasında fare basılı olduğundan
    // UpdateDockTitleBars() bekler; diğer başlıklar fare bırakılınca güncellenir).
    if (auto *titleBar = qobject_cast<DockTitleBar *>(dock->titleBarWidget())) {
        titleBar->SetCollapsed(false);
    }

    QSize size(qMax(520, dockedSize.width() * 2 / 3), qMax(360, dockedSize.height() * 2 / 3));
    QPoint topLeft = QCursor::pos() - QPoint(size.width() / 2, 20);
    if (const QScreen *screen = QGuiApplication::screenAt(QCursor::pos())) {
        const QRect available = screen->availableGeometry();
        size = size.boundedTo(available.size());
        topLeft.setX(qBound(available.left(), topLeft.x(), available.right() - size.width()));
        topLeft.setY(qBound(available.top(), topLeft.y(), available.bottom() - size.height()));
    }
    dock->resize(size);
    dock->move(topLeft);
    dock->show();
    dock->raise();
    dock->activateWindow();
}

void MainWindow::DockIntoMainWindow(QDockWidget *dock)
{
    if (dock == nullptr || dock->window() == this) {
        return;
    }
    const bool wasHidden = dock->isHidden();
    dock->setFloating(false); // Qt eski yerine döndürür
    if (dock->window() != this) {
        // Eski yeri kalmamış panel ana penceredeki bir panelin sekme grubuna eklenir.
        QDockWidget *target = nullptr;
        const QList<QDockWidget *> docks = {m_messagesDock, m_traceDock, m_canOpenDock, m_filterDock};
        for (QDockWidget *other : docks) {
            if (other != nullptr && other != dock && other->window() == this && !other->isFloating()
                && !other->isHidden()) {
                target = other;
                break;
            }
        }
        if (target != nullptr) {
            tabifyDockWidget(target, dock);
        } else {
            addDockWidget(Qt::LeftDockWidgetArea, dock);
        }
        dock->setFloating(false);
    }
    if (!wasHidden) {
        dock->show();
        dock->raise();
    } else {
        dock->hide(); // kapatılmış panel kapalı kalır (Görünüm menüsünden açılır)
    }
    UpdateDockTitleBars();
}

void MainWindow::RestoreWindowLayout()
{
    const QSettings settings;
    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    const QByteArray state = settings.value(QStringLiteral("window/state")).toByteArray();
    if (state.isEmpty() || !restoreState(state, WindowStateVersion)) {
        ApplyTabbedLayout();
    }
    UpdateDockTitleBars();
}

void MainWindow::BuildToolBar()
{
    QToolBar *toolBar = addToolBar(tr("Bağlantı"));
    toolBar->setObjectName(QStringLiteral("ConnectionToolBar"));
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    toolBar->toggleViewAction()->setEnabled(false);

    auto *brandIcon = new QLabel(this);
    brandIcon->setPixmap(Theme::CreateAppIcon().pixmap(26, 26));
    auto *brandLabel = new QLabel(tr("Canbus Analyzer"), this);
    brandLabel->setObjectName(QStringLiteral("BrandLabel"));
    toolBar->addWidget(brandIcon);
    toolBar->addWidget(brandLabel);
    toolBar->addSeparator();

    m_portCombo = new QComboBox(this);
    m_portCombo->setPlaceholderText(tr("Port seç"));
    m_portCombo->setMinimumWidth(250);
    m_portCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(m_portCombo, &QComboBox::currentIndexChanged, this, &MainWindow::UpdateModeAvailability);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("CAN CC"), false);
    m_modeCombo->addItem(tr("CAN FD"), true);
    m_modeCombo->setToolTip(tr("CAN CC: klasik CAN · CAN FD: esnek veri hızı (FD destekli adaptör gerekir)"));

    m_bitrateCombo = new QComboBox(this);
    m_bitrateCombo->setMinimumWidth(118);
    m_bitrateCombo->setToolTip(tr("Nominal (arbitration) bit hızı"));
    m_dataBitrateCombo = new QComboBox(this);
    m_dataBitrateCombo->setMinimumWidth(110);
    m_dataBitrateCombo->setToolTip(tr("CAN FD veri fazı bit hızı (BRS)"));

    const QSettings settings;
    m_modeCombo->setCurrentIndex(settings.value(QStringLiteral("bus/fd")).toBool() ? 1 : 0);
    UpdateBitrateCombos();
    const int savedBitrate = settings.value(QStringLiteral("bus/bitrate"), 500000).toInt();
    const int savedDataBitrate = settings.value(QStringLiteral("bus/dataBitrate"), 2000000).toInt();
    if (m_bitrateCombo->findData(savedBitrate) >= 0) {
        m_bitrateCombo->setCurrentIndex(m_bitrateCombo->findData(savedBitrate));
    }
    if (m_dataBitrateCombo->findData(savedDataBitrate) >= 0) {
        m_dataBitrateCombo->setCurrentIndex(m_dataBitrateCombo->findData(savedDataBitrate));
    }
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, &MainWindow::UpdateBitrateCombos);

    toolBar->addWidget(new QLabel(tr("Port"), this));
    toolBar->addWidget(m_portCombo);
    toolBar->addWidget(new QLabel(tr("Mod"), this));
    toolBar->addWidget(m_modeCombo);
    toolBar->addWidget(new QLabel(tr("Bit hızı"), this));
    toolBar->addWidget(m_bitrateCombo);
    m_dataBitrateLabel = new QLabel(tr("Veri hızı"), this);
    m_dataBitrateLabelAction = toolBar->addWidget(m_dataBitrateLabel);
    m_dataBitrateComboAction = toolBar->addWidget(m_dataBitrateCombo);
    toolBar->addSeparator();

    m_connectAction = toolBar->addAction(tr("Bağlan"));
    m_connectAction->setShortcut(QKeySequence(Qt::Key_F5));
    m_connectAction->setToolTip(tr("Bağlan / bağlantıyı kes (F5)"));
    connect(m_connectAction, &QAction::triggered, this, &MainWindow::ToggleConnection);
    m_connectButton = toolBar->widgetForAction(m_connectAction);
    if (m_connectButton != nullptr) {
        m_connectButton->setObjectName(QStringLiteral("PrimaryAction"));
        m_connectButton->setCursor(Qt::PointingHandCursor);
    }
    UpdateBitrateCombos();

    // Sağ üst köşe: açık / koyu tema anahtarı
    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);
    m_themeSwitch = new ThemeSwitch(this);
    m_themeSwitch->setToolTip(tr("Açık / koyu tema (Ctrl+Shift+D)"));
    m_themeSwitch->setChecked(Theme::CurrentMode() == Theme::Mode::Dark);
    connect(m_themeSwitch, &ThemeSwitch::toggled, this, [this](bool dark) {
        if (m_darkThemeAction != nullptr) {
            m_darkThemeAction->setChecked(dark); // eylem SetDarkTheme'i çağırır
        }
    });
    toolBar->addWidget(m_themeSwitch);
}

void MainWindow::BuildMenus()
{
    QMenu *fileMenu = ui->menubar->addMenu(tr("&Dosya"));
    fileMenu->addAction(m_openTxListAction);
    m_recentMenu = fileMenu->addMenu(tr("Son kullanılan listeler"));
    fileMenu->addAction(m_saveTxListAction);
    fileMenu->addAction(m_saveTxListAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_openEdsAction);
    fileMenu->addAction(m_exportTraceAction);
    fileMenu->addSeparator();
    QAction *quit = fileMenu->addAction(tr("Çı&kış"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QMainWindow::close);
    RebuildRecentMenu();

    QMenu *busMenu = ui->menubar->addMenu(tr("&Bus"));
    busMenu->addAction(m_connectAction);
    QAction *rescan = busMenu->addAction(tr("Portları yeniden tara"));
    connect(rescan, &QAction::triggered, this, &MainWindow::RefreshPorts);
    busMenu->addSeparator();
    busMenu->addAction(m_clearRxAction);
    busMenu->addAction(m_txClearAction);
    QAction *clearTrace = busMenu->addAction(tr("Trace'i temizle"));
    connect(clearTrace, &QAction::triggered, this, &MainWindow::ClearTrace);
    QAction *resetTx = busMenu->addAction(tr("Tx sayaçlarını sıfırla"));
    connect(resetTx, &QAction::triggered, m_txModel, &TxMessageModel::ResetCounters);

    QMenu *transmitMenu = ui->menubar->addMenu(tr("&Gönder"));
    transmitMenu->addAction(m_txNewAction);
    transmitMenu->addAction(m_txEditAction);
    transmitMenu->addAction(m_txSendAction);
    transmitMenu->addSeparator();
    transmitMenu->addAction(m_txDeleteAction);
    transmitMenu->addAction(m_txClearAction);

    QMenu *viewMenu = ui->menubar->addMenu(tr("&Görünüm"));
    viewMenu->addAction(m_messagesDock->toggleViewAction());
    viewMenu->addAction(m_traceDock->toggleViewAction());
    viewMenu->addAction(m_canOpenDock->toggleViewAction());
    viewMenu->addAction(m_filterDock->toggleViewAction());
    viewMenu->addSeparator();
    QMenu *layoutMenu = viewMenu->addMenu(tr("Pencere düzeni"));
    QAction *tabbed = layoutMenu->addAction(tr("Sekmeli (tek pencere)"));
    connect(tabbed, &QAction::triggered, this, &MainWindow::ApplyTabbedLayout);
    QAction *split = layoutMenu->addAction(tr("Üç bölmeli (Mesajlar | Trace / CANopen)"));
    connect(split, &QAction::triggered, this, &MainWindow::ApplySplitLayout);
    layoutMenu->addSeparator();
    QAction *hint = layoutMenu->addAction(tr("İpucu: sekmeyi sürükleyip bırakarak paneli ayırın veya bölün"));
    hint->setEnabled(false);
    viewMenu->addSeparator();
    AddFormatMenus(viewMenu);
    viewMenu->addSeparator();
    m_darkThemeAction = viewMenu->addAction(tr("Koyu tema"));
    m_darkThemeAction->setCheckable(true);
    m_darkThemeAction->setChecked(Theme::CurrentMode() == Theme::Mode::Dark);
    m_darkThemeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    connect(m_darkThemeAction, &QAction::toggled, this, &MainWindow::SetDarkTheme);

    // Dil adları her dilde kendi adıyla gösterilir (Türkçe / English).
    viewMenu->addSeparator();
    QMenu *languageMenu = viewMenu->addMenu(tr("Dil"));
    auto *languageGroup = new QActionGroup(languageMenu);
    const AppLanguage savedLanguage = SavedLanguage();
    for (const AppLanguage language : {AppLanguage::Turkish, AppLanguage::English}) {
        QAction *action = languageMenu->addAction(LanguageNativeName(language));
        action->setCheckable(true);
        action->setChecked(language == savedLanguage);
        languageGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, language]() { ChangeLanguage(language); });
    }

    QMenu *helpMenu = ui->menubar->addMenu(tr("&Yardım"));
    QAction *checkUpdates = helpMenu->addAction(tr("Güncellemeleri denetle..."));
    connect(checkUpdates, &QAction::triggered, this, &MainWindow::CheckForUpdates);
    helpMenu->addSeparator();
    QAction *about = helpMenu->addAction(tr("&Hakkında"));
    connect(about, &QAction::triggered, this, &MainWindow::ShowAboutDialog);
}

void MainWindow::AddFormatMenus(QMenu *menu)
{
    QMenu *idMenu = menu->addMenu(tr("CAN ID biçimi"));
    auto *idGroup = new QActionGroup(idMenu);
    const QList<QPair<QString, IdFormat>> idFormats = {{tr("Onaltılık (hex)"), IdFormat::Hex},
                                                       {tr("Onluk (dec)"), IdFormat::Decimal}};
    for (const auto &entry : idFormats) {
        QAction *action = idMenu->addAction(entry.first);
        action->setCheckable(true);
        action->setChecked(m_idFormat == entry.second);
        idGroup->addAction(action);
        const IdFormat format = entry.second;
        connect(action, &QAction::triggered, this, [this, format]() { SetDisplayFormat(format, m_dataFormat); });
    }

    QMenu *dataMenu = menu->addMenu(tr("Veri baytları biçimi"));
    auto *dataGroup = new QActionGroup(dataMenu);
    const QList<QPair<QString, DataFormat>> dataFormats = {{tr("Onaltılık (hex)"), DataFormat::Hex},
                                                           {tr("Onluk (dec)"), DataFormat::Decimal},
                                                           {tr("ASCII"), DataFormat::Ascii}};
    for (const auto &entry : dataFormats) {
        QAction *action = dataMenu->addAction(entry.first);
        action->setCheckable(true);
        action->setChecked(m_dataFormat == entry.second);
        dataGroup->addAction(action);
        const DataFormat format = entry.second;
        connect(action, &QAction::triggered, this, [this, format]() { SetDisplayFormat(m_idFormat, format); });
    }

    // Menü her açıldığında güncel seçimi göster (biçim başka bir menüden değişmiş olabilir).
    connect(idMenu, &QMenu::aboutToShow, idMenu, [this, idMenu]() {
        const QList<QAction *> actions = idMenu->actions();
        for (int i = 0; i < actions.size(); ++i) {
            actions.at(i)->setChecked(static_cast<int>(m_idFormat) == i);
        }
    });
    connect(dataMenu, &QMenu::aboutToShow, dataMenu, [this, dataMenu]() {
        const QList<QAction *> actions = dataMenu->actions();
        for (int i = 0; i < actions.size(); ++i) {
            actions.at(i)->setChecked(static_cast<int>(m_dataFormat) == i);
        }
    });
}

void MainWindow::SetDisplayFormat(IdFormat idFormat, DataFormat dataFormat)
{
    m_idFormat = idFormat;
    m_dataFormat = dataFormat;
    m_rxModel->SetDisplayFormat(idFormat, dataFormat);
    m_txModel->SetDisplayFormat(idFormat, dataFormat);
    m_traceModel->SetDisplayFormat(idFormat, dataFormat);
    QSettings settings;
    settings.setValue(QStringLiteral("view/idFormat"), IdFormatKey(idFormat));
    settings.setValue(QStringLiteral("view/dataFormat"), DataFormatKey(dataFormat));
}

void MainWindow::ConfigureTableView(QTableView *view) const
{
    view->setFont(Theme::MonospaceFont());
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setAlternatingRowColors(true);
    view->setShowGrid(false);
    view->setWordWrap(false);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->verticalHeader()->setVisible(false);
    view->verticalHeader()->setDefaultSectionSize(view->fontMetrics().height() + 10);
    view->horizontalHeader()->setHighlightSections(false);
    view->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    view->horizontalHeader()->setFont(QApplication::font());
}

QWidget *MainWindow::BuildMessagesPage()
{
    // --- Rx ---
    m_rxView = new QTableView(this);
    ConfigureTableView(m_rxView);
    m_rxView->setSortingEnabled(true);
    m_rxView->addActions({m_rxCopyAction, m_rxDeleteAction});
    connect(m_rxView, &QTableView::customContextMenuRequested, this, &MainWindow::ShowRxContextMenu);

    auto *rxTitle = new QLabel(tr("Alınan mesajlar"), this);
    rxTitle->setObjectName(QStringLiteral("SectionTitle"));
    auto *rxHint = new QLabel(tr("Her ID tek satırda · verisi değişen satırlar vurgulanır · sağ tık: seçenekler"), this);
    rxHint->setObjectName(QStringLiteral("MutedText"));

    auto *rxHeader = new QHBoxLayout;
    rxHeader->addWidget(rxTitle);
    rxHeader->addSpacing(10);
    rxHeader->addWidget(rxHint);
    rxHeader->addStretch(1);

    auto *rxBox = new QWidget(this);
    auto *rxLayout = new QVBoxLayout(rxBox);
    rxLayout->setContentsMargins(0, 0, 0, 0);
    rxLayout->setSpacing(8);
    rxLayout->addLayout(rxHeader);
    rxLayout->addWidget(m_rxView);

    // --- Tx ---
    m_txView = new QTableView(this);
    ConfigureTableView(m_txView);
    m_txView->setModel(m_txModel);
    m_txView->horizontalHeader()->setSectionResizeMode(TxMessageModel::ColumnComment, QHeaderView::Stretch);
    m_txView->setColumnWidth(TxMessageModel::ColumnId, 110);
    m_txView->setColumnWidth(TxMessageModel::ColumnType, 116);
    m_txView->setColumnWidth(TxMessageModel::ColumnDlc, 50);
    m_txView->setColumnWidth(TxMessageModel::ColumnData, 330);
    m_txView->setColumnWidth(TxMessageModel::ColumnPeriod, 110);
    m_txView->addActions({m_txNewAction, m_txEditAction, m_txSendAction, m_txCutAction, m_txCopyAction,
                          m_txPasteAction, m_txDeleteAction, m_txClearAction});
    connect(m_txView, &QTableView::customContextMenuRequested, this, &MainWindow::ShowTxContextMenu);
    connect(m_txView, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        if (index.column() != TxMessageModel::ColumnId) {
            EditTxMessage();
        }
    });

    auto *txTitle = new QLabel(tr("Gönderilecek mesajlar"), this);
    txTitle->setObjectName(QStringLiteral("SectionTitle"));
    m_txOpenButton = new QPushButton(this);
    m_txOpenButton->setToolTip(tr("Gönderim listesini aç (Ctrl+Shift+O)"));
    m_txSaveButton = new QPushButton(this);
    m_txSaveButton->setToolTip(tr("Gönderim listesini kaydet (Ctrl+S)"));
    auto *addButton = new QPushButton(tr("Yeni..."), this);
    auto *editButton = new QPushButton(tr("Düzenle..."), this);
    auto *removeButton = new QPushButton(tr("Sil"), this);

    auto *sendButton = new QPushButton(tr("Gönder"), this);
    sendButton->setProperty("primary", true);
    sendButton->setToolTip(tr("Seçili mesajları bir kez gönder (tabloda Boşluk)"));
    connect(m_txOpenButton, &QPushButton::clicked, m_openTxListAction, &QAction::trigger);
    connect(m_txSaveButton, &QPushButton::clicked, m_saveTxListAction, &QAction::trigger);
    connect(addButton, &QPushButton::clicked, m_txNewAction, &QAction::trigger);
    connect(editButton, &QPushButton::clicked, m_txEditAction, &QAction::trigger);
    connect(removeButton, &QPushButton::clicked, m_txDeleteAction, &QAction::trigger);

    connect(sendButton, &QPushButton::clicked, m_txSendAction, &QAction::trigger);

    auto *txHeader = new QHBoxLayout;
    txHeader->addWidget(txTitle);
    txHeader->addSpacing(8);
    txHeader->addWidget(m_txOpenButton);
    txHeader->addWidget(m_txSaveButton);
    txHeader->addStretch(1);
    txHeader->addWidget(addButton);
    txHeader->addWidget(editButton);
    txHeader->addWidget(removeButton);

    txHeader->addSpacing(6);
    txHeader->addWidget(sendButton);

    auto *txBox = new QWidget(this);
    auto *txLayout = new QVBoxLayout(txBox);
    txLayout->setContentsMargins(0, 0, 0, 0);
    txLayout->setSpacing(8);
    txLayout->addLayout(txHeader);
    txLayout->addWidget(m_txView);

    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setHandleWidth(12);
    splitter->addWidget(rxBox);
    splitter->addWidget(txBox);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->addWidget(splitter);
    return page;
}

QWidget *MainWindow::BuildTracePage()
{
    m_traceView = new QTableView(this);
    ConfigureTableView(m_traceView);
    m_traceView->addAction(m_traceCopyAction);
    connect(m_traceView, &QTableView::customContextMenuRequested, this, &MainWindow::ShowTraceContextMenu);

    auto *title = new QLabel(tr("Trace"), this);
    title->setObjectName(QStringLiteral("SectionTitle"));
    m_traceCountLabel = new QLabel(this);
    m_traceCountLabel->setObjectName(QStringLiteral("CounterBadge"));
    auto *hint = new QLabel(tr("Tüm Rx/Tx çerçeveleri geliş sırasıyla · en fazla 100 000 kayıt"), this);
    hint->setObjectName(QStringLiteral("MutedText"));

    m_traceFilterCheck = new QCheckBox(tr("Filtreyi uygula"), this);
    connect(m_traceFilterCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_traceProxy->SetFilterActive(checked);
        UpdateStatistics();
    });
    m_traceAutoScrollCheck = new QCheckBox(tr("Otomatik kaydır"), this);
    m_traceAutoScrollCheck->setChecked(true);

    m_tracePauseButton = new QPushButton(tr("Duraklat"), this);
    m_tracePauseButton->setCheckable(true);
    connect(m_tracePauseButton, &QPushButton::toggled, this, [this](bool paused) {
        m_traceModel->SetPaused(paused);
        m_tracePauseButton->setText(paused ? tr("Sürdür") : tr("Duraklat"));
        UpdateStatistics();
    });
    auto *clearButton = new QPushButton(tr("Temizle"), this);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::ClearTrace);
    auto *exportButton = new QPushButton(tr("CSV kaydet..."), this);
    connect(exportButton, &QPushButton::clicked, m_exportTraceAction, &QAction::trigger);

    auto *header = new QHBoxLayout;
    header->addWidget(title);
    header->addSpacing(6);
    header->addWidget(m_traceCountLabel);
    header->addSpacing(10);
    header->addWidget(hint);
    header->addStretch(1);
    header->addWidget(m_traceFilterCheck);
    header->addSpacing(8);
    header->addWidget(m_traceAutoScrollCheck);
    header->addSpacing(12);
    header->addWidget(m_tracePauseButton);
    header->addWidget(clearButton);
    header->addWidget(exportButton);

    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);
    layout->addLayout(header);
    layout->addWidget(m_traceView, 1);
    return page;
}

void MainWindow::BuildStatusBar()
{
    m_connectionLabel = new QLabel(this);
    m_connectionLabel->setTextFormat(Qt::RichText);
    m_busStatusLabel = new QLabel(this);
    m_rxCountLabel = new QLabel(this);
    m_txCountLabel = new QLabel(this);
    m_filterLabel = new QLabel(this);

    statusBar()->setSizeGripEnabled(false);
    statusBar()->addWidget(m_connectionLabel);
    statusBar()->addWidget(m_busStatusLabel);
    statusBar()->addPermanentWidget(m_filterLabel);
    statusBar()->addPermanentWidget(m_rxCountLabel);
    statusBar()->addPermanentWidget(m_txCountLabel);
}

void MainWindow::SetDarkTheme(bool dark)
{
    if (m_themeSwitch != nullptr && m_themeSwitch->isChecked() != dark) {
        m_themeSwitch->setChecked(dark); // anahtarın sinyali eylemi tekrar değiştirmez (aynı durum)
    }
    Theme::Apply(*qApp, dark ? Theme::Mode::Dark : Theme::Mode::Light);
    QSettings settings;
    settings.setValue(QStringLiteral("appearance/theme"), dark ? QStringLiteral("dark") : QStringLiteral("light"));
    ApplyThemeColors();
    UpdateConnectionState();
}

void MainWindow::ApplyThemeColors()
{
    const Theme::Palette &colors = Theme::Colors();
    m_rxModel->SetHighlightColor(colors.accent);
    m_traceModel->SetDirectionColors(colors.success, colors.accent);
    m_canOpenPanel->ApplyThemeColors();
    RefreshActionIcons();
    for (QDockWidget *dock : {m_messagesDock, m_traceDock, m_canOpenDock, m_filterDock}) {
        if (auto *titleBar = dock != nullptr ? qobject_cast<DockTitleBar *>(dock->titleBarWidget()) : nullptr) {
            titleBar->RefreshIcons();
        }
    }
}

void MainWindow::RefreshActionIcons()
{
    const Theme::Palette &colors = Theme::Colors();
    m_txNewAction->setIcon(Theme::Icon(QStringLiteral("new"), colors.accent));
    m_txEditAction->setIcon(Theme::Icon(QStringLiteral("edit")));
    m_txSendAction->setIcon(Theme::Icon(QStringLiteral("send"), colors.accent));
    m_txCutAction->setIcon(Theme::Icon(QStringLiteral("cut")));
    m_txCopyAction->setIcon(Theme::Icon(QStringLiteral("copy")));
    m_txPasteAction->setIcon(Theme::Icon(QStringLiteral("paste")));
    m_txDeleteAction->setIcon(Theme::Icon(QStringLiteral("delete"), colors.danger));
    m_txClearAction->setIcon(Theme::Icon(QStringLiteral("clear"), colors.danger));
    m_rxCopyAction->setIcon(Theme::Icon(QStringLiteral("copy")));
    m_rxDeleteAction->setIcon(Theme::Icon(QStringLiteral("delete"), colors.danger));
    m_clearRxAction->setIcon(Theme::Icon(QStringLiteral("clear"), colors.danger));
    m_traceCopyAction->setIcon(Theme::Icon(QStringLiteral("copy")));
    m_openTxListAction->setIcon(Theme::Icon(QStringLiteral("open")));
    m_saveTxListAction->setIcon(Theme::Icon(QStringLiteral("save")));
    m_exportTraceAction->setIcon(Theme::Icon(QStringLiteral("export")));
    m_openEdsAction->setIcon(Theme::Icon(QStringLiteral("open")));
    if (m_txOpenButton != nullptr) {
        m_txOpenButton->setIcon(Theme::Icon(QStringLiteral("open")));
        m_txSaveButton->setIcon(Theme::Icon(QStringLiteral("save")));
    }
}

void MainWindow::RefreshPorts()
{
    if (m_can && m_can->IsOpen()) {
        return; // Bağlıyken port listesi sabit kalır.
    }

    QString errorText;
    const QList<PcanChannelInfo> ports = PcanInterface::DetectChannels(errorText);
    const bool errorChanged = errorText != m_portError;
    m_portError = errorText;

    bool same = ports.size() == m_ports.size();
    for (int i = 0; same && i < ports.size(); ++i) {
        same = ports.at(i).handle == m_ports.at(i).handle && ports.at(i).occupied == m_ports.at(i).occupied
               && ports.at(i).deviceName == m_ports.at(i).deviceName;
    }
    if (same && !errorChanged && (m_portCombo->count() > 0 || ports.isEmpty())) {
        return;
    }
    m_ports = ports;

    const QString previous = m_portCombo->currentData().toString();
    {
        QSignalBlocker blocker(m_portCombo);
        m_portCombo->clear();
        for (const PcanChannelInfo &port : std::as_const(m_ports)) {
            m_portCombo->addItem(port.DisplayName(), QString::number(port.handle));
            m_portCombo->setItemData(m_portCombo->count() - 1,
                                     tr("Handle 0x%1 · cihaz no %2 · %3")
                                         .arg(port.handle, 0, 16)
                                         .arg(port.deviceId)
                                         .arg(port.fdCapable ? tr("CAN FD destekli") : tr("yalnızca CAN CC")),
                                     Qt::ToolTipRole);
        }
        // Önceki seçim hâlâ bağlıysa korunur; değilse ilk algılanan port otomatik seçilir.
        int index = m_portCombo->findData(previous);
        if (index < 0 && m_portCombo->count() > 0) {
            index = 0;
        }
        m_portCombo->setCurrentIndex(index);
    }

    if (m_ports.isEmpty()) {
        m_portCombo->setToolTip(m_portError.isEmpty() ? tr("Bağlı PCAN cihazı bulunamadı. Cihazı takın; liste "
                                                           "otomatik güncellenir.")
                                                      : m_portError);
    } else {
        m_portCombo->setToolTip(tr("%1 PCAN kanalı algılandı").arg(m_ports.size()));
    }
    UpdateModeAvailability();
}

void MainWindow::UpdateModeAvailability()
{
    const int index = m_portCombo->currentIndex();
    // Port seçilmemişken FD seçimi serbesttir; seçili port FD desteklemiyorsa kapatılır.
    const bool fdAllowed = index < 0 || index >= m_ports.size() || m_ports.at(index).fdCapable;
    if (auto *model = qobject_cast<QStandardItemModel *>(m_modeCombo->model())) {
        if (QStandardItem *item = model->item(1)) {
            item->setEnabled(fdAllowed);
            item->setToolTip(fdAllowed ? QString() : tr("Seçili adaptör CAN FD desteklemiyor"));
        }
    }
    if (!fdAllowed && m_modeCombo->currentIndex() == 1) {
        m_modeCombo->setCurrentIndex(0);
    }
}

void MainWindow::UpdateBitrateCombos()
{
    const bool fd = m_modeCombo->currentData().toBool();
    const int previous = m_bitrateCombo->currentData().toInt();
    const QList<int> bitrates = fd ? PcanInterface::SupportedFdBitrates() : PcanInterface::SupportedBitrates();
    {
        QSignalBlocker blocker(m_bitrateCombo);
        m_bitrateCombo->clear();
        for (const int bitrate : bitrates) {
            m_bitrateCombo->addItem(BitrateText(bitrate), bitrate);
        }
        int index = m_bitrateCombo->findData(previous > 0 ? previous : 500000);
        if (index < 0) {
            index = m_bitrateCombo->findData(500000);
        }
        m_bitrateCombo->setCurrentIndex(qMax(0, index));
    }

    if (m_dataBitrateCombo->count() == 0) {
        const QList<int> dataBitrates = PcanInterface::SupportedFdDataBitrates();
        for (const int bitrate : dataBitrates) {
            m_dataBitrateCombo->addItem(BitrateText(bitrate), bitrate);
        }
        m_dataBitrateCombo->setCurrentIndex(qMax(0, m_dataBitrateCombo->findData(2000000)));
    }
    if (m_dataBitrateLabelAction != nullptr) {
        m_dataBitrateLabelAction->setVisible(fd);
        m_dataBitrateComboAction->setVisible(fd);
    }
}

void MainWindow::ToggleConnection()
{
    if (m_can && m_can->IsOpen()) {
        DisconnectFromBus();
    } else {
        ConnectToBus();
    }
}

void MainWindow::ConnectToBus()
{
    if (m_portCombo->currentIndex() < 0) {
        RefreshPorts();
    }
    if (m_portCombo->currentIndex() < 0) {
        QString text = tr("Bağlanmak için bir port seçin.\n\nBağlı PCAN cihazı algılanamadı; cihazın takılı ve "
                          "PEAK sürücüsünün kurulu olduğundan emin olun.");
        if (!m_portError.isEmpty()) {
            text += QStringLiteral("\n\n") + m_portError;
        }
        QMessageBox::warning(this, tr("Port seçilmedi"), text);
        return;
    }

    CanBusSettings settings;
    settings.channel = m_portCombo->currentData().toString();
    settings.fd = m_modeCombo->currentData().toBool();
    settings.bitrate = m_bitrateCombo->currentData().toInt();
    settings.dataBitrate = m_dataBitrateCombo->currentData().toInt();

    auto device = std::make_unique<PcanInterface>();
    connect(device.get(), &ICanInterface::FramesReceived, this, &MainWindow::HandleFramesReceived);
    connect(device.get(), &ICanInterface::BusStatusChanged, m_busStatusLabel, &QLabel::setText);
    connect(device.get(), &ICanInterface::ErrorOccurred, this,
            [this](const QString &text) { statusBar()->showMessage(text, 5000); });
    // Kuyruklu bağlantı: uyarı penceresi arayüzün kendi zamanlayıcı yuvasının dışında açılır.
    connect(device.get(), &ICanInterface::ConnectionLost, this, &MainWindow::HandleConnectionLost,
            Qt::QueuedConnection);

    QString errorText;
    if (!device->Open(settings, errorText)) {
        QMessageBox::critical(this, tr("Bağlantı Hatası"), errorText);
        return;
    }

    QSettings stored;
    stored.setValue(QStringLiteral("bus/fd"), settings.fd);
    stored.setValue(QStringLiteral("bus/bitrate"), settings.bitrate);
    stored.setValue(QStringLiteral("bus/dataBitrate"), settings.dataBitrate);

    // Önceki arayüz (varsa) burada, kendi sinyallerinin dışında güvenle silinir.
    m_can = std::move(device);
    m_txModel->ResetSchedule(m_clock.elapsed());
    m_txTimer.start();
    UpdateConnectionState();
}

void MainWindow::DisconnectFromBus()
{
    m_txTimer.stop();
    if (m_can) {
        m_can->Close();
    }
    m_busStatusLabel->clear();
    UpdateConnectionState();
    RefreshPorts();
}

void MainWindow::HandleConnectionLost(const QString &reason)
{
    // Arayüz kanalı zaten kapattı; uygulama açık kalır, yalnızca durum güncellenip kullanıcı uyarılır.
    m_txTimer.stop();
    m_busStatusLabel->clear();
    UpdateConnectionState();
    RefreshPorts();
    statusBar()->showMessage(tr("Bağlantı koptu."), 8000);
    QMessageBox::warning(this, tr("Bağlantı koptu"),
                         tr("CAN bağlantısı kesildi.\n\n%1\n\nAdaptörü yeniden takıp \"Bağlan\" ile tekrar "
                            "bağlanabilirsiniz.")
                             .arg(reason));
}

void MainWindow::UpdateConnectionState()
{
    const bool connected = m_can && m_can->IsOpen();
    m_connectAction->setText(connected ? tr("Bağlantıyı Kes") : tr("Bağlan"));
    m_portCombo->setEnabled(!connected);
    m_modeCombo->setEnabled(!connected);
    m_bitrateCombo->setEnabled(!connected);
    m_dataBitrateCombo->setEnabled(!connected);
    m_canOpenPanel->SetConnected(connected);

    if (m_connectButton != nullptr) {
        m_connectButton->setProperty("connected", connected);
        Theme::Repolish(m_connectButton);
    }

    const Theme::Palette &colors = Theme::Colors();
    if (connected) {
        QString speed = m_bitrateCombo->currentText();
        if (m_can->IsFdMode()) {
            speed = tr("CAN FD %1 / %2").arg(speed, m_dataBitrateCombo->currentText());
        }
        m_connectionLabel->setText(Theme::StatusDotText(colors.success, tr("Bağlı: %1 @ %2").arg(m_can->Name(), speed)));
    } else {
        m_connectionLabel->setText(Theme::StatusDotText(colors.textMuted, tr("Bağlı değil")));
    }
}

void MainWindow::HandleFramesReceived(const QVector<CanFrame> &frames)
{
    m_rxTotal += static_cast<quint64>(frames.size());
    m_rxModel->AddFrames(frames);
    m_traceModel->AppendFrames(frames, TraceDirection::Rx, TraceTimeUs());
    m_canOpenPanel->HandleFrames(frames);
}

bool MainWindow::SendFrame(const CanFrame &frame, QString &errorText)
{
    if (!m_can || !m_can->IsOpen()) {
        errorText = tr("CAN arayüzü bağlı değil.");
        return false;
    }
    if (!m_can->WriteFrame(frame, errorText)) {
        statusBar()->showMessage(errorText, 5000);
        return false;
    }
    ++m_txTotal;
    m_traceModel->AppendFrames({frame}, TraceDirection::Tx, TraceTimeUs());
    return true;
}

void MainWindow::RecordManualTx(const CanFrame &frame, const QString &description)
{
    // Aynı çerçeve listede varsa sayacı artırılır; yoksa elle gönderim (periyot 0) satırı eklenir.
    int row = m_txModel->FindMessage(frame);
    if (row < 0) {
        TxMessage message;
        message.frame = frame;
        message.frame.timestampUs = 0;
        message.periodMs = 0;
        message.comment = description;
        message.nextDueMs = m_clock.elapsed();
        row = m_txModel->AddMessage(message);
    }
    m_txModel->MarkSent(row);
}

void MainWindow::ConfigureTxDialog(TxMessageDialog &dialog) const
{
    const int index = m_portCombo->currentIndex();
    if (index >= 0 && index < m_ports.size() && !m_ports.at(index).fdCapable) {
        dialog.SetFdSupported(false, tr("Seçili adaptör (%1) CAN FD desteklemiyor.").arg(m_ports.at(index).deviceName));
    } else {
        dialog.SetFdSupported(true, QString());
    }
}

void MainWindow::AddTxMessage()
{
    TxMessageDialog dialog(this);
    ConfigureTxDialog(dialog);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    TxMessage message = dialog.Message();
    message.nextDueMs = m_clock.elapsed();
    const int row = m_txModel->AddMessage(message);
    m_txView->selectRow(row);
}

void MainWindow::EditTxMessage()
{
    const QList<int> rows = SelectedTxRows();
    if (rows.isEmpty()) {
        return;
    }
    const int row = rows.first();
    TxMessageDialog dialog(this);
    dialog.SetMessage(m_txModel->MessageAt(row));
    ConfigureTxDialog(dialog);
    if (dialog.exec() == QDialog::Accepted) {
        m_txModel->UpdateMessage(row, dialog.Message());
    }
}

void MainWindow::RemoveTxMessages()
{
    QList<int> rows = SelectedTxRows();
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (const int row : std::as_const(rows)) {
        m_txModel->RemoveMessage(row);
    }
}

void MainWindow::SendSelectedTxMessages()
{
    const QList<int> rows = SelectedTxRows();
    for (const int row : rows) {
        QString errorText;
        if (!SendFrame(m_txModel->MessageAt(row).frame, errorText)) {
            statusBar()->showMessage(errorText, 5000);
            return;
        }
        m_txModel->MarkSent(row);
    }
}

void MainWindow::CopyTxMessages()
{
    const QList<int> rows = SelectedTxRows();
    if (rows.isEmpty()) {
        return;
    }
    QVector<TxMessage> messages;
    for (const int row : rows) {
        messages.append(m_txModel->MessageAt(row));
    }
    auto *mime = new QMimeData;
    mime->setText(SelectedRowsText(m_txView));
    mime->setData(TxListFile::MimeType(), TxListFile::Serialize(messages));
    QApplication::clipboard()->setMimeData(mime);
    statusBar()->showMessage(tr("%1 mesaj kopyalandı").arg(messages.size()), 3000);
}

void MainWindow::CutTxMessages()
{
    if (SelectedTxRows().isEmpty()) {
        return;
    }
    CopyTxMessages();
    RemoveTxMessages();
}

void MainWindow::PasteTxMessages()
{
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (mime == nullptr) {
        return;
    }
    QByteArray payload;
    if (mime->hasFormat(TxListFile::MimeType())) {
        payload = mime->data(TxListFile::MimeType());
    } else if (mime->hasText()) {
        payload = mime->text().toUtf8();
    }
    QString errorText;
    std::optional<QVector<TxMessage>> messages = TxListFile::Deserialize(payload, errorText);
    if (!messages || messages->isEmpty()) {
        statusBar()->showMessage(tr("Panoda yapıştırılacak gönderim mesajı yok."), 4000);
        return;
    }
    const qint64 now = m_clock.elapsed();
    for (TxMessage &message : *messages) {
        message.nextDueMs = now;
    }
    const QList<int> selected = SelectedTxRows();
    const int insertRow = selected.isEmpty() ? m_txModel->rowCount() : selected.last() + 1;
    const int first = m_txModel->InsertMessages(insertRow, *messages);
    m_txView->clearSelection();
    for (int row = first; row < first + messages->size(); ++row) {
        m_txView->selectionModel()->select(m_txModel->index(row, 0),
                                           QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
}

void MainWindow::ClearTxMessages()
{
    if (m_txModel->rowCount() == 0) {
        return;
    }
    const auto answer = QMessageBox::question(this, tr("Gönderim listesi"),
                                              tr("Gönderim listesindeki %1 mesajın tümü silinsin mi?")
                                                  .arg(m_txModel->rowCount()));
    if (answer == QMessageBox::Yes) {
        m_txModel->Clear();
    }
}

void MainWindow::ProcessPeriodicTx()
{
    const QVector<int> dueRows = m_txModel->TakeDueRows(m_clock.elapsed());
    for (const int row : dueRows) {
        QString errorText;
        if (SendFrame(m_txModel->MessageAt(row).frame, errorText)) {
            m_txModel->MarkSent(row);
        }
    }
}

QString MainWindow::SelectedRowsText(const QTableView *view) const
{
    const QAbstractItemModel *model = view->model();
    QModelIndexList rows = view->selectionModel()->selectedRows();
    if (rows.isEmpty() || model == nullptr) {
        return QString();
    }
    std::sort(rows.begin(), rows.end(), [](const QModelIndex &a, const QModelIndex &b) { return a.row() < b.row(); });

    QStringList lines;
    QStringList header;
    for (int column = 0; column < model->columnCount(); ++column) {
        header << model->headerData(column, Qt::Horizontal).toString();
    }
    lines << header.join(QLatin1Char('\t'));
    for (const QModelIndex &rowIndex : std::as_const(rows)) {
        QStringList cells;
        for (int column = 0; column < model->columnCount(); ++column) {
            cells << model->index(rowIndex.row(), column).data(Qt::DisplayRole).toString();
        }
        lines << cells.join(QLatin1Char('\t'));
    }
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

void MainWindow::CopyRxRows()
{
    const QString text = SelectedRowsText(m_rxView);
    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);
    }
}

void MainWindow::RemoveRxRows()
{
    const QModelIndexList selected = m_rxView->selectionModel()->selectedRows();
    QList<int> sourceRows;
    for (const QModelIndex &index : selected) {
        sourceRows.append(m_rxProxy->mapToSource(index).row());
    }
    m_rxModel->RemoveRows(sourceRows);
    UpdateStatistics();
}

void MainWindow::CopyTraceRows()
{
    const QString text = SelectedRowsText(m_traceView);
    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);
    }
}

void MainWindow::ShowRxContextMenu(const QPoint &position)
{
    const bool hasSelection = m_rxView->selectionModel()->hasSelection();
    m_rxCopyAction->setEnabled(hasSelection);
    m_rxDeleteAction->setEnabled(hasSelection);

    QMenu menu(this);
    menu.addAction(m_rxCopyAction);
    menu.addAction(m_rxDeleteAction);
    menu.addAction(m_clearRxAction);
    menu.addSeparator();
    AddFormatMenus(&menu);
    menu.exec(m_rxView->viewport()->mapToGlobal(position));

    m_rxCopyAction->setEnabled(true);
    m_rxDeleteAction->setEnabled(true);
}

void MainWindow::ShowTxContextMenu(const QPoint &position)
{
    const bool hasSelection = m_txView->selectionModel()->hasSelection();
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    const bool canPaste = mime != nullptr && (mime->hasFormat(TxListFile::MimeType()) || mime->hasText());
    m_txEditAction->setEnabled(hasSelection);
    m_txSendAction->setEnabled(hasSelection && m_can && m_can->IsOpen());
    m_txCutAction->setEnabled(hasSelection);
    m_txCopyAction->setEnabled(hasSelection);
    m_txPasteAction->setEnabled(canPaste);
    m_txDeleteAction->setEnabled(hasSelection);
    m_txClearAction->setEnabled(m_txModel->rowCount() > 0);

    QMenu menu(this);
    menu.addAction(m_txNewAction);
    menu.addAction(m_txEditAction);
    menu.addAction(m_txSendAction);
    menu.addSeparator();
    menu.addAction(m_txCutAction);
    menu.addAction(m_txCopyAction);
    menu.addAction(m_txPasteAction);
    menu.addAction(m_txDeleteAction);
    menu.addAction(m_txClearAction);
    menu.addSeparator();
    AddFormatMenus(&menu);
    menu.addSeparator();
    menu.addAction(m_openTxListAction);
    menu.addAction(m_saveTxListAction);
    menu.exec(m_txView->viewport()->mapToGlobal(position));

    // Kısayollar menü dışında da çalışsın diye eylemler yeniden etkinleştirilir.
    const QList<QAction *> actions = {m_txEditAction, m_txSendAction, m_txCutAction, m_txCopyAction,
                                      m_txPasteAction, m_txDeleteAction, m_txClearAction};
    for (QAction *action : actions) {
        action->setEnabled(true);
    }
}

void MainWindow::ShowTraceContextMenu(const QPoint &position)
{
    m_traceCopyAction->setEnabled(m_traceView->selectionModel()->hasSelection());

    QMenu menu(this);
    menu.addAction(m_traceCopyAction);
    menu.addSeparator();
    QAction *pause = menu.addAction(Theme::Icon(QStringLiteral("pause")),
                                    m_traceModel->IsPaused() ? tr("Kaydı sürdür") : tr("Kaydı duraklat"));
    connect(pause, &QAction::triggered, m_tracePauseButton, &QPushButton::toggle);
    QAction *autoScroll = menu.addAction(tr("Otomatik kaydır"));
    autoScroll->setCheckable(true);
    autoScroll->setChecked(m_traceAutoScrollCheck->isChecked());
    connect(autoScroll, &QAction::toggled, m_traceAutoScrollCheck, &QCheckBox::setChecked);
    QAction *filter = menu.addAction(tr("Filtreyi uygula"));
    filter->setCheckable(true);
    filter->setChecked(m_traceFilterCheck->isChecked());
    connect(filter, &QAction::toggled, m_traceFilterCheck, &QCheckBox::setChecked);
    QAction *clear = menu.addAction(Theme::Icon(QStringLiteral("clear"), Theme::Colors().danger), tr("Trace'i temizle"));
    connect(clear, &QAction::triggered, this, &MainWindow::ClearTrace);
    menu.addAction(m_exportTraceAction);
    menu.addSeparator();
    AddFormatMenus(&menu);
    menu.exec(m_traceView->viewport()->mapToGlobal(position));

    m_traceCopyAction->setEnabled(true);
}

void MainWindow::OpenTxList()
{
    if (!ConfirmDiscardTxChanges()) {
        return;
    }
    const QSettings settings;
    const QString directory = settings.value(QStringLiteral("files/lastTxListDir")).toString();
    const QString filePath =
        QFileDialog::getOpenFileName(this, tr("Gönderim listesini aç"), directory, TxListFile::FileFilter());
    if (!filePath.isEmpty()) {
        LoadTxListFile(filePath);
    }
}

bool MainWindow::LoadTxListFile(const QString &filePath)
{
    QString errorText;
    const std::optional<QVector<TxMessage>> messages = TxListFile::Load(filePath, errorText);
    if (!messages) {
        QMessageBox::critical(this, tr("Gönderim listesi"),
                              tr("%1 açılamadı:\n%2").arg(QFileInfo(filePath).fileName(), errorText));
        return false;
    }
    m_txModel->SetMessages(*messages);
    m_txModel->ResetSchedule(m_clock.elapsed());
    m_txListPath = filePath;
    m_txListModified = false;
    AddRecentTxList(filePath);
    UpdateWindowTitle();
    m_messagesDock->raise();
    statusBar()->showMessage(tr("%1 mesaj yüklendi: %2").arg(messages->size()).arg(filePath), 5000);
    return true;
}

bool MainWindow::SaveTxList()
{
    if (m_txListPath.isEmpty()) {
        return SaveTxListAs();
    }
    return WriteTxListFile(m_txListPath);
}

bool MainWindow::SaveTxListAs()
{
    const QSettings settings;
    QString suggestion = m_txListPath;
    if (suggestion.isEmpty()) {
        const QString directory = settings.value(QStringLiteral("files/lastTxListDir")).toString();
        suggestion = QDir(directory).filePath(QStringLiteral("gonderim_listesi.cantx"));
    }
    QString filePath =
        QFileDialog::getSaveFileName(this, tr("Gönderim listesini kaydet"), suggestion, TxListFile::FileFilter());
    if (filePath.isEmpty()) {
        return false;
    }
    if (QFileInfo(filePath).suffix().isEmpty()) {
        filePath += QStringLiteral(".cantx");
    }
    return WriteTxListFile(filePath);
}

bool MainWindow::WriteTxListFile(const QString &filePath)
{
    QString errorText;
    if (!TxListFile::Save(filePath, m_txModel->Messages(), errorText)) {
        QMessageBox::critical(this, tr("Gönderim listesi"), tr("Dosya kaydedilemedi:\n%1").arg(errorText));
        return false;
    }
    m_txListPath = filePath;
    m_txListModified = false;
    AddRecentTxList(filePath);
    UpdateWindowTitle();
    statusBar()->showMessage(tr("%1 mesaj kaydedildi: %2").arg(m_txModel->rowCount()).arg(filePath), 5000);
    return true;
}

bool MainWindow::ConfirmDiscardTxChanges()
{
    if (!m_txListModified || (m_txModel->rowCount() == 0 && m_txListPath.isEmpty())) {
        return true;
    }
    const QString name = m_txListPath.isEmpty() ? tr("Adsız gönderim listesi") : QFileInfo(m_txListPath).fileName();
    const auto answer = QMessageBox::question(
        this, tr("Kaydedilmemiş değişiklikler"),
        tr("\"%1\" üzerinde kaydedilmemiş değişiklikler var. Kaydetmek ister misiniz?").arg(name),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save) {
        return SaveTxList();
    }
    return answer == QMessageBox::Discard;
}

void MainWindow::AddRecentTxList(const QString &filePath)
{
    QSettings settings;
    QStringList recent = settings.value(QStringLiteral("files/recentTxLists")).toStringList();
    const QString absolute = QFileInfo(filePath).absoluteFilePath();
    recent.removeAll(absolute);
    recent.prepend(absolute);
    while (recent.size() > MaxRecentFiles) {
        recent.removeLast();
    }
    settings.setValue(QStringLiteral("files/recentTxLists"), recent);
    settings.setValue(QStringLiteral("files/lastTxListDir"), QFileInfo(absolute).absolutePath());
    RebuildRecentMenu();
}

void MainWindow::RebuildRecentMenu()
{
    if (m_recentMenu == nullptr) {
        return;
    }
    m_recentMenu->clear();
    const QSettings settings;
    const QStringList recent = settings.value(QStringLiteral("files/recentTxLists")).toStringList();
    for (const QString &filePath : recent) {
        QAction *action = m_recentMenu->addAction(QFileInfo(filePath).fileName());
        action->setToolTip(filePath);
        action->setStatusTip(filePath);
        connect(action, &QAction::triggered, this, [this, filePath]() {
            if (ConfirmDiscardTxChanges()) {
                LoadTxListFile(filePath);
            }
        });
    }
    m_recentMenu->setEnabled(!recent.isEmpty());
}

void MainWindow::UpdateWindowTitle()
{
    // Kayıtlı bir gönderim listesi açık değilse yalnızca uygulama adı gösterilir.
    if (m_txListPath.isEmpty()) {
        setWindowTitle(tr("Canbus Analyzer"));
        setWindowModified(false);
        return;
    }
    setWindowTitle(tr("%1[*] — Canbus Analyzer").arg(QFileInfo(m_txListPath).fileName()));
    setWindowModified(m_txListModified);
}

void MainWindow::UpdateStatistics()
{
    const quint64 rate = m_rxTotal - m_rxAtLastStatistics;
    m_rxAtLastStatistics = m_rxTotal;
    m_rxCountLabel->setText(tr("Rx %1 · %2/s").arg(m_rxTotal).arg(rate));
    m_txCountLabel->setText(tr("Tx %1").arg(m_txTotal));

    const bool filterActive = m_filterPanel->Filter().IsEnabled();
    m_filterLabel->setText(filterActive ? tr("Filtre açık · %1/%2 ID").arg(m_rxProxy->rowCount()).arg(m_rxModel->rowCount())
                                        : tr("Filtre kapalı"));

    QString traceText = tr("%1 kayıt").arg(m_traceModel->rowCount());
    if (m_traceProxy->IsFilterActive() && filterActive) {
        traceText = tr("%1 / %2 kayıt").arg(m_traceProxy->rowCount()).arg(m_traceModel->rowCount());
    }
    if (m_traceModel->IsPaused()) {
        traceText += tr(" · duraklatıldı");
    }
    m_traceCountLabel->setText(traceText);
}

void MainWindow::ClearReceiveTable()
{
    m_rxModel->Clear();
    m_rxTotal = 0;
    m_rxAtLastStatistics = 0;
    UpdateStatistics();
}

void MainWindow::ClearTrace()
{
    m_traceModel->Clear();
    m_traceStartUs = m_clock.nsecsElapsed() / 1000;
    UpdateStatistics();
}

void MainWindow::ExportTrace()
{
    // Önce bekleyen çerçeveleri tabloya al ki dosya güncel olsun.
    m_traceModel->FlushPending();
    if (m_traceProxy->rowCount() == 0) {
        QMessageBox::information(this, tr("Trace"), tr("Kaydedilecek trace kaydı yok."));
        return;
    }

    const QString defaultName =
        QStringLiteral("trace_%1.csv").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString filePath =
        QFileDialog::getSaveFileName(this, tr("Trace'i kaydet"), defaultName, tr("CSV dosyaları (*.csv)"));
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::critical(this, tr("Trace"), tr("Dosya yazılamadı: %1").arg(file.errorString()));
        return;
    }
    QTextStream stream(&file);
    stream << TraceModel::CsvHeader() << '\n';
    const int rows = m_traceProxy->rowCount();
    for (int row = 0; row < rows; ++row) {
        const QModelIndex source = m_traceProxy->mapToSource(m_traceProxy->index(row, 0));
        stream << m_traceModel->CsvLine(source.row()) << '\n';
    }
    stream.flush();
    statusBar()->showMessage(tr("%1 kayıt kaydedildi: %2").arg(rows).arg(filePath), 6000);
}

void MainWindow::BrowseEdsFile()
{
    m_canOpenDock->show();
    m_canOpenDock->raise();
    m_canOpenPanel->BrowseEdsFile();
}

qint64 MainWindow::TraceTimeUs() const
{
    return m_clock.nsecsElapsed() / 1000 - m_traceStartUs;
}

QList<int> MainWindow::SelectedTxRows() const
{
    QList<int> rows;
    const QModelIndexList selected = m_txView->selectionModel()->selectedRows();
    for (const QModelIndex &index : selected) {
        rows.append(index.row());
    }
    std::sort(rows.begin(), rows.end());
    return rows;
}

void MainWindow::CheckForUpdates()
{
    if (m_updateChecker == nullptr) {
        m_updateChecker = new UpdateChecker(this);
        connect(m_updateChecker, &UpdateChecker::UpdateAvailable, this, [this](const ReleaseInfo &release) {
            QMessageBox box(QMessageBox::Information, tr("Güncelleme var"),
                            tr("Yeni sürüm mevcut: <b>%1</b> (kullandığınız: %2).")
                                .arg(release.tag.toHtmlEscaped(), QString::fromLatin1(AppInfo::Version)),
                            QMessageBox::NoButton, this);
            if (!release.notes.isEmpty()) {
                box.setDetailedText(release.notes);
            }
            QPushButton *download = box.addButton(tr("İndirme sayfasını aç"), QMessageBox::AcceptRole);
            box.addButton(tr("Daha sonra"), QMessageBox::RejectRole);
            box.exec();
            if (box.clickedButton() == download) {
                QDesktopServices::openUrl(release.pageUrl);
            }
        });
        connect(m_updateChecker, &UpdateChecker::UpToDate, this, [this](const ReleaseInfo &latest) {
            QMessageBox::information(this, tr("Güncelleme"),
                                     tr("Canbus Analyzer güncel (sürüm %1, en son yayın %2).")
                                         .arg(QString::fromLatin1(AppInfo::Version), latest.tag));
        });
        connect(m_updateChecker, &UpdateChecker::NoReleases, this, [this]() {
            QMessageBox::information(this, tr("Güncelleme"),
                                     tr("GitHub deposunda henüz yayınlanmış bir sürüm yok (%1/%2).")
                                         .arg(QString::fromLatin1(AppInfo::GitHubOwner),
                                              QString::fromLatin1(AppInfo::GitHubRepository)));
        });
        connect(m_updateChecker, &UpdateChecker::CheckFailed, this, [this](const QString &reason) {
            QMessageBox::warning(this, tr("Güncelleme"), tr("Güncelleme denetlenemedi:\n%1").arg(reason));
        });
    }
    statusBar()->showMessage(tr("Güncellemeler denetleniyor..."), 3000);
    m_updateChecker->CheckForUpdates();
}

void MainWindow::ShowAboutDialog()
{
    QMessageBox::about(this, tr("Canbus Analyzer Hakkında"),
                       tr("<h3>Canbus Analyzer %2</h3>"
                          "<p>CAN bus izleme ve CANopen (EDS, SDO, NMT, PDO) aracı.</p>"
                          "<p>Donanım: PEAK PCAN (PCAN-Basic) · CAN CC ve CAN FD.<br>"
                          "CAN XL, PCAN-Basic sürücüsü tarafından desteklenmediği için kullanılamaz.</p>"
                          "<p>Qt %1</p>")
                           .arg(QString::fromLatin1(qVersion()), QString::fromLatin1(AppInfo::Version)));
}
