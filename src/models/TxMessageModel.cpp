#include "TxMessageModel.h"

TxMessageModel::TxMessageModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int TxMessageModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_messages.size());
}

int TxMessageModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnTotal;
}

QVariant TxMessageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_messages.size()) {
        return QVariant();
    }
    const TxMessage &message = m_messages.at(index.row());
    const CanFrame &frame = message.frame;

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColumnId: return FormatCanId(frame.id, frame.extended);
        case ColumnType:
            return QStringLiteral("%1%2").arg(frame.extended ? QStringLiteral("Ext") : QStringLiteral("Std"),
                                              frame.rtr ? QStringLiteral(" RTR") : QString());
        case ColumnDlc: return frame.dlc;
        case ColumnData: return FormatFrameData(frame);
        case ColumnPeriod: return message.periodMs > 0 ? QString::number(message.periodMs) : tr("Elle");
        case ColumnCount: return message.count;
        case ColumnComment: return message.comment;
        default: break;
        }
    } else if (role == Qt::CheckStateRole && index.column() == ColumnId && message.periodMs > 0) {
        return message.periodicEnabled ? Qt::Checked : Qt::Unchecked;
    } else if (role == Qt::ToolTipRole && index.column() == ColumnId && message.periodMs > 0) {
        return tr("Periyodik gönderimi aç/kapat");
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColumnDlc || index.column() == ColumnPeriod || index.column() == ColumnCount) {
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        }
        return QVariant::fromValue(Qt::AlignLeft | Qt::AlignVCenter);
    }
    return QVariant();
}

QVariant TxMessageModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section) {
    case ColumnId: return tr("ID (hex)");
    case ColumnType: return tr("Tip");
    case ColumnDlc: return tr("DLC");
    case ColumnData: return tr("Veri");
    case ColumnPeriod: return tr("Periyot (ms)");
    case ColumnCount: return tr("Sayaç");
    case ColumnComment: return tr("Açıklama");
    default: break;
    }
    return QVariant();
}

Qt::ItemFlags TxMessageModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags result = QAbstractTableModel::flags(index);
    if (index.isValid() && index.column() == ColumnId && m_messages.at(index.row()).periodMs > 0) {
        result |= Qt::ItemIsUserCheckable;
    }
    return result;
}

bool TxMessageModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::CheckStateRole || index.column() != ColumnId) {
        return false;
    }
    TxMessage &message = m_messages[index.row()];
    message.periodicEnabled = value.toInt() == Qt::Checked;
    emit dataChanged(index, index, {Qt::CheckStateRole});
    return true;
}

int TxMessageModel::AddMessage(const TxMessage &message)
{
    const int row = static_cast<int>(m_messages.size());
    beginInsertRows(QModelIndex(), row, row);
    m_messages.append(message);
    endInsertRows();
    return row;
}

void TxMessageModel::UpdateMessage(int row, const TxMessage &message)
{
    if (row < 0 || row >= m_messages.size()) {
        return;
    }
    TxMessage updated = message;
    updated.count = m_messages.at(row).count;
    updated.nextDueMs = m_messages.at(row).nextDueMs;
    m_messages[row] = updated;
    emit dataChanged(index(row, 0), index(row, ColumnTotal - 1));
}

void TxMessageModel::RemoveMessage(int row)
{
    if (row < 0 || row >= m_messages.size()) {
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    m_messages.removeAt(row);
    endRemoveRows();
}

const TxMessage &TxMessageModel::MessageAt(int row) const
{
    return m_messages.at(row);
}

void TxMessageModel::MarkSent(int row)
{
    if (row < 0 || row >= m_messages.size()) {
        return;
    }
    ++m_messages[row].count;
    emit dataChanged(index(row, ColumnCount), index(row, ColumnCount), {Qt::DisplayRole});
}

QVector<int> TxMessageModel::TakeDueRows(qint64 nowMs)
{
    QVector<int> due;
    for (int row = 0; row < m_messages.size(); ++row) {
        TxMessage &message = m_messages[row];
        if (message.periodMs <= 0 || !message.periodicEnabled || nowMs < message.nextDueMs) {
            continue;
        }
        due.append(row);
        message.nextDueMs += message.periodMs;
        if (message.nextDueMs <= nowMs) {
            // Gecikme birikmişse (ör. UI meşguldü) art arda gönderim yapmak yerine yeniden hizala.
            message.nextDueMs = nowMs + message.periodMs;
        }
    }
    return due;
}

void TxMessageModel::ResetSchedule(qint64 nowMs)
{
    for (TxMessage &message : m_messages) {
        message.nextDueMs = nowMs;
    }
}

void TxMessageModel::ResetCounters()
{
    if (m_messages.isEmpty()) {
        return;
    }
    for (TxMessage &message : m_messages) {
        message.count = 0;
    }
    emit dataChanged(index(0, ColumnCount), index(static_cast<int>(m_messages.size()) - 1, ColumnCount));
}
