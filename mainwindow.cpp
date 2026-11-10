#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "src/can/ICanInterface.h"
#include "src/can/PcanInterface.h"
#include "src/can/VirtualCanInterface.h"
#include "src/models/FrameFilterProxyModel.h"
#include "src/models/RxMessageModel.h"
#include "src/models/TraceModel.h"
#include "src/models/TxMessageModel.h"
#include "src/ui/CanOpenPanel.h"
#include "src/ui/FilterPanel.h"
#include "src/ui/Theme.h"
#include "src/ui/TxMessageDialog.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_clock.start();

    m_rxModel = new RxMessageModel(this);
    m_txModel = new TxMessageModel(this);
    m_traceModel = new TraceModel(this);

    BuildUi();

    const auto interpreter = [this](const CanFrame &frame) { return m_canOpenPanel->InterpretFrame(frame); };
    m_rxModel->SetInterpreter(interpreter);
    m_traceModel->SetInterpreter(interpreter);
    m_canOpenPanel->SetSendFunction([this](const CanFrame &frame, QString &errorText) {
        return SendFrame(frame, errorText);
    });
    connect(m_canOpenPanel, &CanOpenPanel::InterpretationChanged, m_rxModel, &RxMessageModel::RefreshInterpretations);
    connect(m_canOpenPanel, &CanOpenPanel::InterpretationChanged, m_traceView->viewport(),
            [this]() { m_traceView->viewport()->update(); });
    connect(m_filterPanel, &FilterPanel::FilterChanged, this, [this]() {
        m_rxProxy->RefreshFilter();
        m_traceProxy->RefreshFilter();
        UpdateStatistics();
    });

    m_txTimer.setInterval(1);
    m_txTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_txTimer, &QTimer::timeout, this, &MainWindow::ProcessPeriodicTx);

    m_statisticsTimer.setInterval(1000);
    connect(&m_statisticsTimer, &QTimer::timeout, this, &MainWindow::UpdateStatistics);
    m_statisticsTimer.start();

    ApplyThemeColors();
    UpdateConnectionState();
    UpdateStatistics();
}

MainWindow::~MainWindow()
{
    if (m_can) {
        m_can->Close();
    }
    delete ui;
}

void MainWindow::BuildUi()
{
    setWindowTitle(tr("Canbus Analyzer"));
    setWindowIcon(Theme::CreateAppIcon());
    resize(1360, 840);

    m_canOpenPanel = new CanOpenPanel(this);
    m_filterPanel = new FilterPanel(this);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_tabs->addTab(BuildMessagesPage(), tr("Mesajlar"));
    m_tabs->addTab(BuildTracePage(), tr("Trace"));
    m_tabs->addTab(m_canOpenPanel, tr("CANopen / EDS"));
    setCentralWidget(m_tabs);

    // Rx: her ID için tek satır, sıralanabilir
    m_rxProxy = new FrameFilterProxyModel(
        &m_filterPanel->Filter(), [this](int row) { return m_rxModel->FramePointerAt(row); }, this);
    m_rxProxy->setSourceModel(m_rxModel);
    m_rxProxy->setSortRole(RxMessageModel::SortRole);
    m_rxView->setModel(m_rxProxy);
    m_rxView->sortByColumn(RxMessageModel::ColumnId, Qt::AscendingOrder);
    m_rxView->horizontalHeader()->setSectionResizeMode(RxMessageModel::ColumnInterpretation, QHeaderView::Stretch);
    m_rxView->setColumnWidth(RxMessageModel::ColumnId, 100);
    m_rxView->setColumnWidth(RxMessageModel::ColumnType, 80);
    m_rxView->setColumnWidth(RxMessageModel::ColumnDlc, 50);
    m_rxView->setColumnWidth(RxMessageModel::ColumnData, 240);
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
    m_traceView->setColumnWidth(TraceModel::ColumnType, 80);
    m_traceView->setColumnWidth(TraceModel::ColumnDlc, 50);
    m_traceView->setColumnWidth(TraceModel::ColumnData, 240);
    connect(m_traceProxy, &QAbstractItemModel::rowsInserted, this, [this]() {
        if (m_traceAutoScrollCheck->isChecked()) {
            m_traceView->scrollToBottom();
        }
    });

    auto *filterDock = new QDockWidget(tr("Filtre"), this);
    filterDock->setObjectName(QStringLiteral("FilterDock"));
    filterDock->setWidget(m_filterPanel);
    filterDock->setMinimumWidth(300);
    addDockWidget(Qt::RightDockWidgetArea, filterDock);

    BuildToolBar();
    BuildMenus();
    BuildStatusBar();
}

