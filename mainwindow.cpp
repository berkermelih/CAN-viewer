#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "src/can/ICanInterface.h"
#include "src/can/PcanInterface.h"
#include "src/can/VirtualCanInterface.h"
#include "src/models/RxFilterProxyModel.h"
#include "src/models/RxMessageModel.h"
#include "src/models/TxMessageModel.h"
#include "src/ui/CanOpenPanel.h"
#include "src/ui/FilterPanel.h"
#include "src/ui/TxMessageDialog.h"

#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_clock.start();

    m_rxModel = new RxMessageModel(this);
    m_txModel = new TxMessageModel(this);

    BuildUi();

    m_rxModel->SetInterpreter([this](const CanFrame &frame) { return m_canOpenPanel->InterpretFrame(frame); });
    m_canOpenPanel->SetSendFunction([this](const CanFrame &frame, QString &errorText) {
        return SendFrame(frame, errorText);
    });
    connect(m_canOpenPanel, &CanOpenPanel::InterpretationChanged, m_rxModel, &RxMessageModel::RefreshInterpretations);
    connect(m_filterPanel, &FilterPanel::FilterChanged, this, [this]() {
        m_rxProxy->RefreshFilter();
        UpdateStatistics();
    });

    m_txTimer.setInterval(1);
    m_txTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_txTimer, &QTimer::timeout, this, &MainWindow::ProcessPeriodicTx);

    m_statisticsTimer.setInterval(1000);
    connect(&m_statisticsTimer, &QTimer::timeout, this, &MainWindow::UpdateStatistics);
    m_statisticsTimer.start();

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
    resize(1280, 800);

    m_canOpenPanel = new CanOpenPanel(this);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(BuildMessagesPage(), tr("Mesajlar"));
    m_tabs->addTab(m_canOpenPanel, tr("CANopen / EDS"));
    setCentralWidget(m_tabs);

    m_filterPanel = new FilterPanel(this);
    m_rxProxy = new RxFilterProxyModel(&m_filterPanel->Filter(), this);
    m_rxProxy->setSourceModel(m_rxModel);
    m_rxView->setModel(m_rxProxy);
    m_rxView->sortByColumn(RxMessageModel::ColumnId, Qt::AscendingOrder);
    m_rxView->horizontalHeader()->setSectionResizeMode(RxMessageModel::ColumnInterpretation, QHeaderView::Stretch);
    m_rxView->setColumnWidth(RxMessageModel::ColumnId, 90);
    m_rxView->setColumnWidth(RxMessageModel::ColumnType, 70);
    m_rxView->setColumnWidth(RxMessageModel::ColumnDlc, 45);
    m_rxView->setColumnWidth(RxMessageModel::ColumnData, 210);

    auto *filterDock = new QDockWidget(tr("Filtre"), this);
    filterDock->setObjectName(QStringLiteral("FilterDock"));
    filterDock->setWidget(m_filterPanel);
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

    m_channelCombo = new QComboBox(this);
    const QList<QPair<QString, QString>> channels = PcanInterface::AvailableChannels();
    for (const auto &channel : channels) {
        m_channelCombo->addItem(channel.second, channel.first);
    }
    m_channelCombo->addItem(tr("Sanal (loopback)"), VirtualCanInterface::ChannelName());

    m_bitrateCombo = new QComboBox(this);
    const QList<int> bitrates = PcanInterface::SupportedBitrates();
    for (const int bitrate : bitrates) {
        m_bitrateCombo->addItem(bitrate >= 1000000 ? QStringLiteral("%1 Mbit/s").arg(bitrate / 1000000)
                                                   : QStringLiteral("%1 kbit/s").arg(bitrate / 1000),
                                bitrate);
    }
    m_bitrateCombo->setCurrentIndex(m_bitrateCombo->findData(500000));

    toolBar->addWidget(new QLabel(tr(" Kanal: "), this));
    toolBar->addWidget(m_channelCombo);
    toolBar->addWidget(new QLabel(tr("  Bit hızı: "), this));
    toolBar->addWidget(m_bitrateCombo);
    toolBar->addSeparator();

    m_connectAction = toolBar->addAction(tr("Bağlan"));
    m_connectAction->setShortcut(QKeySequence(Qt::Key_F5));
    connect(m_connectAction, &QAction::triggered, this, &MainWindow::ToggleConnection);

    toolBar->addSeparator();
    QAction *clearAction = toolBar->addAction(tr("Rx Temizle"));
    clearAction->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(clearAction, &QAction::triggered, this, &MainWindow::ClearReceiveTable);

    QAction *edsAction = toolBar->addAction(tr("EDS Aç"));
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
    fileMenu->addSeparator();
    QAction *quit = fileMenu->addAction(tr("Çı&kış"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QMainWindow::close);

    QMenu *busMenu = ui->menubar->addMenu(tr("&Bus"));
    busMenu->addAction(m_connectAction);
    QAction *clearRx = busMenu->addAction(tr("Rx tablosunu temizle"));
    connect(clearRx, &QAction::triggered, this, &MainWindow::ClearReceiveTable);
    QAction *resetTx = busMenu->addAction(tr("Tx sayaçlarını sıfırla"));
    connect(resetTx, &QAction::triggered, m_txModel, &TxMessageModel::ResetCounters);

    QMenu *viewMenu = ui->menubar->addMenu(tr("&Görünüm"));
    if (auto *filterDock = findChild<QDockWidget *>(QStringLiteral("FilterDock"))) {
        viewMenu->addAction(filterDock->toggleViewAction());
    }

    QMenu *helpMenu = ui->menubar->addMenu(tr("&Yardım"));
    QAction *about = helpMenu->addAction(tr("&Hakkında"));
    connect(about, &QAction::triggered, this, &MainWindow::ShowAboutDialog);
}

