#include "CanOpenPanel.h"

#include "../canopen/SdoClient.h"
#include "../eds/DcfWriter.h"
#include "../eds/EdsParser.h"
#include "Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QDir>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QRegularExpression>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace {

enum TreeColumn
{
    TreeColumnIndex,
    TreeColumnName,
    TreeColumnType,
    TreeColumnAccess,
    TreeColumnDefault,
    TreeColumnParameter,
    TreeColumnPdo,
    TreeColumnValue,
    TreeColumnTotal
};

// Ağaç öğelerinde saklanan roller
constexpr int RoleIndex = Qt::UserRole + 1;
constexpr int RoleSubIndex = Qt::UserRole + 2;
constexpr int RoleIsEntry = Qt::UserRole + 3;

// Salt okunur bir ağaç hücresi oluşturur.
QStandardItem *MakeItem(const QString &text)
{
    auto *item = new QStandardItem(text);
    item->setEditable(false);
    return item;
}

} // namespace

CanOpenPanel::CanOpenPanel(QWidget *parent)
    : QWidget(parent)
    , m_sdoClient(new SdoClient(this))
{
    BuildUi();

    m_sdoClient->SetSendFunction([this](const CanFrame &frame, QString &errorText) {
        if (!m_send) {
            errorText = tr("Gönderim fonksiyonu ayarlanmamış.");
            return false;
        }
        const bool sent = m_send(frame, errorText);
        // Elle başlatılan SDO aktarımlarının çerçeveleri Tx tablosunda gösterilir (parametre seti hariç).
        if (sent && !m_sdoTxDescription.isEmpty() && !IsBatchActive()) {
            emit ManualFrameSent(frame, m_sdoTxFrameCount++ == 0 ? m_sdoTxDescription
                                                                 : tr("%1 · devam").arg(m_sdoTxDescription));
        }
        return sent;
    });
    connect(m_sdoClient, &SdoClient::UploadFinished, this, &CanOpenPanel::HandleUploadFinished);
    connect(m_sdoClient, &SdoClient::DownloadFinished, this, &CanOpenPanel::HandleDownloadFinished);
    connect(m_sdoClient, &SdoClient::TransferFailed, this, &CanOpenPanel::HandleTransferFailed);

    UpdateButtonStates();
}

void CanOpenPanel::SetSendFunction(SendFunction function)
{
    m_send = std::move(function);
}