void MainWindow::BuildToolBar()
{
    QToolBar *toolBar = addToolBar(tr("Bağlantı"));
    toolBar->setObjectName(QStringLiteral("ConnectionToolBar"));
    toolBar->setMovable(false);
    toolBar->setFloatable(false);

    auto *brandIcon = new QLabel(this);
    brandIcon->setPixmap(Theme::CreateAppIcon().pixmap(26, 26));
    auto *brandLabel = new QLabel(tr("Canbus Analyzer"), this);
    brandLabel->setObjectName(QStringLiteral("BrandLabel"));
    toolBar->addWidget(brandIcon);
    toolBar->addWidget(brandLabel);
    toolBar->addSeparator();

    m_channelCombo = new QComboBox(this);
    const QList<QPair<QString, QString>> channels = PcanInterface::AvailableChannels();
    for (const auto &channel : channels) {
        m_channelCombo->addItem(channel.second, channel.first);
    }
    m_channelCombo->addItem(tr("Sanal (loopback)"), VirtualCanInterface::ChannelName());
    m_channelCombo->setMinimumWidth(150);

    m_bitrateCombo = new QComboBox(this);
    const QList<int> bitrates = PcanInterface::SupportedBitrates();
    for (const int bitrate : bitrates) {
        m_bitrateCombo->addItem(bitrate >= 1000000 ? QStringLiteral("%1 Mbit/s").arg(bitrate / 1000000)
                                                   : QStringLiteral("%1 kbit/s").arg(bitrate / 1000),
                                bitrate);
    }
    m_bitrateCombo->setCurrentIndex(m_bitrateCombo->findData(500000));
    m_bitrateCombo->setMinimumWidth(110);

    toolBar->addWidget(new QLabel(tr("Kanal"), this));
    toolBar->addWidget(m_channelCombo);
    toolBar->addWidget(new QLabel(tr("Bit hızı"), this));
    toolBar->addWidget(m_bitrateCombo);

    m_connectAction = toolBar->addAction(tr("Bağlan"));
    m_connectAction->setShortcut(QKeySequence(Qt::Key_F5));
    m_connectAction->setToolTip(tr("Bağlan / bağlantıyı kes (F5)"));
    connect(m_connectAction, &QAction::triggered, this, &MainWindow::ToggleConnection);
    m_connectButton = toolBar->widgetForAction(m_connectAction);
    if (m_connectButton != nullptr) {
        m_connectButton->setObjectName(QStringLiteral("PrimaryAction"));
        m_connectButton->setCursor(Qt::PointingHandCursor);
    }

    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);

    QAction *clearAction = toolBar->addAction(tr("Rx Temizle"));
    clearAction->setShortcut(QKeySequence(Qt::Key_Escape));
    clearAction->setToolTip(tr("Alınan mesajlar tablosunu temizle (Esc)"));
    connect(clearAction, &QAction::triggered, this, &MainWindow::ClearReceiveTable);

    QAction *edsAction = toolBar->addAction(tr("EDS Aç"));
    edsAction->setToolTip(tr("EDS dosyası aç (Ctrl+O)"));
    connect(edsAction, &QAction::triggered, this, [this]() {
        m_tabs->setCurrentWidget(m_canOpenPanel);
        m_canOpenPanel->BrowseEdsFile();
    });
}

