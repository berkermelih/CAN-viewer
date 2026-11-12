#include "TxMessageDialog.h"

#include "DataEditDialog.h"
#include "InputHelpers.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

// CAN FD için seçilebilecek uzunluklar.
const int FdLengths[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

} // namespace

TxMessageDialog::TxMessageDialog(QWidget *parent)
    : QDialog(parent)
    , m_data(MaxCanFdDataLength, '\0')
{
    setWindowTitle(tr("Gönderim Mesajını Düzenle"));
    BuildUi();
    FillLengthCombo(8);
    RefreshDataFields();
    UpdateFieldStates();
}

void TxMessageDialog::BuildUi()
{
    // --- Çerçeve biçimi ---
    m_ccRadio = new QRadioButton(tr("CAN CC"), this);
    m_fdRadio = new QRadioButton(tr("CAN FD"), this);
    m_xlRadio = new QRadioButton(tr("CAN XL"), this);
    m_ccRadio->setChecked(true);
    m_ccRadio->setToolTip(tr("Klasik CAN (CAN 2.0A/B), en fazla 8 bayt"));
    m_fdRadio->setToolTip(tr("CAN FD, en fazla 64 bayt. Kanal CAN FD modunda bağlı olmalıdır."));
    m_xlRadio->setEnabled(false);
    m_xlRadio->setToolTip(tr("CAN XL, PCAN-Basic sürücüsü tarafından desteklenmiyor."));
    auto *formatGroup = new QButtonGroup(this);
    formatGroup->addButton(m_ccRadio);
    formatGroup->addButton(m_fdRadio);
    formatGroup->addButton(m_xlRadio);
    connect(m_ccRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            FillLengthCombo(CurrentLength());
            UpdateFieldStates();
        }
    });
    connect(m_fdRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            FillLengthCombo(CurrentLength());
            UpdateFieldStates();
        }
    });

    m_formatHint = new QLabel(this);
    m_formatHint->setObjectName(QStringLiteral("MutedText"));

    auto *formatRow = new QHBoxLayout;
    formatRow->setSpacing(18);
    formatRow->addWidget(m_ccRadio);
    formatRow->addWidget(m_fdRadio);
    formatRow->addWidget(m_xlRadio);
    formatRow->addStretch(1);

    // --- ID, uzunluk, veri ---
    m_idEdit = new QLineEdit(this);
    m_idEdit->setFont(Theme::MonospaceFont());
    m_idEdit->setText(QStringLiteral("000"));
    m_idEdit->setMaxLength(8);
    m_idEdit->setFixedWidth(104);
    m_idEdit->setValidator(new UpperCaseValidator(QRegularExpression(QStringLiteral("[0-9A-F]{1,8}")), m_idEdit));
    connect(m_idEdit, &QLineEdit::textEdited, this, &TxMessageDialog::HandleIdEdited);

    m_lengthCombo = new QComboBox(this);
    m_lengthCombo->setFixedWidth(88);
    connect(m_lengthCombo, &QComboBox::currentIndexChanged, this, [this]() {
        RefreshDataFields();
        UpdateFieldStates();
    });

    m_byteRow = BuildByteEditors();
    m_longDataPreview = new QLineEdit(this);
    m_longDataPreview->setReadOnly(true);
    m_longDataPreview->setFont(Theme::MonospaceFont());
    m_longDataPreview->setToolTip(tr("8 bayttan uzun veri \"...\" düğmesiyle düzenlenir."));
    m_editorButton = new QPushButton(tr("..."), this);
    m_editorButton->setFixedWidth(40);
    m_editorButton->setToolTip(tr("Veriyi hex editörde düzenle"));
    connect(m_editorButton, &QPushButton::clicked, this, &TxMessageDialog::EditDataInEditor);

    auto *dataRow = new QHBoxLayout;
    dataRow->setSpacing(6);
    dataRow->addWidget(m_byteRow);
    dataRow->addWidget(m_longDataPreview, 1);
    dataRow->addWidget(m_editorButton, 0, Qt::AlignTop);
    dataRow->addStretch(0);

    auto *topGrid = new QGridLayout;
    topGrid->setHorizontalSpacing(12);
    topGrid->setVerticalSpacing(4);
    topGrid->addWidget(new QLabel(tr("ID (hex):"), this), 0, 0);
    topGrid->addWidget(new QLabel(tr("Uzunluk:"), this), 0, 1);
    topGrid->addWidget(new QLabel(tr("Veri (hex):"), this), 0, 2);
    topGrid->addWidget(m_idEdit, 1, 0, Qt::AlignTop);
    topGrid->addWidget(m_lengthCombo, 1, 1, Qt::AlignTop);
    topGrid->addLayout(dataRow, 1, 2);
    topGrid->setColumnStretch(2, 1);

    // --- Periyot ve mesaj tipi ---
    m_cycleSpin = new QSpinBox(this);
    m_cycleSpin->setRange(0, 3600000);
    m_cycleSpin->setFixedWidth(120);
    m_cycleSpin->setToolTip(tr("0: yalnızca elle gönderim; >0: periyodik gönderim aralığı"));
    m_pausedCheck = new QCheckBox(tr("Duraklatıldı"), this);
    m_pausedCheck->setToolTip(tr("Periyodik gönderimi başlangıçta durdurur (tablodaki onay kutusuyla da değiştirilebilir)."));
    connect(m_cycleSpin, &QSpinBox::valueChanged, this, &TxMessageDialog::UpdateFieldStates);

    auto *cycleRow = new QHBoxLayout;
    cycleRow->addWidget(m_cycleSpin);
    cycleRow->addWidget(new QLabel(tr("ms"), this));
    cycleRow->addStretch(1);

    auto *cycleBox = new QVBoxLayout;
    cycleBox->setSpacing(4);
    cycleBox->addWidget(new QLabel(tr("Periyot (cycle time):"), this));
    cycleBox->addLayout(cycleRow);
    cycleBox->addSpacing(6);
    cycleBox->addWidget(m_pausedCheck);
    cycleBox->addStretch(1);

    m_extendedCheck = new QCheckBox(tr("Extended frame (29 bit)"), this);
    m_rtrCheck = new QCheckBox(tr("Remote request (RTR)"), this);
    m_brsCheck = new QCheckBox(tr("Bit rate switch (BRS)"), this);
    m_brsCheck->setToolTip(tr("CAN FD veri fazını yüksek veri bit hızında gönderir."));
    connect(m_rtrCheck, &QCheckBox::toggled, this, &TxMessageDialog::UpdateFieldStates);

    auto *typeGroup = new QGroupBox(tr("Mesaj tipi"), this);
    auto *typeLayout = new QVBoxLayout(typeGroup);
    typeLayout->setSpacing(6);
    typeLayout->addWidget(m_extendedCheck);
    typeLayout->addWidget(m_rtrCheck);
    typeLayout->addWidget(m_brsCheck);

    auto *middleRow = new QHBoxLayout;
    middleRow->setSpacing(24);
    middleRow->addLayout(cycleBox);
    middleRow->addWidget(typeGroup, 1);

    // --- Açıklama ---
    m_commentEdit = new QLineEdit(this);
    m_commentEdit->setPlaceholderText(tr("İsteğe bağlı not"));
    auto *commentRow = new QHBoxLayout;
    commentRow->addWidget(new QLabel(tr("Açıklama:"), this));
    commentRow->addWidget(m_commentEdit, 1);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setObjectName(QStringLiteral("ErrorText"));
    m_errorLabel->setWordWrap(true);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (QPushButton *okButton = buttons->button(QDialogButtonBox::Ok)) {
        okButton->setProperty("primary", true);
    }
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("İptal"));
    connect(buttons, &QDialogButtonBox::accepted, this, &TxMessageDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &TxMessageDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 14);
    layout->setSpacing(12);
    layout->addLayout(formatRow);
    layout->addWidget(m_formatHint);
    layout->addLayout(topGrid);
    layout->addLayout(middleRow);
    layout->addLayout(commentRow);
    layout->addWidget(m_errorLabel);
    layout->addWidget(buttons);
}