void CanOpenPanel::BuildUi()
{
    const QFont fixedFont = Theme::MonospaceFont();

    // --- Sol: EDS ağacı ---
    auto *titleLabel = new QLabel(tr("Nesne sözlüğü"), this);
    titleLabel->setObjectName(QStringLiteral("SectionTitle"));
    auto *openButton = new QPushButton(tr("EDS / DCF Aç..."), this);
    openButton->setToolTip(tr("EDS (cihaz tanımı) veya DCF (cihaz yapılandırması) dosyası aç"));
    openButton->setProperty("primary", true);
    connect(openButton, &QPushButton::clicked, this, &CanOpenPanel::BrowseEdsFile);
    m_fileLabel = new QLabel(tr("EDS / DCF dosyası yüklenmedi."), this);
    m_fileLabel->setObjectName(QStringLiteral("MutedText"));
    m_fileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(titleLabel);
    titleRow->addSpacing(8);
    titleRow->addWidget(m_fileLabel, 1);
    titleRow->addWidget(openButton);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Ara: index (ör. 1018) veya ad (ör. Statusword)"));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &CanOpenPanel::ApplySearch);

    m_treeModel = new QStandardItemModel(0, TreeColumnTotal, this);
    m_treeModel->setHorizontalHeaderLabels(
        {tr("Index"), tr("Ad"), tr("Veri tipi"), tr("Erişim"), tr("Varsayılan"), tr("DCF değeri"), tr("PDO"),
         tr("Okunan değer")});

    m_treeProxy = new QSortFilterProxyModel(this);
    m_treeProxy->setSourceModel(m_treeModel);
    m_treeProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_treeProxy->setFilterKeyColumn(-1);
    m_treeProxy->setRecursiveFilteringEnabled(true);
    m_treeProxy->setAutoAcceptChildRows(true);

    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_treeProxy);
    m_treeView->setUniformRowHeights(true);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setAnimated(true);
    m_treeView->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_treeView->header()->setStretchLastSection(true);
    m_treeView->setColumnWidth(TreeColumnIndex, 190);
    m_treeView->setColumnWidth(TreeColumnName, 260);
    m_treeView->setColumnWidth(TreeColumnType, 120);
    m_treeView->setColumnWidth(TreeColumnAccess, 70);
    m_treeView->setColumnWidth(TreeColumnDefault, 130);
    m_treeView->setColumnWidth(TreeColumnParameter, 110);
    m_treeView->setColumnWidth(TreeColumnPdo, 50);
    connect(m_treeView->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current, const QModelIndex &) { HandleTreeSelection(current); });
    connect(m_treeView, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        const QModelIndex first = index.siblingAtColumn(TreeColumnIndex);
        if (first.data(RoleIsEntry).toBool() && m_connected) {
            ReadSelectedObject();
        }
    });

    auto *leftWidget = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);
    leftLayout->addLayout(titleRow);
    leftLayout->addWidget(m_searchEdit);
    leftLayout->addWidget(m_treeView, 1);

    // --- Sağ: düğüm, SDO, NMT, günlük ---
    m_nodeIdSpin = new QSpinBox(this);
    m_nodeIdSpin->setRange(1, CanOpen::MaxNodeId);
    m_nodeIdSpin->setValue(1);
    connect(m_nodeIdSpin, &QSpinBox::valueChanged, this, [this]() {
        RebuildPdoDecoder();
        emit InterpretationChanged();
    });

    auto *nodeGroup = new QGroupBox(tr("Hedef düğüm"), this);
    auto *nodeLayout = new QFormLayout(nodeGroup);
    nodeLayout->addRow(tr("Node ID:"), m_nodeIdSpin);

    m_indexEdit = new QLineEdit(this);
    m_indexEdit->setPlaceholderText(QStringLiteral("1018"));
    m_indexEdit->setFont(fixedFont);
    m_subIndexEdit = new QLineEdit(this);
    m_subIndexEdit->setPlaceholderText(QStringLiteral("01"));
    m_subIndexEdit->setFont(fixedFont);
    m_dataTypeCombo = new QComboBox(this);
    m_dataTypeCombo->addItem(tr("Bilinmiyor (hex bayt)"), 0);
    const quint16 types[] = {
        CanOpenDataType::Boolean,    CanOpenDataType::Integer8,      CanOpenDataType::Integer16,
        CanOpenDataType::Integer32,  CanOpenDataType::Integer64,     CanOpenDataType::Unsigned8,
        CanOpenDataType::Unsigned16, CanOpenDataType::Unsigned32,    CanOpenDataType::Unsigned64,
        CanOpenDataType::Real32,     CanOpenDataType::Real64,        CanOpenDataType::VisibleString,
        CanOpenDataType::OctetString, CanOpenDataType::UnicodeString, CanOpenDataType::Domain};
    for (const quint16 type : types) {
        m_dataTypeCombo->addItem(DataTypeName(type), type);
    }
    m_valueEdit = new QLineEdit(this);
    m_valueEdit->setFont(fixedFont);

    m_readButton = new QPushButton(tr("Oku (Download)"), this);
    m_readButton->setProperty("primary", true);
    m_writeButton = new QPushButton(tr("Yaz (Upload)"), this);
    m_writeButton->setProperty("primary", true);
    connect(m_readButton, &QPushButton::clicked, this, &CanOpenPanel::ReadSelectedObject);
    connect(m_writeButton, &QPushButton::clicked, this, &CanOpenPanel::WriteSelectedObject);
    connect(m_valueEdit, &QLineEdit::returnPressed, this, &CanOpenPanel::WriteSelectedObject);

    m_saveVendorButton = new QPushButton(tr("Kaydet (Vendor)"), this);
    m_saveVendorButton->setToolTip(tr("Hedef düğüme 2105h:00 ← \"save\" yazar (ör. Node 2: 602 · 23 05 21 00 73 61 76 65)."));
    m_saveParameterButton = new QPushButton(tr("Kaydet (parametre)"), this);
    m_saveParameterButton->setToolTip(
        tr("Hedef düğüme 1010h:01 ← \"save\" yazar (ör. Node 2: 602 · 23 10 10 01 73 61 76 65)."));
    connect(m_saveVendorButton, &QPushButton::clicked, this,
            [this]() { SendSaveCommand(0x2105, 0x00, tr("Kaydet (Vendor)")); });
    connect(m_saveParameterButton, &QPushButton::clicked, this,
            [this]() { SendSaveCommand(0x1010, 0x01, tr("Kaydet (parametre)")); });

    auto *addressRow = new QHBoxLayout;
    addressRow->addWidget(m_indexEdit, 2);
    addressRow->addWidget(new QLabel(QStringLiteral(":"), this));
    addressRow->addWidget(m_subIndexEdit, 1);

    auto *sdoButtons = new QHBoxLayout;
    sdoButtons->addWidget(m_readButton);
    sdoButtons->addWidget(m_writeButton);
    auto *saveButtons = new QHBoxLayout;
    saveButtons->addWidget(m_saveVendorButton);
    saveButtons->addWidget(m_saveParameterButton);

    auto *sdoGroup = new QGroupBox(tr("SDO"), this);
    auto *sdoLayout = new QFormLayout(sdoGroup);
    sdoLayout->addRow(tr("Index:Sub (hex):"), addressRow);
    sdoLayout->addRow(tr("Veri tipi:"), m_dataTypeCombo);
    sdoLayout->addRow(tr("Değer:"), m_valueEdit);
    sdoLayout->addRow(sdoButtons);
    sdoLayout->addRow(saveButtons);

    // Özel metin ("0 (tüm düğümler)") kullanılmaz; aksi halde 0 seçiliyken üzerine rakam yazılamaz.
    m_nmtTargetSpin = new QSpinBox(this);
    m_nmtTargetSpin->setRange(0, CanOpen::MaxNodeId);
    m_nmtTargetSpin->setValue(0);
    m_nmtTargetSpin->setToolTip(tr("0: tüm düğümler (broadcast), 1–127: tek düğüm"));
    m_nmtTargetHint = new QLabel(this);
    m_nmtTargetHint->setObjectName(QStringLiteral("MutedText"));
    const auto updateTargetHint = [this](int value) {
        m_nmtTargetHint->setText(value == 0 ? tr("tüm düğümler") : tr("Node %1").arg(value));
    };
    updateTargetHint(m_nmtTargetSpin->value());
    connect(m_nmtTargetSpin, &QSpinBox::valueChanged, this, updateTargetHint);

    auto *nmtGroup = new QGroupBox(tr("NMT / SYNC"), this);
    auto *nmtLayout = new QGridLayout(nmtGroup);
    nmtLayout->addWidget(new QLabel(tr("Hedef:"), this), 0, 0);
    nmtLayout->addWidget(m_nmtTargetSpin, 0, 1);
    nmtLayout->addWidget(m_nmtTargetHint, 0, 2);

    struct NmtButtonInfo
    {
        const char *text;
        CanOpen::NmtCommand command;
    };
    const NmtButtonInfo nmtButtons[] = {
        {"Start", CanOpen::NmtCommand::Start},
        {"Stop", CanOpen::NmtCommand::Stop},
        {"Pre-operational", CanOpen::NmtCommand::PreOperational},
        {"Reset node", CanOpen::NmtCommand::ResetNode},
        {"Reset comm.", CanOpen::NmtCommand::ResetCommunication},
    };
    int buttonIndex = 0;
    for (const NmtButtonInfo &info : nmtButtons) {
        auto *button = new QPushButton(QString::fromLatin1(info.text), this);
        const CanOpen::NmtCommand command = info.command;
        connect(button, &QPushButton::clicked, this, [this, command]() { SendNmtCommand(command); });
        nmtLayout->addWidget(button, 1 + buttonIndex / 3, buttonIndex % 3);
        m_busButtons.append(button);
        ++buttonIndex;
    }
    auto *syncButton = new QPushButton(QStringLiteral("SYNC"), this);
    connect(syncButton, &QPushButton::clicked, this, &CanOpenPanel::SendSync);
    nmtLayout->addWidget(syncButton, 1 + buttonIndex / 3, buttonIndex % 3);
    m_busButtons.append(syncButton);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setFont(fixedFont);
    m_log->setMaximumBlockCount(2000);
    auto *clearLogButton = new QPushButton(tr("Temizle"), this);
    connect(clearLogButton, &QPushButton::clicked, m_log, &QPlainTextEdit::clear);

    auto *rightWidget = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);
    rightLayout->addWidget(nodeGroup);
    rightLayout->addWidget(sdoGroup);

    // --- Parametre seti (DCF) ---
    m_uploadSetButton = new QPushButton(tr("Cihazdan oku → DCF kaydet..."), this);
    m_uploadSetButton->setToolTip(tr("EDS'teki parametreleri cihazdan sırayla okur ve değerleriyle birlikte DCF "
                                     "dosyasına kaydeder (upload)."));
    m_downloadSetButton = new QPushButton(tr("DCF'ten cihaza yaz..."), this);
    m_downloadSetButton->setToolTip(tr("Kayıtlı bir DCF dosyasındaki ParameterValue değerlerini cihaza yazar "
                                       "(download)."));
    connect(m_uploadSetButton, &QPushButton::clicked, this, &CanOpenPanel::UploadParameterSet);
    connect(m_downloadSetButton, &QPushButton::clicked, this, &CanOpenPanel::DownloadParameterSet);
    m_includeReadOnlyCheck = new QCheckBox(tr("Salt okunur nesneleri de oku"), this);
    m_includeReadOnlyCheck->setToolTip(tr("Kapalıyken yalnızca yazılabilir (rw) parametreler okunur; bunlar daha "
                                          "sonra geri yüklenebilecek ayarlardır."));
    m_storeCheck = new QCheckBox(tr("Yazdıktan sonra kalıcı kaydet (1010h:01 \"save\")"), this);
    m_storeCheck->setToolTip(tr("Cihaz destekliyorsa değerleri kalıcı belleğe kaydeder."));
    m_batchProgress = new QProgressBar(this);
    m_batchProgress->setTextVisible(true);
    m_batchProgress->setVisible(false);
    m_cancelBatchButton = new QPushButton(tr("İptal"), this);
    m_cancelBatchButton->setVisible(false);
    connect(m_cancelBatchButton, &QPushButton::clicked, this, &CanOpenPanel::CancelBatch);
    m_batchStatusLabel = new QLabel(tr("Yazmadan önce cihazı Pre-operational durumuna almanız önerilir."), this);
    m_batchStatusLabel->setObjectName(QStringLiteral("MutedText"));
    m_batchStatusLabel->setWordWrap(true);

    auto *setButtons = new QHBoxLayout;
    setButtons->addWidget(m_uploadSetButton);
    setButtons->addWidget(m_downloadSetButton);
    auto *progressRow = new QHBoxLayout;
    progressRow->addWidget(m_batchProgress, 1);
    progressRow->addWidget(m_cancelBatchButton);

    auto *setGroup = new QGroupBox(tr("Parametre seti (DCF)"), this);
    auto *setLayout = new QVBoxLayout(setGroup);
    setLayout->setSpacing(6);
    setLayout->addLayout(setButtons);
    setLayout->addWidget(m_includeReadOnlyCheck);
    setLayout->addWidget(m_storeCheck);
    setLayout->addLayout(progressRow);
    setLayout->addWidget(m_batchStatusLabel);
    rightLayout->addWidget(setGroup);
    rightLayout->addWidget(nmtGroup);
    auto *logTitle = new QLabel(tr("Günlük"), this);
    logTitle->setObjectName(QStringLiteral("SectionTitle"));
    auto *logHeader = new QHBoxLayout;
    logHeader->addWidget(logTitle);
    logHeader->addStretch(1);
    logHeader->addWidget(clearLogButton);
    rightLayout->addLayout(logHeader);
    rightLayout->addWidget(m_log, 1);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(10);
    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->addWidget(splitter);
}

