#include "CanOpenPanel.h"

#include "../canopen/SdoClient.h"
#include "../eds/EdsParser.h"

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
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

#include <utility>

namespace {

enum TreeColumn
{
    TreeColumnIndex,
    TreeColumnName,
    TreeColumnType,
    TreeColumnAccess,
    TreeColumnDefault,
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
        return m_send(frame, errorText);
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
    const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    // --- Sol: EDS ağacı ---
    auto *openButton = new QPushButton(tr("EDS Aç..."), this);
    connect(openButton, &QPushButton::clicked, this, &CanOpenPanel::BrowseEdsFile);
    m_fileLabel = new QLabel(tr("EDS dosyası yüklenmedi."), this);
    m_fileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *fileRow = new QHBoxLayout;
    fileRow->addWidget(openButton);
    fileRow->addWidget(m_fileLabel, 1);

    m_treeModel = new QStandardItemModel(0, TreeColumnTotal, this);
    m_treeModel->setHorizontalHeaderLabels(
        {tr("Index"), tr("Ad"), tr("Veri tipi"), tr("Erişim"), tr("Varsayılan"), tr("PDO"), tr("Okunan değer")});

    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_treeModel);
    m_treeView->setUniformRowHeights(true);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_treeView->setColumnWidth(TreeColumnIndex, 110);
    m_treeView->setColumnWidth(TreeColumnName, 240);
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
    leftLayout->addLayout(fileRow);
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

    m_readButton = new QPushButton(tr("Oku (Upload)"), this);
    m_writeButton = new QPushButton(tr("Yaz (Download)"), this);
    connect(m_readButton, &QPushButton::clicked, this, &CanOpenPanel::ReadSelectedObject);
    connect(m_writeButton, &QPushButton::clicked, this, &CanOpenPanel::WriteSelectedObject);
    connect(m_valueEdit, &QLineEdit::returnPressed, this, &CanOpenPanel::WriteSelectedObject);

    auto *addressRow = new QHBoxLayout;
    addressRow->addWidget(m_indexEdit, 2);
    addressRow->addWidget(new QLabel(QStringLiteral(":"), this));
    addressRow->addWidget(m_subIndexEdit, 1);

    auto *sdoButtons = new QHBoxLayout;
    sdoButtons->addWidget(m_readButton);
    sdoButtons->addWidget(m_writeButton);

    auto *sdoGroup = new QGroupBox(tr("SDO"), this);
    auto *sdoLayout = new QFormLayout(sdoGroup);
    sdoLayout->addRow(tr("Index:Sub (hex):"), addressRow);
    sdoLayout->addRow(tr("Veri tipi:"), m_dataTypeCombo);
    sdoLayout->addRow(tr("Değer:"), m_valueEdit);
    sdoLayout->addRow(sdoButtons);

    m_nmtTargetSpin = new QSpinBox(this);
    m_nmtTargetSpin->setRange(0, CanOpen::MaxNodeId);
    m_nmtTargetSpin->setSpecialValueText(tr("0 (tüm düğümler)"));
    m_nmtTargetSpin->setValue(0);

    auto *nmtGroup = new QGroupBox(tr("NMT / SYNC"), this);
    auto *nmtLayout = new QGridLayout(nmtGroup);
    nmtLayout->addWidget(new QLabel(tr("Hedef:"), this), 0, 0);
    nmtLayout->addWidget(m_nmtTargetSpin, 0, 1, 1, 2);

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
    auto *clearLogButton = new QPushButton(tr("Günlüğü temizle"), this);
    connect(clearLogButton, &QPushButton::clicked, m_log, &QPlainTextEdit::clear);

    auto *rightWidget = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(nodeGroup);
    rightLayout->addWidget(sdoGroup);
    rightLayout->addWidget(nmtGroup);
    rightLayout->addWidget(new QLabel(tr("Günlük:"), this));
    rightLayout->addWidget(m_log, 1);
    rightLayout->addWidget(clearLogButton, 0, Qt::AlignRight);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    auto *layout = new QVBoxLayout(this);
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