QWidget *TxMessageDialog::BuildByteEditors()
{
    auto *row = new QWidget(this);
    auto *grid = new QGridLayout(row);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(0);

    const QRegularExpression hexPattern(QStringLiteral("[0-9A-F]{0,2}"));
    for (int i = 0; i < MaxClassicDataLength; ++i) {
        auto *edit = new QLineEdit(row);
        edit->setFont(Theme::MonospaceFont());
        edit->setMaxLength(2);
        edit->setFixedWidth(38);
        edit->setAlignment(Qt::AlignCenter);
        edit->setValidator(new UpperCaseValidator(hexPattern, edit));
        connect(edit, &QLineEdit::textEdited, this, [this, i](const QString &text) { HandleByteEdited(i, text); });
        connect(edit, &QLineEdit::editingFinished, this, [this, edit, i]() {
            edit->setText(QStringLiteral("%1")
                              .arg(static_cast<uint>(static_cast<quint8>(m_data.at(i))), 2, 16, QLatin1Char('0'))
                              .toUpper());
        });
        m_byteEdits.append(edit);

        auto *index = new QLabel(QString::number(i), row);
        index->setObjectName(QStringLiteral("MutedText"));
        index->setAlignment(Qt::AlignCenter);
        grid->addWidget(edit, 0, i);
        grid->addWidget(index, 1, i);
    }
    return row;
}