bool CanOpenPanel::OpenEdsFile(const QString &filePath)
{
    QString errorText;
    QStringList warnings;
    std::optional<ObjectDictionary> dictionary = EdsParser::OpenEdsFile(filePath, errorText, warnings);
    if (!dictionary) {
        AppendLog(tr("EDS yüklenemedi: %1").arg(errorText));
        QMessageBox::warning(this, tr("EDS Hatası"), errorText);
        return false;
    }

    m_dictionary = std::move(*dictionary);
    PopulateTree();
    RebuildPdoDecoder();

    // DCF: kaydedildiği düğüm numarasını hedef olarak al.
    const QString commissionedNode = m_dictionary.CommissioningValue(QStringLiteral("NodeID"));
    if (const auto nodeId = ParseEdsNumber(commissionedNode); nodeId && *nodeId >= 1 && *nodeId <= CanOpen::MaxNodeId) {
        m_nodeIdSpin->setValue(static_cast<int>(*nodeId));
        AppendLog(tr("DCF: hedef düğüm %1 olarak ayarlandı ([DeviceComissioning] NodeID).").arg(*nodeId));
    }

    const QString product = m_dictionary.DeviceInfoValue(QStringLiteral("productname"));
    const QString vendor = m_dictionary.DeviceInfoValue(QStringLiteral("vendorname"));
    QString label = QFileInfo(filePath).fileName();
    if (!product.isEmpty()) {
        label += QStringLiteral(" — ") + product;
        if (!vendor.isEmpty()) {
            label += QStringLiteral(" (%1)").arg(vendor);
        }
    }
    m_fileLabel->setText(label);
    m_fileLabel->setToolTip(filePath);

    AppendLog(tr("%1 yüklendi: %2 — %3 nesne, %4 PDO tanımı.")
                  .arg(m_dictionary.HasParameterValues() ? QStringLiteral("DCF") : QStringLiteral("EDS"))
                  .arg(QFileInfo(filePath).fileName())
                  .arg(m_dictionary.Objects().size())
                  .arg(m_pdoDecoder.Count()));
    for (const QString &warning : std::as_const(warnings)) {
        AppendLog(tr("Uyarı: %1").arg(warning));
    }
    emit InterpretationChanged();
    return true;
}

void CanOpenPanel::BrowseEdsFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("EDS / DCF Dosyası Aç"), QString(),
        tr("EDS / DCF dosyaları (*.eds *.EDS *.dcf *.DCF);;EDS dosyaları (*.eds *.EDS);;DCF dosyaları (*.dcf *.DCF);;"
           "Tüm dosyalar (*)"));
    if (!filePath.isEmpty()) {
        OpenEdsFile(filePath);
    }
}

void CanOpenPanel::HandleFrames(const QVector<CanFrame> &frames)
{
    if (!m_sdoClient->IsBusy()) {
        return;
    }
    for (const CanFrame &frame : frames) {
        m_sdoClient->HandleFrame(frame);
    }
}

QString CanOpenPanel::InterpretFrame(const CanFrame &frame) const
{
    if (frame.fd) {
        return QString(); // CANopen FD (CiA 1301) bu sürümde yorumlanmaz.
    }
    const QString pdoText = m_pdoDecoder.DecodeFrame(frame);
    return pdoText.isEmpty() ? CanOpen::DescribeFrame(frame) : pdoText;
}

void CanOpenPanel::SetConnected(bool connected)
{
    m_connected = connected;
    if (!connected && m_sdoClient->IsBusy()) {
        m_sdoClient->Cancel();
    }
    UpdateButtonStates();
}

void CanOpenPanel::ApplyThemeColors()
{
    const QColor accent = Theme::Colors().accent;
    for (const auto &group : std::as_const(m_groupItems)) {
        group.first->setForeground(accent);
    }
}

void CanOpenPanel::ApplyGroupSpans()
{
    for (const auto &group : std::as_const(m_groupItems)) {
        const QModelIndex proxyIndex = m_treeProxy->mapFromSource(group.first->index());
        if (proxyIndex.isValid()) {
            m_treeView->setFirstColumnSpanned(proxyIndex.row(), proxyIndex.parent(), true);
        }
    }
}

void CanOpenPanel::RestoreDefaultExpansion()
{
    for (const auto &group : std::as_const(m_groupItems)) {
        m_treeView->setExpanded(m_treeProxy->mapFromSource(group.first->index()), group.second);
    }
}

