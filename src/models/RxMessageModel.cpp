#include "RxMessageModel.h"

#include <utility>

RxMessageModel::RxMessageModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int RxMessageModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

int RxMessageModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnTotal;
}

QVariant RxMessageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size()) {
        return QVariant();
    }
    const Entry &entry = m_entries.at(index.row());
    const CanFrame &frame = entry.frame;

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColumnId: return FormatCanId(frame.id, frame.extended);
        case ColumnType:
            return QStringLiteral("%1%2").arg(frame.extended ? QStringLiteral("Ext") : QStringLiteral("Std"),
                                              frame.rtr ? QStringLiteral(" RTR") : QString());
        case ColumnDlc: return frame.dlc;
        case ColumnData: return FormatFrameData(frame);
        case ColumnCycle:
            return entry.cycleUs < 0 ? QString() : QString::number(static_cast<double>(entry.cycleUs) / 1000.0, 'f', 1);
        case ColumnCount: return entry.count;
        case ColumnTime: return QString::number(static_cast<double>(frame.timestampUs) / 1000000.0, 'f', 3);
        case ColumnInterpretation: return entry.interpretation;
        default: break;
        }
    } else if (role == SortRole) {
        switch (index.column()) {
        case ColumnId: return static_cast<qulonglong>(MakeKey(frame));
        case ColumnDlc: return frame.dlc;
        case ColumnCycle: return entry.cycleUs;
        case ColumnCount: return entry.count;
        case ColumnTime: return frame.timestampUs;
        default: return data(index, Qt::DisplayRole);
        }
    } else if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case ColumnDlc:
        case ColumnCycle:
        case ColumnCount:
        case ColumnTime:
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return QVariant::fromValue(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    return QVariant();
}

QVariant RxMessageModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section) {
    case ColumnId: return tr("ID (hex)");
    case ColumnType: return tr("Tip");
    case ColumnDlc: return tr("DLC");
    case ColumnData: return tr("Veri");
    case ColumnCycle: return tr("Periyot (ms)");
    case ColumnCount: return tr("Sayaç");
    case ColumnTime: return tr("Zaman (s)");
    case ColumnInterpretation: return tr("CANopen");
    default: break;
    }
    return QVariant();
}

void RxMessageModel::AddFrames(const QVector<CanFrame> &frames)
{
    int firstChanged = -1;
    int lastChanged = -1;

    for (const CanFrame &frame : frames) {
        const quint32 key = MakeKey(frame);
        const auto it = m_rowByKey.constFind(key);
        if (it == m_rowByKey.constEnd()) {
            const int row = static_cast<int>(m_entries.size());
            Entry entry;
            entry.frame = frame;
            entry.count = 1;
            if (m_interpreter) {
                entry.interpretation = m_interpreter(frame);
            }
            beginInsertRows(QModelIndex(), row, row);
            m_entries.append(entry);
            m_rowByKey.insert(key, row);
            endInsertRows();
            continue;
        }

        const int row = it.value();
        Entry &entry = m_entries[row];
        entry.cycleUs = frame.timestampUs - entry.frame.timestampUs;
        entry.frame = frame;
        ++entry.count;
        if (m_interpreter) {
            entry.interpretation = m_interpreter(frame);
        }
        firstChanged = firstChanged < 0 ? row : qMin(firstChanged, row);
        lastChanged = qMax(lastChanged, row);
    }

    if (firstChanged >= 0) {
        emit dataChanged(index(firstChanged, 0), index(lastChanged, ColumnTotal - 1));
    }
}

void RxMessageModel::Clear()
{
    beginResetModel();
    m_entries.clear();
    m_rowByKey.clear();
    endResetModel();
}

void RxMessageModel::SetInterpreter(InterpreterFunction interpreter)
{
    m_interpreter = std::move(interpreter);
    RefreshInterpretations();
}

void RxMessageModel::RefreshInterpretations()
{
    if (m_entries.isEmpty()) {
        return;
    }
    for (Entry &entry : m_entries) {
        entry.interpretation = m_interpreter ? m_interpreter(entry.frame) : QString();
    }
    emit dataChanged(index(0, ColumnInterpretation), index(static_cast<int>(m_entries.size()) - 1, ColumnInterpretation));
}

const CanFrame &RxMessageModel::FrameAt(int row) const
{
    return m_entries.at(row).frame;
}

quint32 RxMessageModel::MakeKey(const CanFrame &frame)
{
    // 29 bit ID'de 31. bit boştur; extended bilgisini orada taşı, RTR'yi 30. bitte.
    return frame.id | (frame.extended ? 0x80000000u : 0u) | (frame.rtr ? 0x40000000u : 0u);
}
