#include "FilterPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

enum FilterColumn
{
    FilterColumnEnabled,
    FilterColumnAction,
    FilterColumnMode,
    FilterColumnValue,
    FilterColumnTotal
};

} // namespace

FilterPanel::FilterPanel(QWidget *parent)
    : QWidget(parent)
{
    BuildUi();
    UpdateInputState();
}

const MessageFilter &FilterPanel::Filter() const
{
    return m_filter;
}

void FilterPanel::BuildUi()
{
    m_enabledCheck = new QCheckBox(tr("Filtre etkin"), this);
    connect(m_enabledCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        m_filter.SetEnabled(enabled);
        emit FilterChanged();
    });

    m_table = new QTableWidget(0, FilterColumnTotal, this);
    m_table->setHorizontalHeaderLabels({tr("Etkin"), tr("İşlem"), tr("Mod"), tr("Değer")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_table, &QTableWidget::itemChanged, this, &FilterPanel::HandleItemChanged);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(MessageFilter::ModeName(FilterMode::SingleId), static_cast<int>(FilterMode::SingleId));
    m_modeCombo->addItem(MessageFilter::ModeName(FilterMode::IdRange), static_cast<int>(FilterMode::IdRange));
    m_modeCombo->addItem(MessageFilter::ModeName(FilterMode::Mask), static_cast<int>(FilterMode::Mask));
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, &FilterPanel::UpdateInputState);

    m_actionCombo = new QComboBox(this);
    m_actionCombo->addItem(tr("Dahil et"), true);
    m_actionCombo->addItem(tr("Hariç tut"), false);

    m_firstEdit = new QLineEdit(this);
    m_secondEdit = new QLineEdit(this);
    m_secondLabel = new QLabel(this);
    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #c62828;"));
    m_errorLabel->setWordWrap(true);
    connect(m_firstEdit, &QLineEdit::returnPressed, this, &FilterPanel::AddRuleFromInputs);
    connect(m_secondEdit, &QLineEdit::returnPressed, this, &FilterPanel::AddRuleFromInputs);

    auto *form = new QFormLayout;
    form->addRow(tr("Mod:"), m_modeCombo);
    form->addRow(tr("İşlem:"), m_actionCombo);
    form->addRow(tr("ID (hex):"), m_firstEdit);
    form->addRow(m_secondLabel, m_secondEdit);

    auto *addButton = new QPushButton(tr("Ekle"), this);
    auto *removeButton = new QPushButton(tr("Sil"), this);
    auto *clearButton = new QPushButton(tr("Temizle"), this);
    connect(addButton, &QPushButton::clicked, this, &FilterPanel::AddRuleFromInputs);
    connect(removeButton, &QPushButton::clicked, this, &FilterPanel::RemoveSelectedRules);
    connect(clearButton, &QPushButton::clicked, this, &FilterPanel::ClearRules);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(addButton);
    buttonRow->addWidget(removeButton);
    buttonRow->addWidget(clearButton);

    auto *hint = new QLabel(tr("Hariç tut kuralları önceliklidir. En az bir \"Dahil et\" kuralı varsa "
                               "yalnızca eşleşen ID'ler gösterilir."),
                            this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: palette(mid);"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_enabledCheck);
    layout->addWidget(m_table, 1);
    layout->addLayout(form);
    layout->addLayout(buttonRow);
    layout->addWidget(m_errorLabel);
    layout->addWidget(hint);
}

void FilterPanel::AddRuleFromInputs()
{
    FilterRule rule;
    rule.mode = static_cast<FilterMode>(m_modeCombo->currentData().toInt());
    rule.include = m_actionCombo->currentData().toBool();

    if (!ParseHexNumber(m_firstEdit->text(), rule.first) || rule.first > MaxExtendedCanId) {
        m_errorLabel->setText(tr("ID geçerli bir hex sayı olmalıdır (en fazla 1FFFFFFF)."));
        return;
    }
    if (rule.mode != FilterMode::SingleId) {
        if (!ParseHexNumber(m_secondEdit->text(), rule.second) || rule.second > MaxExtendedCanId) {
            m_errorLabel->setText(rule.mode == FilterMode::IdRange
                                      ? tr("Aralık sonu geçerli bir hex sayı olmalıdır.")
                                      : tr("Maske geçerli bir hex sayı olmalıdır."));
            return;
        }
    }

    m_errorLabel->clear();
    m_filter.AddRule(rule);
    RefreshTable();

    // Kullanıcı kural eklediyse filtreyi otomatik aç.
    if (!m_enabledCheck->isChecked()) {
        m_enabledCheck->setChecked(true); // FilterChanged yayılır
    } else {
        emit FilterChanged();
    }
}

void FilterPanel::RemoveSelectedRules()
{
    QList<int> rows;
    const QList<QTableWidgetItem *> items = m_table->selectedItems();
    for (const QTableWidgetItem *item : items) {
        if (!rows.contains(item->row())) {
            rows.append(item->row());
        }
    }
    if (rows.isEmpty()) {
        return;
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (const int row : rows) {
        m_filter.RemoveRule(row);
    }
    RefreshTable();
    emit FilterChanged();
}

void FilterPanel::ClearRules()
{
    if (m_filter.Rules().isEmpty()) {
        return;
    }
    m_filter.ClearRules();
    RefreshTable();
    emit FilterChanged();
}

void FilterPanel::RefreshTable()
{
    m_updatingTable = true;
    const QVector<FilterRule> &rules = m_filter.Rules();
    m_table->setRowCount(static_cast<int>(rules.size()));
    for (int row = 0; row < rules.size(); ++row) {
        const FilterRule &rule = rules.at(row);

        auto *enabledItem = new QTableWidgetItem;
        enabledItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        enabledItem->setCheckState(rule.enabled ? Qt::Checked : Qt::Unchecked);
        m_table->setItem(row, FilterColumnEnabled, enabledItem);
        m_table->setItem(row, FilterColumnAction,
                         new QTableWidgetItem(rule.include ? tr("Dahil et") : tr("Hariç tut")));
        m_table->setItem(row, FilterColumnMode, new QTableWidgetItem(MessageFilter::ModeName(rule.mode)));
        m_table->setItem(row, FilterColumnValue, new QTableWidgetItem(MessageFilter::DescribeRule(rule)));
    }
    m_updatingTable = false;
}

void FilterPanel::HandleItemChanged(QTableWidgetItem *item)
{
    if (m_updatingTable || item == nullptr || item->column() != FilterColumnEnabled) {
        return;
    }
    m_filter.SetRuleEnabled(item->row(), item->checkState() == Qt::Checked);
    emit FilterChanged();
}

void FilterPanel::UpdateInputState()
{
    const auto mode = static_cast<FilterMode>(m_modeCombo->currentData().toInt());
    switch (mode) {
    case FilterMode::SingleId:
        m_secondLabel->setText(tr("—"));
        m_secondEdit->setEnabled(false);
        m_secondEdit->clear();
        m_firstEdit->setPlaceholderText(tr("ör. 181"));
        break;
    case FilterMode::IdRange:
        m_secondLabel->setText(tr("Bitiş (hex):"));
        m_secondEdit->setEnabled(true);
        m_firstEdit->setPlaceholderText(tr("ör. 180"));
        m_secondEdit->setPlaceholderText(tr("ör. 1FF"));
        break;
    case FilterMode::Mask:
        m_secondLabel->setText(tr("Maske (hex):"));
        m_secondEdit->setEnabled(true);
        m_firstEdit->setPlaceholderText(tr("ör. 580"));
        m_secondEdit->setPlaceholderText(tr("ör. 780"));
        break;
    }
}