void CanOpenPanel::ApplySearch(const QString &text)
{
    m_treeProxy->setFilterFixedString(text.trimmed());
    ApplyGroupSpans();
    if (text.trimmed().isEmpty()) {
        m_treeView->collapseAll();
        RestoreDefaultExpansion();
    } else {
        m_treeView->expandAll();
    }
}

void CanOpenPanel::PopulateTree()
{
    m_treeModel->removeRows(0, m_treeModel->rowCount());
    m_valueItems.clear();
    m_groupItems.clear();

    // CiA 301 index alanlarına göre başlıklar (son başlık, hiçbir alana girmeyenleri de toplar).
    struct GroupInfo
    {
        quint16 first;
        quint16 last;
        const char *title;
        const char *range;
        bool expanded;
    };
    const GroupInfo groups[] = {
        {0x0001, 0x0FFF, QT_TR_NOOP("Veri tipleri"), "0001–0FFF", false},
        {0x1000, 0x1FFF, QT_TR_NOOP("Temel — Communication profile"), "1000–1FFF", true},
        {0x2000, 0x5FFF, QT_TR_NOOP("Vendor specific — Manufacturer"), "2000–5FFF", true},
        {0x6000, 0x9FFF, QT_TR_NOOP("Device specific — Device profile"), "6000–9FFF", true},
        {0xA000, 0xFFFF, QT_TR_NOOP("Diğer — Interface profile / reserved"), "A000–FFFF", false},
    };
    constexpr int groupCount = static_cast<int>(sizeof(groups) / sizeof(groups[0]));

    QFont groupFont = font();
    groupFont.setBold(true);

    // Başlık satırı oluşturur; başlık tüm satır genişliğine yayılır (bkz. ApplyGroupSpans).
    auto makeGroupRow = [this, &groupFont](const QString &title, const QString &detail, bool expanded) {
        QStandardItem *titleItem = MakeItem(QStringLiteral("%1   ·   %2").arg(title, detail));
        titleItem->setFont(groupFont);
        titleItem->setData(false, RoleIsEntry);
        QList<QStandardItem *> row;
        row << titleItem;
        for (int column = TreeColumnName; column < TreeColumnTotal; ++column) {
            row << MakeItem(QString());
        }
        m_groupItems.append(qMakePair(titleItem, expanded));
        return row;
    };

    // Bilgi bölümünü (anahtar, değer) alt satırları olarak ekler.
    auto appendInfoSection = [](QStandardItem *parent, const QString &name, const ObjectDictionary::InfoEntries &entries) {
        if (entries.isEmpty()) {
            return;
        }
        QStandardItem *sectionItem = MakeItem(name);
        sectionItem->setData(false, RoleIsEntry);
        QList<QStandardItem *> sectionRow;
        sectionRow << sectionItem << MakeItem(QString());
        parent->appendRow(sectionRow);
        for (const auto &pair : entries) {
            QStandardItem *keyItem = MakeItem(pair.first);
            keyItem->setData(false, RoleIsEntry);
            sectionItem->appendRow({keyItem, MakeItem(pair.second)});
        }
    };

    // --- Dosya ve cihaz bilgisi ---
    if (!m_dictionary.FileInfoEntries().isEmpty() || !m_dictionary.DeviceInfoEntries().isEmpty()
        || !m_dictionary.CommissioningEntries().isEmpty()) {
        QList<QStandardItem *> infoRow = makeGroupRow(tr("Dosya ve cihaz bilgisi"), QStringLiteral("[FileInfo] / [DeviceInfo]"), false);
        appendInfoSection(infoRow.first(), QStringLiteral("FileInfo"), m_dictionary.FileInfoEntries());
        appendInfoSection(infoRow.first(), QStringLiteral("DeviceInfo"), m_dictionary.DeviceInfoEntries());
        appendInfoSection(infoRow.first(), QStringLiteral("DeviceComissioning"), m_dictionary.CommissioningEntries());
        m_treeModel->appendRow(infoRow);
    }

    // Nesneleri başlıklara dağıt.
    QVector<QList<const OdObject *>> grouped(groupCount);
    const QMap<quint16, OdObject> &allObjects = m_dictionary.Objects();
    for (auto it = allObjects.constBegin(); it != allObjects.constEnd(); ++it) {
        int target = groupCount - 1;
        for (int g = 0; g < groupCount; ++g) {
            if (it.key() >= groups[g].first && it.key() <= groups[g].last) {
                target = g;
                break;
            }
        }
        grouped[target].append(&it.value());
    }

    // Tek bir girdi için satır hücrelerini oluşturur.
    auto makeEntryRow = [this](const OdEntry &entry, const QString &indexText, const QString &name) {
        QList<QStandardItem *> row;
        QStandardItem *indexItem = MakeItem(indexText);
        indexItem->setData(entry.index, RoleIndex);
        indexItem->setData(entry.subIndex, RoleSubIndex);
        indexItem->setData(true, RoleIsEntry);
        QStandardItem *valueItem = MakeItem(QString());
        row << indexItem << MakeItem(name) << MakeItem(DataTypeName(entry.dataType)) << MakeItem(entry.accessType)
            << MakeItem(entry.defaultValue) << MakeItem(entry.parameterValue)
            << MakeItem(entry.pdoMapping ? tr("Evet") : QString()) << valueItem;
        m_valueItems.insert(ValueKey(entry.index, entry.subIndex), valueItem);
        return row;
    };

    for (int g = 0; g < groupCount; ++g) {
        const QList<const OdObject *> &objects = grouped.at(g);
        if (objects.isEmpty()) {
            continue;
        }
        QList<QStandardItem *> groupRow =
            makeGroupRow(tr(groups[g].title),
                         tr("%1 · %2 nesne").arg(QString::fromUtf8(groups[g].range)).arg(objects.size()),
                         groups[g].expanded);
        QStandardItem *groupItem = groupRow.first();

        for (const OdObject *object : objects) {
            const QString indexText =
                QStringLiteral("0x%1").arg(QStringLiteral("%1").arg(object->index, 4, 16, QLatin1Char('0')).toUpper());

            const bool isComplex = object->objectType == CanOpenObjectType::Array
                                   || object->objectType == CanOpenObjectType::Record;
            if (!isComplex && object->entries.size() == 1) {
                groupItem->appendRow(makeEntryRow(object->entries.first(), indexText, object->name));
                continue;
            }

            QList<QStandardItem *> parentRow;
            QStandardItem *parentItem = MakeItem(indexText);
            parentItem->setData(object->index, RoleIndex);
            parentItem->setData(false, RoleIsEntry);
            parentRow << parentItem << MakeItem(object->name) << MakeItem(ObjectTypeName(object->objectType));
            for (int column = TreeColumnAccess; column < TreeColumnTotal; ++column) {
                parentRow << MakeItem(QString());
            }
            for (const OdEntry &entry : object->entries) {
                parentItem->appendRow(makeEntryRow(entry, AddressText(entry.index, entry.subIndex), entry.name));
            }
            groupItem->appendRow(parentRow);
        }
        m_treeModel->appendRow(groupRow);
    }

    ApplyThemeColors();
    m_searchEdit->clear();
    ApplyGroupSpans();
    m_treeView->collapseAll();
    RestoreDefaultExpansion();
}