void MainWindow::BuildMenus()
{
    QMenu *fileMenu = ui->menubar->addMenu(tr("&Dosya"));
    QAction *openEds = fileMenu->addAction(tr("EDS Dosyası &Aç..."));
    openEds->setShortcut(QKeySequence::Open);
    connect(openEds, &QAction::triggered, this, [this]() {
        m_tabs->setCurrentWidget(m_canOpenPanel);
        m_canOpenPanel->BrowseEdsFile();
    });
    QAction *exportTrace = fileMenu->addAction(tr("Trace'i CSV olarak &kaydet..."));
    exportTrace->setShortcut(QKeySequence::Save);
    connect(exportTrace, &QAction::triggered, this, &MainWindow::ExportTrace);
    fileMenu->addSeparator();
    QAction *quit = fileMenu->addAction(tr("Çı&kış"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QMainWindow::close);

    QMenu *busMenu = ui->menubar->addMenu(tr("&Bus"));
    busMenu->addAction(m_connectAction);
    busMenu->addSeparator();
    QAction *clearRx = busMenu->addAction(tr("Rx tablosunu temizle"));
    connect(clearRx, &QAction::triggered, this, &MainWindow::ClearReceiveTable);
    QAction *clearTrace = busMenu->addAction(tr("Trace'i temizle"));
    connect(clearTrace, &QAction::triggered, this, &MainWindow::ClearTrace);
    QAction *resetTx = busMenu->addAction(tr("Tx sayaçlarını sıfırla"));
    connect(resetTx, &QAction::triggered, m_txModel, &TxMessageModel::ResetCounters);

    QMenu *viewMenu = ui->menubar->addMenu(tr("&Görünüm"));
    m_darkThemeAction = viewMenu->addAction(tr("Koyu tema"));
    m_darkThemeAction->setCheckable(true);
    m_darkThemeAction->setChecked(Theme::CurrentMode() == Theme::Mode::Dark);
    m_darkThemeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    connect(m_darkThemeAction, &QAction::toggled, this, &MainWindow::SetDarkTheme);
    viewMenu->addSeparator();
    if (auto *filterDock = findChild<QDockWidget *>(QStringLiteral("FilterDock"))) {
        viewMenu->addAction(filterDock->toggleViewAction());
    }

    QMenu *helpMenu = ui->menubar->addMenu(tr("&Yardım"));
    QAction *about = helpMenu->addAction(tr("&Hakkında"));
    connect(about, &QAction::triggered, this, &MainWindow::ShowAboutDialog);
}

void MainWindow::ConfigureTableView(QTableView *view) const
{
    view->setFont(Theme::MonospaceFont());
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setAlternatingRowColors(true);
    view->setShowGrid(false);
    view->setWordWrap(false);
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

    auto *rxTitle = new QLabel(tr("Alınan mesajlar"), this);
    rxTitle->setObjectName(QStringLiteral("SectionTitle"));
    auto *rxHint = new QLabel(tr("Her ID tek satırda · verisi değişen satırlar vurgulanır"), this);
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
    m_txView->setColumnWidth(TxMessageModel::ColumnType, 80);
    m_txView->setColumnWidth(TxMessageModel::ColumnDlc, 50);
    m_txView->setColumnWidth(TxMessageModel::ColumnData, 240);
    m_txView->setColumnWidth(TxMessageModel::ColumnPeriod, 110);
    connect(m_txView, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        if (index.column() != TxMessageModel::ColumnId) {
            EditTxMessage();
        }
    });

    auto *txTitle = new QLabel(tr("Gönderilecek mesajlar"), this);
    txTitle->setObjectName(QStringLiteral("SectionTitle"));
    auto *addButton = new QPushButton(tr("Yeni..."), this);
    auto *editButton = new QPushButton(tr("Düzenle..."), this);
    auto *removeButton = new QPushButton(tr("Sil"), this);
    auto *sendButton = new QPushButton(tr("Gönder"), this);
    sendButton->setProperty("primary", true);
    sendButton->setToolTip(tr("Seçili mesajları bir kez gönder (tabloda Boşluk)"));
    connect(addButton, &QPushButton::clicked, this, &MainWindow::AddTxMessage);
    connect(editButton, &QPushButton::clicked, this, &MainWindow::EditTxMessage);
    connect(removeButton, &QPushButton::clicked, this, &MainWindow::RemoveTxMessages);
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::SendSelectedTxMessages);

    auto *sendAction = new QAction(this);
    sendAction->setShortcut(QKeySequence(Qt::Key_Space));
    sendAction->setShortcutContext(Qt::WidgetShortcut);
    m_txView->addAction(sendAction);
    connect(sendAction, &QAction::triggered, this, &MainWindow::SendSelectedTxMessages);

    auto *txHeader = new QHBoxLayout;
    txHeader->addWidget(txTitle);
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
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::ExportTrace);

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
    const QString channel = m_channelCombo->currentData().toString();
    const int bitrate = m_bitrateCombo->currentData().toInt();

    std::unique_ptr<ICanInterface> device;
    if (channel == VirtualCanInterface::ChannelName()) {
        device = std::make_unique<VirtualCanInterface>();
    } else {
        device = std::make_unique<PcanInterface>();
    }

    connect(device.get(), &ICanInterface::FramesReceived, this, &MainWindow::HandleFramesReceived);
    connect(device.get(), &ICanInterface::BusStatusChanged, m_busStatusLabel, &QLabel::setText);
    connect(device.get(), &ICanInterface::ErrorOccurred, this,
            [this](const QString &text) { statusBar()->showMessage(text, 5000); });

    QString errorText;
    if (!device->Open(channel, bitrate, errorText)) {
        QMessageBox::critical(this, tr("Bağlantı Hatası"), errorText);
        return;
    }

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
}

void MainWindow::UpdateConnectionState()
{
    const bool connected = m_can && m_can->IsOpen();
    m_connectAction->setText(connected ? tr("Bağlantıyı Kes") : tr("Bağlan"));
    m_channelCombo->setEnabled(!connected);
    m_bitrateCombo->setEnabled(!connected);
    m_canOpenPanel->SetConnected(connected);

    if (m_connectButton != nullptr) {
        m_connectButton->setProperty("connected", connected);
        Theme::Repolish(m_connectButton);
    }

    const Theme::Palette &colors = Theme::Colors();
    if (connected) {
        m_connectionLabel->setText(Theme::StatusDotText(
            colors.success, tr("Bağlı: %1 @ %2").arg(m_can->Name(), m_bitrateCombo->currentText())));
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

void MainWindow::AddTxMessage()
{
    TxMessageDialog dialog(this);
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

void MainWindow::ShowAboutDialog()
{
    QMessageBox::about(this, tr("Canbus Analyzer Hakkında"),
                       tr("<h3>Canbus Analyzer</h3>"
                          "<p>CAN bus izleme ve CANopen (EDS, SDO, NMT, PDO) aracı.</p>"
                          "<p>Donanım: PEAK PCAN (PCAN-Basic) veya sanal loopback.</p>"
                          "<p>Qt %1</p>").arg(QString::fromLatin1(qVersion())));
}