    AppendLog(tr("EDS yüklendi: %1 — %2 nesne, %3 PDO tanımı.")
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
    const QString filePath = QFileDialog::getOpenFileName(this, tr("EDS Dosyası Aç"), QString(),
                                                          tr("EDS dosyaları (*.eds *.EDS);;Tüm dosyalar (*)"));
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

void CanOpenPanel::PopulateTree()
{
    m_treeModel->removeRows(0, m_treeModel->rowCount());
    m_valueItems.clear();

    // Tek bir girdi için satır hücrelerini oluşturur.
    auto makeEntryRow = [this](const OdEntry &entry, const QString &indexText, const QString &name) {
        QList<QStandardItem *> row;
        QStandardItem *indexItem = MakeItem(indexText);
        indexItem->setData(entry.index, RoleIndex);
        indexItem->setData(entry.subIndex, RoleSubIndex);
        indexItem->setData(true, RoleIsEntry);
        QStandardItem *valueItem = MakeItem(QString());
        row << indexItem << MakeItem(name) << MakeItem(DataTypeName(entry.dataType)) << MakeItem(entry.accessType)
            << MakeItem(entry.defaultValue) << MakeItem(entry.pdoMapping ? tr("Evet") : QString()) << valueItem;
        m_valueItems.insert(ValueKey(entry.index, entry.subIndex), valueItem);
        return row;
    };

    const QMap<quint16, OdObject> &objects = m_dictionary.Objects();
    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
        const OdObject &object = it.value();
        const QString indexText = QStringLiteral("0x%1").arg(QStringLiteral("%1").arg(object.index, 4, 16, QLatin1Char('0')).toUpper());

        const bool isComplex = object.objectType == CanOpenObjectType::Array
                               || object.objectType == CanOpenObjectType::Record;
        if (!isComplex && object.entries.size() == 1) {
            m_treeModel->appendRow(makeEntryRow(object.entries.first(), indexText, object.name));
            continue;
        }

        QList<QStandardItem *> parentRow;
        QStandardItem *parentItem = MakeItem(indexText);
        parentItem->setData(object.index, RoleIndex);
        parentItem->setData(false, RoleIsEntry);
        parentRow << parentItem << MakeItem(object.name) << MakeItem(ObjectTypeName(object.objectType));
        for (int column = TreeColumnAccess; column < TreeColumnTotal; ++column) {
            parentRow << MakeItem(QString());
        }

        for (const OdEntry &entry : object.entries) {
            parentItem->appendRow(makeEntryRow(entry, AddressText(entry.index, entry.subIndex), entry.name));
        }
        m_treeModel->appendRow(parentRow);
    }
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
    if (!m_sdoClient->StartUpload(nodeId, index, subIndex, errorText)) {
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
    if (!m_sdoClient->StartDownload(nodeId, index, subIndex, *data, errorText)) {
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
    QString errorText;
    if (!m_send(CanOpen::BuildNmtFrame(command, target), errorText)) {
        AppendLog(tr("NMT gönderilemedi: %1").arg(errorText));
        return;
    }
    AppendLog(tr("NMT %1 → %2")
                  .arg(CanOpen::NmtCommandName(static_cast<quint8>(command)),
                       target == 0 ? tr("tüm düğümler") : tr("Node %1").arg(static_cast<uint>(target))));
}

void CanOpenPanel::SendSync()
{
    if (!m_send) {
        return;
    }
    QString errorText;
    if (!m_send(CanOpen::BuildSyncFrame(), errorText)) {
        AppendLog(tr("SYNC gönderilemedi: %1").arg(errorText));
        return;
    }
    AppendLog(QStringLiteral("SYNC"));
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
    AppendLog(tr("Node %1: %2 yazma başarılı.").arg(static_cast<uint>(nodeId)).arg(AddressText(index, subIndex)));
    UpdateButtonStates();
}

void CanOpenPanel::HandleTransferFailed(quint8 nodeId, quint16 index, quint8 subIndex, const QString &reason)
{
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
    const bool idle = !m_sdoClient->IsBusy();
    m_readButton->setEnabled(m_connected && idle);
    m_writeButton->setEnabled(m_connected && idle);
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