void CanOpenPanel::HandleTreeSelection(const QModelIndex &current)
{
    const QModelIndex first = current.siblingAtColumn(TreeColumnIndex);
    if (!first.isValid() || !first.data(RoleIsEntry).toBool()) {
        return;
    }
    const quint16 index = static_cast<quint16>(first.data(RoleIndex).toUInt());
    const quint8 subIndex = static_cast<quint8>(first.data(RoleSubIndex).toUInt());
    m_indexEdit->setText(QStringLiteral("%1").arg(index, 4, 16, QLatin1Char('0')).toUpper());
    m_subIndexEdit->setText(QStringLiteral("%1").arg(static_cast<uint>(subIndex), 2, 16, QLatin1Char('0')).toUpper());

    const OdEntry *entry = m_dictionary.FindEntry(index, subIndex);
    if (entry != nullptr) {
        const int comboIndex = m_dataTypeCombo->findData(entry->dataType);
        m_dataTypeCombo->setCurrentIndex(comboIndex >= 0 ? comboIndex : 0);
    }
}

void CanOpenPanel::ReadSelectedObject()
{
    quint16 index = 0;
    quint8 subIndex = 0;
    if (!ReadSdoTarget(index, subIndex)) {
        return;
    }
    const quint8 nodeId = static_cast<quint8>(m_nodeIdSpin->value());
    QString errorText;
    BeginSdoTxReport(tr("SDO oku %1 → Node %2").arg(AddressText(index, subIndex)).arg(static_cast<uint>(nodeId)));
    if (!m_sdoClient->StartUpload(nodeId, index, subIndex, errorText)) {
        EndSdoTxReport();
        AppendLog(tr("SDO okuma başlatılamadı: %1").arg(errorText));
        return;
    }
    AppendLog(tr("Node %1: %2 okunuyor...").arg(static_cast<uint>(nodeId)).arg(AddressText(index, subIndex)));
    UpdateButtonStates();
}

void CanOpenPanel::WriteSelectedObject()
{
    if (!m_connected) {
        return;
    }
    quint16 index = 0;
    quint8 subIndex = 0;
    if (!ReadSdoTarget(index, subIndex)) {
        return;
    }

    const quint16 dataType = static_cast<quint16>(m_dataTypeCombo->currentData().toUInt());
    QString errorText;
    const std::optional<QByteArray> data = EncodeValue(dataType, m_valueEdit->text(), errorText);
    if (!data) {
        AppendLog(tr("Değer geçersiz: %1").arg(errorText));
        return;
    }

    const OdEntry *entry = m_dictionary.FindEntry(index, subIndex);
    if (entry != nullptr && (entry->accessType == QLatin1String("ro") || entry->accessType == QLatin1String("const"))) {
        AppendLog(tr("Uyarı: EDS'e göre %1 salt okunur (%2); yine de gönderiliyor.")
                      .arg(AddressText(index, subIndex), entry->accessType));
    }

    const quint8 nodeId = static_cast<quint8>(m_nodeIdSpin->value());
    BeginSdoTxReport(tr("SDO yaz %1 = %2 → Node %3")
                         .arg(AddressText(index, subIndex), m_valueEdit->text().trimmed())
                         .arg(static_cast<uint>(nodeId)));
    if (!m_sdoClient->StartDownload(nodeId, index, subIndex, *data, errorText)) {
        EndSdoTxReport();
        AppendLog(tr("SDO yazma başlatılamadı: %1").arg(errorText));
        return;
    }
    AppendLog(tr("Node %1: %2 ← %3 yazılıyor...")
                  .arg(static_cast<uint>(nodeId))
                  .arg(AddressText(index, subIndex), m_valueEdit->text()));
    UpdateButtonStates();
}

void CanOpenPanel::SendNmtCommand(CanOpen::NmtCommand command)
{
    if (!m_send) {
        return;
    }
    const quint8 target = static_cast<quint8>(m_nmtTargetSpin->value());
    const CanFrame frame = CanOpen::BuildNmtFrame(command, target);
    QString errorText;
    if (!m_send(frame, errorText)) {
        AppendLog(tr("NMT gönderilemedi: %1").arg(errorText));
        return;
    }
    const QString description =
        tr("NMT %1 → %2")
            .arg(CanOpen::NmtCommandName(static_cast<quint8>(command)),
                 target == 0 ? tr("tüm düğümler") : tr("Node %1").arg(static_cast<uint>(target)));
    AppendLog(description);
    emit ManualFrameSent(frame, description);
}

void CanOpenPanel::SendSaveCommand(quint16 index, quint8 subIndex, const QString &name)
{
    if (!m_connected || m_sdoClient->IsBusy() || IsBatchActive()) {
        return;
    }
    const quint8 nodeId = static_cast<quint8>(m_nodeIdSpin->value());
    QString errorText;
    // "save" imzası: 73 61 76 65 (UNSIGNED32 0x65766173, little-endian) → 23 <index> <sub> 73 61 76 65
    BeginSdoTxReport(tr("%1 → Node %2").arg(name).arg(static_cast<uint>(nodeId)));
    if (!m_sdoClient->StartDownload(nodeId, index, subIndex, QByteArrayLiteral("save"), errorText)) {
        EndSdoTxReport();
        AppendLog(tr("%1 gönderilemedi: %2").arg(name, errorText));
        return;
    }
    AppendLog(tr("Node %1: %2 — %3 ← \"save\" gönderildi (COB-ID %4).")
                  .arg(static_cast<uint>(nodeId))
                  .arg(name, AddressText(index, subIndex))
                  .arg(QString::number(CanOpen::SdoRequestBase + nodeId, 16).toUpper()));
    UpdateButtonStates();
}

void CanOpenPanel::BeginSdoTxReport(const QString &description)
{
    m_sdoTxDescription = description;
    m_sdoTxFrameCount = 0;
}

void CanOpenPanel::EndSdoTxReport()
{
    m_sdoTxDescription.clear();
    m_sdoTxFrameCount = 0;
}

void CanOpenPanel::SendSync()
{
    if (!m_send) {
        return;
    }
    const CanFrame frame = CanOpen::BuildSyncFrame();
    QString errorText;
    if (!m_send(frame, errorText)) {
        AppendLog(tr("SYNC gönderilemedi: %1").arg(errorText));
        return;
    }
    AppendLog(QStringLiteral("SYNC"));
    emit ManualFrameSent(frame, QStringLiteral("SYNC"));
}