void TxMessageDialog::FillLengthCombo(int preferredLength)
{
    const bool fd = m_fdRadio->isChecked();
    QSignalBlocker blocker(m_lengthCombo);
    m_lengthCombo->clear();
    int selected = -1;
    if (fd) {
        for (const int length : FdLengths) {
            m_lengthCombo->addItem(QString::number(length), length);
            if (selected < 0 && length >= preferredLength) {
                selected = m_lengthCombo->count() - 1;
            }
        }
    } else {
        for (int length = 0; length <= MaxClassicDataLength; ++length) {
            m_lengthCombo->addItem(QString::number(length), length);
        }
        selected = qBound(0, preferredLength, MaxClassicDataLength);
    }
    m_lengthCombo->setCurrentIndex(selected < 0 ? m_lengthCombo->count() - 1 : selected);
    blocker.unblock();
    RefreshDataFields();
}

int TxMessageDialog::CurrentLength() const
{
    return m_lengthCombo->currentData().toInt();
}

void TxMessageDialog::UpdateFieldStates()
{
    const bool fd = m_fdRadio->isChecked();
    const int length = CurrentLength();
    const bool longData = length > MaxClassicDataLength;

    m_rtrCheck->setEnabled(!fd);
    if (fd) {
        m_rtrCheck->setChecked(false);
    }
    m_brsCheck->setEnabled(fd);
    if (!fd) {
        m_brsCheck->setChecked(false);
    }
    const bool rtr = m_rtrCheck->isChecked();

    m_byteRow->setVisible(!longData);
    m_longDataPreview->setVisible(longData);
    for (int i = 0; i < m_byteEdits.size(); ++i) {
        m_byteEdits.at(i)->setEnabled(!rtr && i < length);
    }
    m_editorButton->setEnabled(!rtr && length > 0);
    m_pausedCheck->setEnabled(m_cycleSpin->value() > 0);

    if (!m_fdSupported) {
        m_formatHint->setText(fd ? tr("%1 Bu mesaj gönderilemez; CAN CC'ye geçin.").arg(m_fdUnsupportedReason)
                                 : tr("Klasik CAN: en fazla 8 bayt veri. %1").arg(m_fdUnsupportedReason));
    } else {
        m_formatHint->setText(fd ? tr("CAN FD mesajları yalnızca kanal CAN FD modunda bağlıyken gönderilebilir.")
                                 : tr("Klasik CAN: en fazla 8 bayt veri."));
    }
}

void TxMessageDialog::RefreshDataFields()
{
    const int length = CurrentLength();
    for (int i = 0; i < m_byteEdits.size(); ++i) {
        QLineEdit *edit = m_byteEdits.at(i);
        edit->setText(i < length ? QStringLiteral("%1")
                                       .arg(static_cast<uint>(static_cast<quint8>(m_data.at(i))), 2, 16, QLatin1Char('0'))
                                       .toUpper()
                                 : QString());
    }
    m_longDataPreview->setText(m_data.left(length).toHex(' ').toUpper());
    m_longDataPreview->setCursorPosition(0);
}

void TxMessageDialog::HandleByteEdited(int position, const QString &text)
{
    bool ok = false;
    const uint value = text.toUInt(&ok, 16);
    m_data[position] = static_cast<char>(ok ? value : 0u);
    if (text.size() == 2 && position + 1 < m_byteEdits.size() && m_byteEdits.at(position + 1)->isEnabled()) {
        m_byteEdits.at(position + 1)->setFocus();
        m_byteEdits.at(position + 1)->selectAll();
    }
}

