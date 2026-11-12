#include "DataEditDialog.h"

#include "../can/CanFrame.h"
#include "InputHelpers.h"
#include "Theme.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int BytesPerRow = 8;
constexpr int AsciiColumn = BytesPerRow;

/// Hex hücreleri için en fazla iki onaltılık hane kabul eden düzenleyici.
class HexByteDelegate : public QStyledItemDelegate
{
public:
    /// Temsilciyi oluşturur.
    explicit HexByteDelegate(QObject *parent)
        : QStyledItemDelegate(parent)
    {
    }

    /// İki haneli hex girişi kabul eden satır düzenleyicisi oluşturur.
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(option)
        Q_UNUSED(index)
        auto *editor = new QLineEdit(parent);
        editor->setMaxLength(2);
        editor->setAlignment(Qt::AlignCenter);
        editor->setValidator(new UpperCaseValidator(QRegularExpression(QStringLiteral("[0-9A-F]{0,2}")), editor));
        return editor;
    }

    /// Düzenleyicideki metni büyük harfli, iki haneli hex olarak modele yazar.
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        auto *lineEdit = static_cast<QLineEdit *>(editor);
        bool ok = false;
        const uint value = lineEdit->text().toUInt(&ok, 16);
        model->setData(index, QStringLiteral("%1").arg(ok ? value : 0u, 2, 16, QLatin1Char('0')).toUpper());
    }
};

} // namespace

DataEditDialog::DataEditDialog(const QByteArray &data, QWidget *parent)
    : QDialog(parent)
    , m_data(data)
{
    setWindowTitle(tr("Veriyi Düzenle"));
    BuildUi();
    FillTable();
}

QByteArray DataEditDialog::Data() const
{
    return m_data;
}

void DataEditDialog::BuildUi()
{
    auto *openButton = new QToolButton(this);
    openButton->setIcon(Theme::Icon(QStringLiteral("open")));
    openButton->setToolTip(tr("Dosyadan aç (.bin ikili / .txt hex)"));
    auto *saveButton = new QToolButton(this);
    saveButton->setIcon(Theme::Icon(QStringLiteral("save")));
    saveButton->setToolTip(tr("Dosyaya kaydet (.bin ikili / .txt hex)"));
    auto *clearButton = new QToolButton(this);
    clearButton->setIcon(Theme::Icon(QStringLiteral("clear"), Theme::Colors().danger));
    clearButton->setToolTip(tr("Tüm baytları sıfırla"));
    connect(openButton, &QToolButton::clicked, this, &DataEditDialog::OpenDataFile);
    connect(saveButton, &QToolButton::clicked, this, &DataEditDialog::SaveDataFile);
    connect(clearButton, &QToolButton::clicked, this, &DataEditDialog::ClearData);

    auto *toolRow = new QHBoxLayout;
    toolRow->setSpacing(2);
    toolRow->addWidget(openButton);
    toolRow->addWidget(saveButton);
    toolRow->addSpacing(8);
    toolRow->addWidget(clearButton);
    toolRow->addStretch(1);

    m_table = new QTableWidget(this);
    m_table->setFont(Theme::MonospaceFont());
    m_table->setColumnCount(BytesPerRow + 1);
    QStringList headers;
    for (int column = 0; column < BytesPerRow; ++column) {
        headers << QStringLiteral("%1").arg(column, 2, 16, QLatin1Char('0')).toUpper();
    }
    headers << tr("ASCII");
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setFont(Theme::MonospaceFont());
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->verticalHeader()->setFont(Theme::MonospaceFont());
    m_table->verticalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_table->verticalHeader()->setHighlightSections(false);
    m_table->verticalHeader()->setDefaultSectionSize(m_table->fontMetrics().height() + 10);
    for (int column = 0; column < BytesPerRow; ++column) {
        m_table->setColumnWidth(column, 38);
    }
    m_table->horizontalHeader()->setSectionResizeMode(AsciiColumn, QHeaderView::Stretch);
    m_table->setShowGrid(false);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
                             | QAbstractItemView::AnyKeyPressed);
    m_table->setItemDelegate(new HexByteDelegate(m_table));
    connect(m_table, &QTableWidget::itemChanged, this, &DataEditDialog::HandleItemChanged);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("MutedText"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (QPushButton *okButton = buttons->button(QDialogButtonBox::Ok)) {
        okButton->setProperty("primary", true);
    }
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("İptal"));
    connect(buttons, &QDialogButtonBox::accepted, this, &DataEditDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &DataEditDialog::reject);

    auto *bottomRow = new QHBoxLayout;
    bottomRow->addWidget(m_statusLabel, 1);
    bottomRow->addWidget(buttons);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);
    layout->addLayout(toolRow);
    layout->addWidget(m_table, 1);
    layout->addLayout(bottomRow);

    resize(560, 360);
}