QWidget *MainWindow::BuildMessagesPage()
{
    const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    // Ortak tablo görünümü ayarları
    auto configureView = [&fixedFont](QTableView *view) {
        view->setFont(fixedFont);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);
        view->setAlternatingRowColors(true);
        view->verticalHeader()->setVisible(false);
        view->verticalHeader()->setDefaultSectionSize(view->fontMetrics().height() + 6);
        view->horizontalHeader()->setHighlightSections(false);
    };

    // --- Rx ---
    m_rxView = new QTableView(this);
    configureView(m_rxView);
    m_rxView->setSortingEnabled(true);
    m_rxView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    auto *rxBox = new QWidget(this);
    auto *rxLayout = new QVBoxLayout(rxBox);
    rxLayout->setContentsMargins(0, 0, 0, 0);
    rxLayout->addWidget(new QLabel(tr("<b>Alınan mesajlar</b>"), this));
    rxLayout->addWidget(m_rxView);

    // --- Tx ---
    m_txView = new QTableView(this);
    configureView(m_txView);
    m_txView->setModel(m_txModel);
    m_txView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_txView->horizontalHeader()->setSectionResizeMode(TxMessageModel::ColumnComment, QHeaderView::Stretch);
    m_txView->setColumnWidth(TxMessageModel::ColumnId, 110);
    m_txView->setColumnWidth(TxMessageModel::ColumnData, 210);
    connect(m_txView, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        if (index.column() != TxMessageModel::ColumnId) {
            EditTxMessage();
        }
    });

    auto *addButton = new QPushButton(tr("Yeni..."), this);
    auto *editButton = new QPushButton(tr("Düzenle..."), this);
    auto *removeButton = new QPushButton(tr("Sil"), this);
    auto *sendButton = new QPushButton(tr("Gönder"), this);
    sendButton->setToolTip(tr("Seçili mesajları bir kez gönder (Boşluk)"));
    connect(addButton, &QPushButton::clicked, this, &MainWindow::AddTxMessage);
    connect(editButton, &QPushButton::clicked, this, &MainWindow::EditTxMessage);
    connect(removeButton, &QPushButton::clicked, this, &MainWindow::RemoveTxMessages);
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::SendSelectedTxMessages);

    m_sendAction = new QAction(this);
    m_sendAction->setShortcut(QKeySequence(Qt::Key_Space));
    m_sendAction->setShortcutContext(Qt::WidgetShortcut);
    m_txView->addAction(m_sendAction);
    connect(m_sendAction, &QAction::triggered, this, &MainWindow::SendSelectedTxMessages);

    auto *txButtons = new QHBoxLayout;
    txButtons->addWidget(new QLabel(tr("<b>Gönderilecek mesajlar</b>"), this));
    txButtons->addStretch(1);
    txButtons->addWidget(addButton);
    txButtons->addWidget(editButton);
    txButtons->addWidget(removeButton);
    txButtons->addWidget(sendButton);

    auto *txBox = new QWidget(this);
    auto *txLayout = new QVBoxLayout(txBox);
    txLayout->setContentsMargins(0, 0, 0, 0);
    txLayout->addLayout(txButtons);
    txLayout->addWidget(m_txView);

    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(rxBox);
    splitter->addWidget(txBox);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(splitter);
    return page;
}

void MainWindow::BuildStatusBar()
{
    m_connectionLabel = new QLabel(this);
    m_busStatusLabel = new QLabel(this);
    m_rxCountLabel = new QLabel(this);
    m_txCountLabel = new QLabel(this);
    m_filterLabel = new QLabel(this);

    statusBar()->addWidget(m_connectionLabel);
    statusBar()->addWidget(m_busStatusLabel);
    statusBar()->addPermanentWidget(m_filterLabel);
    statusBar()->addPermanentWidget(m_rxCountLabel);
    statusBar()->addPermanentWidget(m_txCountLabel);
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

    if (connected) {
        m_connectionLabel->setText(tr("Bağlı: %1 @ %2").arg(m_can->Name(), m_bitrateCombo->currentText()));
    } else {
        m_connectionLabel->setText(tr("Bağlı değil"));
    }
}

void MainWindow::HandleFramesReceived(const QVector<CanFrame> &frames)
{
    m_rxTotal += static_cast<quint64>(frames.size());
    m_rxModel->AddFrames(frames);
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
    m_rxCountLabel->setText(tr("Rx: %1 (%2/s)").arg(m_rxTotal).arg(rate));
    m_txCountLabel->setText(tr("Tx: %1").arg(m_txTotal));

    const bool filterActive = m_filterPanel->Filter().IsEnabled();
    m_filterLabel->setText(filterActive ? tr("Filtre: açık (%1/%2 ID)").arg(m_rxProxy->rowCount()).arg(m_rxModel->rowCount())
                                        : tr("Filtre: kapalı"));
}

void MainWindow::ClearReceiveTable()
{
    m_rxModel->Clear();
    m_rxTotal = 0;
    m_rxAtLastStatistics = 0;
    UpdateStatistics();
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
                       tr("<b>Canbus Analyzer</b><br>"
                          "CAN bus izleme ve CANopen (EDS, SDO, NMT, PDO) aracı.<br><br>"
                          "Donanım: PEAK PCAN (PCAN-Basic) veya sanal loopback.<br>"
                          "Qt %1").arg(QString::fromLatin1(qVersion())));
}