bool CanOpenPanel::ReadSdoTarget(quint16 &index, quint8 &subIndex)
{
    quint32 indexValue = 0;
    quint32 subValue = 0;
    if (!ParseHexNumber(m_indexEdit->text(), indexValue) || indexValue > 0xFFFF) {
        AppendLog(tr("Index geçerli bir hex değer olmalıdır (0000..FFFF)."));
        return false;
    }
    const QString subText = m_subIndexEdit->text().trimmed();
    if (subText.isEmpty()) {
        subValue = 0;
    } else if (!ParseHexNumber(subText, subValue) || subValue > 0xFF) {
        AppendLog(tr("Subindex geçerli bir hex değer olmalıdır (00..FF)."));
        return false;
    }
    index = static_cast<quint16>(indexValue);
    subIndex = static_cast<quint8>(subValue);
    return true;
}

void CanOpenPanel::HandleUploadFinished(quint8 nodeId, quint16 index, quint8 subIndex, const QByteArray &data)
{
    EndSdoTxReport();
    if (IsBatchActive()) {
        const BatchItem &item = m_batch.at(m_batchPosition);
        const QString text = DcfWriter::FormatParameterValue(item.dataType, data);
        if (!text.isEmpty()) {
            m_batchValues.insert(DcfWriter::ValueKey(index, subIndex), text);
        }
        if (QStandardItem *valueItem = m_valueItems.value(ValueKey(index, subIndex), nullptr)) {
            valueItem->setText(DecodeValue(item.dataType, data));
        }
        AdvanceBatch(true, QString());
        return;
    }
    const OdEntry *entry = m_dictionary.FindEntry(index, subIndex);
    const quint16 dataType = entry != nullptr && entry->dataType != 0
                                 ? entry->dataType
                                 : static_cast<quint16>(m_dataTypeCombo->currentData().toUInt());
    const QString valueText = DecodeValue(dataType, data);

    AppendLog(tr("Node %1: %2 = %3 [%4 bayt]")
                  .arg(static_cast<uint>(nodeId))
                  .arg(AddressText(index, subIndex), valueText)
                  .arg(data.size()));

    QStandardItem *valueItem = m_valueItems.value(ValueKey(index, subIndex), nullptr);
    if (valueItem != nullptr) {
        valueItem->setText(valueText);
    }

    // Yazmaya hazır olması için değeri düz biçimde alana koy.
    m_valueEdit->setText(dataType == 0 || dataType == CanOpenDataType::OctetString || dataType == CanOpenDataType::Domain
                             ? DecodeValue(0, data)
                             : DecodeValue(dataType, data, false).remove(QLatin1Char('"')));
    UpdateButtonStates();
}

void CanOpenPanel::HandleDownloadFinished(quint8 nodeId, quint16 index, quint8 subIndex)
{
    EndSdoTxReport();
    if (IsBatchActive()) {
        AdvanceBatch(true, QString());
        return;
    }
    AppendLog(tr("Node %1: %2 yazma başarılı.").arg(static_cast<uint>(nodeId)).arg(AddressText(index, subIndex)));
    UpdateButtonStates();
}

void CanOpenPanel::HandleTransferFailed(quint8 nodeId, quint16 index, quint8 subIndex, const QString &reason)
{
    EndSdoTxReport();
    if (IsBatchActive()) {
        AdvanceBatch(false, reason);
        return;
    }
    AppendLog(tr("Node %1: %2 SDO hatası — %3").arg(static_cast<uint>(nodeId)).arg(AddressText(index, subIndex), reason));
    UpdateButtonStates();
}

void CanOpenPanel::RebuildPdoDecoder()
{
    if (m_dictionary.IsEmpty()) {
        m_pdoDecoder.Clear();
        return;
    }
    m_pdoDecoder.BuildFromDictionary(m_dictionary, static_cast<quint8>(m_nodeIdSpin->value()));
}

void CanOpenPanel::AppendLog(const QString &text)
{
    m_log->appendPlainText(QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), text));
}

void CanOpenPanel::UpdateButtonStates()
{
    const bool idle = !m_sdoClient->IsBusy() && !IsBatchActive();
    m_readButton->setEnabled(m_connected && idle);
    m_writeButton->setEnabled(m_connected && idle);
    m_saveVendorButton->setEnabled(m_connected && idle);
    m_saveParameterButton->setEnabled(m_connected && idle);
    m_uploadSetButton->setEnabled(m_connected && idle && !m_dictionary.IsEmpty());
    m_downloadSetButton->setEnabled(m_connected && idle);
    for (QPushButton *button : std::as_const(m_busButtons)) {
        button->setEnabled(m_connected);
    }
}

quint32 CanOpenPanel::ValueKey(quint16 index, quint8 subIndex)
{
    return (static_cast<quint32>(index) << 8) | subIndex;
}

QString CanOpenPanel::AddressText(quint16 index, quint8 subIndex)
{
    return QStringLiteral("0x%1:%2")
        .arg(QStringLiteral("%1").arg(index, 4, 16, QLatin1Char('0')).toUpper())
        .arg(QStringLiteral("%1").arg(static_cast<uint>(subIndex), 2, 16, QLatin1Char('0')).toUpper());
}

bool CanOpenPanel::IsReadableAccess(const QString &accessType)
{
    return accessType == QLatin1String("ro") || accessType == QLatin1String("rw") || accessType == QLatin1String("rwr")
           || accessType == QLatin1String("rww") || accessType == QLatin1String("const");
}

bool CanOpenPanel::IsWritableAccess(const QString &accessType)
{
    return accessType == QLatin1String("rw") || accessType == QLatin1String("rwr") || accessType == QLatin1String("rww")
           || accessType == QLatin1String("wo");
}

bool CanOpenPanel::IsBatchActive() const
{
    return m_batchPosition >= 0;
}

void CanOpenPanel::UploadParameterSet()
{
    if (!m_connected || m_dictionary.IsEmpty() || IsBatchActive()) {
        return;
    }

    const QVector<BatchItem> plan = BuildUploadPlan(m_includeReadOnlyCheck->isChecked());
    if (plan.isEmpty()) {
        QMessageBox::information(this, tr("Parametre seti"), tr("EDS'te okunabilecek uygun parametre bulunamadı."));
        return;
    }

    QSettings settings;
    const QString directory = settings.value(QStringLiteral("canopen/lastDcfDir")).toString();
    QString product = m_dictionary.DeviceInfoValue(QStringLiteral("productname")).simplified();
    product.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]+")), QStringLiteral("_"));
    const QString suggestion = QDir(directory).filePath(
        QStringLiteral("%1_node%2.dcf").arg(product.isEmpty() ? QStringLiteral("cihaz") : product).arg(m_nodeIdSpin->value()));
    QString targetPath = QFileDialog::getSaveFileName(this, tr("Parametre setini DCF olarak kaydet"), suggestion,
                                                      tr("DCF dosyaları (*.dcf)"));
    if (targetPath.isEmpty()) {
        return;
    }
    if (QFileInfo(targetPath).suffix().isEmpty()) {
        targetPath += QStringLiteral(".dcf");
    }
    m_batchTargetPath = targetPath;
    AppendLog(tr("Parametre seti okunuyor: %1 nesne, Node %2.").arg(plan.size()).arg(m_nodeIdSpin->value()));
    StartBatch(plan, true);
}