void TxMessageDialog::EditDataInEditor()
{
    const int length = CurrentLength();
    DataEditDialog editor(m_data.left(length), this);
    if (editor.exec() != QDialog::Accepted) {
        return;
    }
    const QByteArray edited = editor.Data();
    for (int i = 0; i < edited.size() && i < m_data.size(); ++i) {
        m_data[i] = edited.at(i);
    }
    RefreshDataFields();
}

void TxMessageDialog::HandleIdEdited(const QString &text)
{
    quint32 id = 0;
    if (ParseHexNumber(text, id) && id > MaxStandardCanId) {
        m_extendedCheck->setChecked(true);
    }
}

void TxMessageDialog::SetMessage(const TxMessage &message)
{
    m_message = message;
    const CanFrame &frame = message.frame;
    (frame.fd ? m_fdRadio : m_ccRadio)->setChecked(true);
    m_idEdit->setText(FormatCanId(frame.id, frame.extended));
    m_extendedCheck->setChecked(frame.extended);
    m_rtrCheck->setChecked(frame.rtr);
    m_brsCheck->setChecked(frame.brs);
    m_data.fill('\0');
    for (int i = 0; i < frame.dlc && i < m_data.size(); ++i) {
        m_data[i] = static_cast<char>(frame.data[static_cast<size_t>(i)]);
    }
    FillLengthCombo(frame.dlc);
    m_cycleSpin->setValue(message.periodMs);
    m_pausedCheck->setChecked(!message.periodicEnabled);
    m_commentEdit->setText(message.comment);
    // BRS onayı FillLengthCombo / UpdateFieldStates sırasında temizlenmiş olabilir.
    m_brsCheck->setChecked(frame.fd && frame.brs);
    RefreshDataFields();
    UpdateFieldStates();
}

void TxMessageDialog::SetFdSupported(bool supported, const QString &reason)
{
    m_fdSupported = supported;
    m_fdUnsupportedReason = reason;
    m_fdRadio->setEnabled(supported);
    m_fdRadio->setToolTip(supported ? tr("CAN FD, en fazla 64 bayt. Kanal CAN FD modunda bağlı olmalıdır.") : reason);
    UpdateFieldStates();
}

TxMessage TxMessageDialog::Message() const
{
    return m_message;
}

void TxMessageDialog::accept()
{
    TxMessage message = m_message;
    QString errorText;
    if (!ValidateInputs(message, errorText)) {
        m_errorLabel->setText(errorText);
        return;
    }
    m_message = message;
    QDialog::accept();
}

bool TxMessageDialog::ValidateInputs(TxMessage &message, QString &errorText) const
{
    quint32 id = 0;
    if (!ParseHexNumber(m_idEdit->text(), id)) {
        errorText = tr("ID geçerli bir hex sayı olmalıdır.");
        return false;
    }
    const bool extended = m_extendedCheck->isChecked();
    const quint32 maxId = extended ? MaxExtendedCanId : MaxStandardCanId;
    if (id > maxId) {
        errorText = tr("ID en fazla 0x%1 olabilir.").arg(QString::number(maxId, 16).toUpper());
        return false;
    }

    if (m_fdRadio->isChecked() && !m_fdSupported) {
        errorText = tr("Bağlı donanım CAN FD desteklemiyor; mesajı CAN CC olarak kaydedin.");
        return false;
    }

    CanFrame frame;
    frame.id = id;
    frame.extended = extended;
    frame.fd = m_fdRadio->isChecked();
    frame.brs = frame.fd && m_brsCheck->isChecked();
    frame.rtr = !frame.fd && m_rtrCheck->isChecked();
    frame.dlc = static_cast<quint8>(CurrentLength());
    if (!frame.rtr) {
        for (int i = 0; i < frame.dlc; ++i) {
            frame.data[static_cast<size_t>(i)] = static_cast<quint8>(m_data.at(i));
        }
    }

    message.frame = frame;
    message.periodMs = m_cycleSpin->value();
    message.periodicEnabled = !m_pausedCheck->isChecked();
    message.comment = m_commentEdit->text().trimmed();
    return true;
}