void DataEditDialog::FillTable()
{
    m_updating = true;
    const int length = static_cast<int>(m_data.size());
    const int rows = qMax(1, (length + BytesPerRow - 1) / BytesPerRow);
    m_table->setRowCount(rows);
    QStringList offsets;
    for (int row = 0; row < rows; ++row) {
        offsets << QString::number(row * BytesPerRow);
        for (int column = 0; column < BytesPerRow; ++column) {
            const int position = row * BytesPerRow + column;
            auto *item = new QTableWidgetItem;
            item->setTextAlignment(Qt::AlignCenter);
            if (position < length) {
                item->setText(QStringLiteral("%1")
                                  .arg(static_cast<uint>(static_cast<quint8>(m_data.at(position))), 2, 16, QLatin1Char('0'))
                                  .toUpper());
                item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
            } else {
                item->setFlags(Qt::NoItemFlags);
            }
            m_table->setItem(row, column, item);
        }
        auto *ascii = new QTableWidgetItem;
        ascii->setFlags(Qt::ItemIsEnabled);
        m_table->setItem(row, AsciiColumn, ascii);
        UpdateAsciiCell(row);
    }
    m_table->setVerticalHeaderLabels(offsets);
    m_statusLabel->setText(tr("%1 bayt").arg(length));
    m_updating = false;
}

void DataEditDialog::HandleItemChanged(QTableWidgetItem *item)
{
    if (m_updating || item == nullptr || item->column() >= BytesPerRow) {
        return;
    }
    const int position = item->row() * BytesPerRow + item->column();
    if (position >= m_data.size()) {
        return;
    }
    bool ok = false;
    const uint value = item->text().toUInt(&ok, 16);
    m_data[position] = static_cast<char>(ok ? value : 0u);
    m_updating = true;
    UpdateAsciiCell(item->row());
    m_updating = false;
}

void DataEditDialog::UpdateAsciiCell(int row)
{
    QString text;
    for (int column = 0; column < BytesPerRow; ++column) {
        const int position = row * BytesPerRow + column;
        if (position >= m_data.size()) {
            break;
        }
        const quint8 byte = static_cast<quint8>(m_data.at(position));
        text.append(byte >= 0x20 && byte < 0x7F ? QChar(byte) : QLatin1Char('.'));
    }
    if (QTableWidgetItem *item = m_table->item(row, AsciiColumn)) {
        item->setText(text);
    }
}

void DataEditDialog::OpenDataFile()
{
    const QString filePath = QFileDialog::getOpenFileName(this, tr("Veri dosyası aç"), QString(),
                                                          tr("Veri dosyaları (*.bin *.txt);;Tüm dosyalar (*)"));
    if (filePath.isEmpty()) {
        return;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_statusLabel->setText(tr("Dosya açılamadı: %1").arg(file.errorString()));
        return;
    }
    QByteArray bytes = file.readAll();
    if (QFileInfo(filePath).suffix().compare(QLatin1String("txt"), Qt::CaseInsensitive) == 0) {
        QByteArray parsed;
        if (!ParseHexBytes(QString::fromLatin1(bytes).simplified(), parsed)) {
            m_statusLabel->setText(tr("Dosya geçerli hex metin içermiyor."));
            return;
        }
        bytes = parsed;
    }

    const int length = static_cast<int>(m_data.size());
    m_data.fill('\0');
    for (int i = 0; i < qMin(length, static_cast<int>(bytes.size())); ++i) {
        m_data[i] = bytes.at(i);
    }
    FillTable();
    if (bytes.size() > length) {
        m_statusLabel->setText(tr("%1 bayt · dosyadaki %2 bayttan yalnızca ilk %1 bayt alındı").arg(length).arg(bytes.size()));
    }
}

void DataEditDialog::SaveDataFile()
{
    const QString filePath = QFileDialog::getSaveFileName(this, tr("Veriyi kaydet"), QStringLiteral("data.bin"),
                                                          tr("İkili dosya (*.bin);;Hex metin (*.txt)"));
    if (filePath.isEmpty()) {
        return;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_statusLabel->setText(tr("Dosya yazılamadı: %1").arg(file.errorString()));
        return;
    }
    if (QFileInfo(filePath).suffix().compare(QLatin1String("txt"), Qt::CaseInsensitive) == 0) {
        file.write(m_data.toHex(' ').toUpper());
        file.write("\n");
    } else {
        file.write(m_data);
    }
    m_statusLabel->setText(tr("%1 bayt kaydedildi").arg(m_data.size()));
}

void DataEditDialog::ClearData()
{
    m_data.fill('\0');
    FillTable();
}
