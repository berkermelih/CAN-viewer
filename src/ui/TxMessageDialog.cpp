#include "TxMessageDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

TxMessageDialog::TxMessageDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Gönderim Mesajı"));

    m_idEdit = new QLineEdit(this);
    m_idEdit->setPlaceholderText(tr("ör. 601"));
    m_extendedCheck = new QCheckBox(tr("Extended (29 bit)"), this);
    m_rtrCheck = new QCheckBox(tr("RTR"), this);
    m_dlcSpin = new QSpinBox(this);
    m_dlcSpin->setRange(0, 8);
    m_dataEdit = new QLineEdit(this);
    m_dataEdit->setPlaceholderText(tr("ör. 40 18 10 01 00 00 00 00"));
    m_periodSpin = new QSpinBox(this);
    m_periodSpin->setRange(0, 3600000);
    m_periodSpin->setSuffix(tr(" ms"));
    m_periodSpin->setSpecialValueText(tr("Elle (periyodik değil)"));
    m_commentEdit = new QLineEdit(this);
    m_errorLabel = new QLabel(this);
    m_errorLabel->setObjectName(QStringLiteral("ErrorText"));
    m_errorLabel->setWordWrap(true);

    auto *idRow = new QHBoxLayout;
    idRow->addWidget(m_idEdit, 1);
    idRow->addWidget(m_extendedCheck);
    idRow->addWidget(m_rtrCheck);

    auto *form = new QFormLayout;
    form->addRow(tr("ID (hex):"), idRow);
    form->addRow(tr("Veri (hex):"), m_dataEdit);
    form->addRow(tr("DLC:"), m_dlcSpin);
    form->addRow(tr("Periyot:"), m_periodSpin);
    form->addRow(tr("Açıklama:"), m_commentEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (QPushButton *okButton = buttons->button(QDialogButtonBox::Ok)) {
        okButton->setProperty("primary", true);
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &TxMessageDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &TxMessageDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_errorLabel);
    layout->addWidget(buttons);

    connect(m_rtrCheck, &QCheckBox::toggled, this, &TxMessageDialog::UpdateRtrState);
    UpdateRtrState();
    resize(460, sizeHint().height());
}

void TxMessageDialog::SetMessage(const TxMessage &message)
{
    m_message = message;
    m_idEdit->setText(FormatCanId(message.frame.id, message.frame.extended));
    m_extendedCheck->setChecked(message.frame.extended);
    m_rtrCheck->setChecked(message.frame.rtr);
    m_dlcSpin->setValue(message.frame.dlc);
    m_dataEdit->setText(message.frame.rtr ? QString() : FormatFrameData(message.frame));
    m_periodSpin->setValue(message.periodMs);
    m_commentEdit->setText(message.comment);
    UpdateRtrState();
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

    CanFrame frame;
    frame.id = id;
    frame.extended = extended;
    frame.rtr = m_rtrCheck->isChecked();

    if (frame.rtr) {
        frame.dlc = static_cast<quint8>(m_dlcSpin->value());
    } else {
        QByteArray bytes;
        if (!ParseHexBytes(m_dataEdit->text(), bytes)) {
            errorText = tr("Veri hex baytlardan oluşmalıdır (ör. 01 A2 FF).");
            return false;
        }
        if (bytes.size() > 8) {
            errorText = tr("Klasik CAN en fazla 8 bayt veri taşır.");
            return false;
        }
        frame.dlc = static_cast<quint8>(bytes.size());
        for (int i = 0; i < bytes.size(); ++i) {
            frame.data[static_cast<size_t>(i)] = static_cast<quint8>(bytes.at(i));
        }
    }

    message.frame = frame;
    message.periodMs = m_periodSpin->value();
    message.comment = m_commentEdit->text().trimmed();
    return true;
}

void TxMessageDialog::UpdateRtrState()
{
    const bool rtr = m_rtrCheck->isChecked();
    m_dlcSpin->setEnabled(rtr);
    m_dataEdit->setEnabled(!rtr);
    if (!rtr) {
        m_dlcSpin->setToolTip(tr("Veri baytlarından otomatik belirlenir."));
    } else {
        m_dlcSpin->setToolTip(QString());
    }
}