void CanOpenPanel::DownloadParameterSet()
{
    if (!m_connected || IsBatchActive()) {
        return;
    }
    QSettings settings;
    const QString lastFile = settings.value(QStringLiteral("canopen/lastDcfFile")).toString();
    const QString filePath = QFileDialog::getOpenFileName(this, tr("Cihaza yazılacak DCF dosyası"), lastFile,
                                                          tr("DCF dosyaları (*.dcf *.DCF);;Tüm dosyalar (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    QString errorText;
    QStringList warnings;
    const std::optional<ObjectDictionary> dcf = EdsParser::OpenEdsFile(filePath, errorText, warnings);
    if (!dcf) {
        QMessageBox::warning(this, tr("Parametre seti"), errorText);
        return;
    }
    if (!dcf->HasParameterValues()) {
        QMessageBox::warning(this, tr("Parametre seti"),
                             tr("%1 içinde ParameterValue bulunmuyor; dosya bir DCF değil (EDS olabilir).")
                                 .arg(QFileInfo(filePath).fileName()));
        return;
    }

    const quint8 nodeId = static_cast<quint8>(m_nodeIdSpin->value());
    QVector<BatchItem> plan;
    int skipped = 0;
    BuildDownloadPlan(*dcf, nodeId, plan, skipped);
    if (plan.isEmpty()) {
        QMessageBox::warning(this, tr("Parametre seti"), tr("DCF'te cihaza yazılabilecek (rw) değer bulunamadı."));
        return;
    }

    QString question = tr("%1 dosyasındaki %2 değer Node %3'e yazılacak.")
                           .arg(QFileInfo(filePath).fileName())
                           .arg(plan.size())
                           .arg(nodeId);
    const auto storedNode = ParseEdsNumber(dcf->CommissioningValue(QStringLiteral("NodeID")));
    if (storedNode && *storedNode != nodeId) {
        question += QStringLiteral("\n\n") + tr("Dikkat: DCF Node %1 için kaydedilmiş.").arg(*storedNode);
    }
    if (skipped > 0) {
        question += QStringLiteral("\n") + tr("%1 değer desteklenmeyen tip veya geçersiz biçim nedeniyle atlanacak.").arg(skipped);
    }
    question += QStringLiteral("\n\n") + tr("Cihazın Pre-operational durumunda olması önerilir. Devam edilsin mi?");
    if (QMessageBox::question(this, tr("Parametre setini yaz"), question) != QMessageBox::Yes) {
        return;
    }

    settings.setValue(QStringLiteral("canopen/lastDcfFile"), filePath);
    AppendLog(tr("Parametre seti yazılıyor: %1 → Node %2 (%3 adım).")
                  .arg(QFileInfo(filePath).fileName())
                  .arg(nodeId)
                  .arg(plan.size()));
    StartBatch(plan, false);
}

QVector<CanOpenPanel::BatchItem> CanOpenPanel::BuildUploadPlan(bool includeReadOnly) const
{
    // Okunacak girdiler: okunabilir, DCF'e yazılabilir tipte; varsayılan olarak yalnızca yazılabilir ayarlar.
    QVector<BatchItem> plan;
    const QMap<quint16, OdObject> &objects = m_dictionary.Objects();
    for (const OdObject &object : objects) {
        for (const OdEntry &entry : object.entries) {
            const bool wanted = includeReadOnly ? IsReadableAccess(entry.accessType)
                                                : IsReadableAccess(entry.accessType) && IsWritableAccess(entry.accessType);
            if (!wanted || !DcfWriter::IsSupportedType(entry.dataType)) {
                continue;
            }
            BatchItem item;
            item.index = entry.index;
            item.subIndex = entry.subIndex;
            item.dataType = entry.dataType;
            plan.append(item);
        }
    }
    return plan;
}

void CanOpenPanel::BuildDownloadPlan(const ObjectDictionary &source, quint8 nodeId, QVector<BatchItem> &plan, int &skipped)
{
    plan.clear();
    skipped = 0;

    // Yazılabilir ve kodlanabilir tüm değerleri topla.
    QMap<quint32, BatchItem> items;
    const QMap<quint16, OdObject> &objects = source.Objects();
    for (const OdObject &object : objects) {
        for (const OdEntry &entry : object.entries) {
            if (entry.parameterValue.isEmpty() || !IsWritableAccess(entry.accessType)) {
                continue;
            }
            QString text = entry.parameterValue.trimmed();
            if (IsSignedIntegerType(entry.dataType) || IsUnsignedIntegerType(entry.dataType)
                || entry.dataType == CanOpenDataType::Boolean) {
                bool ok = false;
                const qlonglong negative = text.toLongLong(&ok, 10);
                if (ok && negative < 0) {
                    text = QString::number(negative);
                } else if (const auto value = ParseEdsNumber(text, nodeId)) {
                    text = QString::number(*value);
                } else {
                    ++skipped;
                    continue;
                }
            }
            QString errorText;
            const std::optional<QByteArray> data = EncodeValue(entry.dataType, text, errorText);
            if (!data || !DcfWriter::IsSupportedType(entry.dataType)) {
                ++skipped;
                continue;
            }
            BatchItem item;
            item.index = entry.index;
            item.subIndex = entry.subIndex;
            item.dataType = entry.dataType;
            item.write = true;
            item.data = *data;
            items.insert(DcfWriter::ValueKey(entry.index, entry.subIndex), item);
        }
    }

    const auto isCommunication = [](quint16 index) {
        return (index >= 0x1400 && index <= 0x15FF) || (index >= 0x1800 && index <= 0x19FF);
    };
    const auto isMapping = [](quint16 index) {
        return (index >= 0x1600 && index <= 0x17FF) || (index >= 0x1A00 && index <= 0x1BFF);
    };

    // 1) PDO'ları devre dışı bırak (COB-ID bit 31 = 1).
    QVector<BatchItem> finalCobIds;
    for (auto it = items.constBegin(); it != items.constEnd(); ++it) {
        const BatchItem &item = it.value();
        if (isCommunication(item.index) && item.subIndex == 1 && item.data.size() == 4) {
            BatchItem disable = item;
            disable.data[3] = static_cast<char>(static_cast<quint8>(disable.data.at(3)) | 0x80);
            disable.note = tr("PDO devre dışı");
            plan.append(disable);
            finalCobIds.append(item);
        }
    }

    // 2) PDO eşlemeleri: sub0 = 0, girdiler, sub0 = n.
    QSet<quint16> mappingIndexes;
    for (auto it = items.constBegin(); it != items.constEnd(); ++it) {
        if (isMapping(it.value().index)) {
            mappingIndexes.insert(it.value().index);
        }
    }
    QList<quint16> sortedMappings(mappingIndexes.begin(), mappingIndexes.end());
    std::sort(sortedMappings.begin(), sortedMappings.end());
    for (const quint16 index : std::as_const(sortedMappings)) {
        const quint32 countKey = DcfWriter::ValueKey(index, 0);
        const bool hasCount = items.contains(countKey);
        if (hasCount) {
            BatchItem clear = items.value(countKey);
            clear.data = QByteArray(clear.data.size(), '\0');
            clear.note = tr("eşleme temizleniyor");
            plan.append(clear);
        }
        for (auto it = items.constBegin(); it != items.constEnd(); ++it) {
            if (it.value().index == index && it.value().subIndex != 0) {
                plan.append(it.value());
            }
        }
        if (hasCount) {
            plan.append(items.value(countKey));
        }
    }

    // 3) Diğer tüm parametreler (index sırasıyla).
    for (auto it = items.constBegin(); it != items.constEnd(); ++it) {
        const BatchItem &item = it.value();
        if (isMapping(item.index) || (isCommunication(item.index) && item.subIndex == 1)) {
            continue;
        }
        plan.append(item);
    }

    // 4) PDO COB-ID'lerini son değerleriyle yaz (DCF'teki geçerlilik biti korunur).
    for (BatchItem item : std::as_const(finalCobIds)) {
        item.note = tr("PDO COB-ID son değer");
        plan.append(item);
    }

    // 5) İsteğe bağlı: kalıcı kaydet.
    if (m_storeCheck->isChecked()) {
        BatchItem store;
        store.index = 0x1010;
        store.subIndex = 1;
        store.dataType = CanOpenDataType::Unsigned32;
        store.write = true;
        store.data = QByteArrayLiteral("save"); // 0x65766173, little-endian
        store.note = tr("kalıcı kaydet");
        plan.append(store);
    }
}

void CanOpenPanel::StartBatch(const QVector<BatchItem> &plan, bool upload)
{
    m_batch = plan;
    m_batchUpload = upload;
    m_batchCancelled = false;
    m_batchFailures = 0;
    m_batchConsecutiveTimeouts = 0;
    m_batchValues.clear();
    m_batchPosition = 0;
    m_batchProgress->setRange(0, static_cast<int>(m_batch.size()));
    m_batchProgress->setValue(0);
    m_batchProgress->setFormat(QStringLiteral("%v / %m"));
    m_batchProgress->setVisible(true);
    m_cancelBatchButton->setVisible(true);
    m_batchStatusLabel->setText(upload ? tr("Okunuyor...") : tr("Yazılıyor..."));
    UpdateButtonStates();
    QTimer::singleShot(0, this, &CanOpenPanel::StartNextBatchItem);
}

void CanOpenPanel::StartNextBatchItem()
{
    if (!IsBatchActive()) {
        return;
    }
    if (m_batchPosition >= m_batch.size()) {
        FinishBatch(false, QString());
        return;
    }
    m_batchProgress->setValue(m_batchPosition);

    const BatchItem &item = m_batch.at(m_batchPosition);
    const quint8 nodeId = static_cast<quint8>(m_nodeIdSpin->value());
    QString errorText;
    const bool started = item.write ? m_sdoClient->StartDownload(nodeId, item.index, item.subIndex, item.data, errorText)
                                    : m_sdoClient->StartUpload(nodeId, item.index, item.subIndex, errorText);
    if (!started) {
        AdvanceBatch(false, errorText);
    }
}

void CanOpenPanel::AdvanceBatch(bool success, const QString &reason)
{
    if (!IsBatchActive()) {
        return;
    }
    const BatchItem &item = m_batch.at(m_batchPosition);
    if (m_batchCancelled || !m_connected) {
        FinishBatch(true, m_batchCancelled ? tr("kullanıcı iptal etti") : tr("bağlantı kesildi"));
        return;
    }

    if (success) {
        m_batchConsecutiveTimeouts = 0;
        if (item.write) {
            AppendLog(tr("✓ %1%2").arg(AddressText(item.index, item.subIndex),
                                       item.note.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(item.note)));
        }
    } else {
        ++m_batchFailures;
        AppendLog(tr("✗ %1%2 — %3")
                      .arg(AddressText(item.index, item.subIndex),
                           item.note.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(item.note), reason));
        // Art arda zaman aşımı: düğüm yanıt vermiyor, kalan adımları denemenin anlamı yok.
        if (reason == CanOpen::SdoAbortCodeText(0x05040000)) {
            if (++m_batchConsecutiveTimeouts >= 3) {
                FinishBatch(true, tr("düğüm yanıt vermiyor (art arda 3 zaman aşımı)"));
                return;
            }
        } else {
            m_batchConsecutiveTimeouts = 0;
        }
    }
    ++m_batchPosition;
    QTimer::singleShot(0, this, &CanOpenPanel::StartNextBatchItem);
}

void CanOpenPanel::FinishBatch(bool aborted, const QString &reason)
{
    const int total = static_cast<int>(m_batch.size());
    const int done = qMin(m_batchPosition, total);
    const bool upload = m_batchUpload;
    m_batchPosition = -1;
    m_batch.clear();
    m_batchProgress->setVisible(false);
    m_cancelBatchButton->setVisible(false);

    QString summary;
    if (aborted) {
        summary = tr("Yarıda kaldı (%1): %2 / %3 adım, %4 hata.").arg(reason).arg(done).arg(total).arg(m_batchFailures);
        if (upload) {
            summary += QLatin1Char(' ') + tr("DCF dosyası yazılmadı.");
        }
    } else if (upload) {
        QString errorText;
        if (m_batchValues.isEmpty()) {
            summary = tr("Hiçbir değer okunamadı; DCF dosyası yazılmadı.");
        } else if (DcfWriter::WriteDcf(m_dictionary.FilePath(), m_batchTargetPath, m_batchValues,
                                       static_cast<quint8>(m_nodeIdSpin->value()), errorText)) {
            summary = tr("%1 değer okundu, %2 hata. Kaydedildi: %3")
                          .arg(m_batchValues.size())
                          .arg(m_batchFailures)
                          .arg(QDir::toNativeSeparators(m_batchTargetPath));
            QSettings settings;
            settings.setValue(QStringLiteral("canopen/lastDcfDir"), QFileInfo(m_batchTargetPath).absolutePath());
            settings.setValue(QStringLiteral("canopen/lastDcfFile"), m_batchTargetPath);
        } else {
            summary = tr("DCF yazılamadı: %1").arg(errorText);
        }
    } else {
        summary = tr("Yazma tamamlandı: %1 adım, %2 hata.").arg(total).arg(m_batchFailures);
    }
    AppendLog(tr("Parametre seti: %1").arg(summary));
    m_batchStatusLabel->setText(summary);
    UpdateButtonStates();
}

void CanOpenPanel::CancelBatch()
{
    if (!IsBatchActive()) {
        return;
    }
    m_batchCancelled = true;
    if (m_sdoClient->IsBusy()) {
        m_sdoClient->Cancel(); // TransferFailed → AdvanceBatch → FinishBatch
    } else {
        FinishBatch(true, tr("kullanıcı iptal etti"));
    }
}