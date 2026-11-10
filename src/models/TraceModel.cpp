#include "TraceModel.h"

#include <utility>

namespace {

// Çerçeve ID'si ve tipinden anahtar üretir (Δt hesabı için; yön bilgisi ayrıca 64 bitte taşınır).
quint64 MakeKey(const CanFrame &frame)
{
    return static_cast<quint64>(frame.id) | (frame.extended ? 0x80000000ULL : 0ULL)
           | (frame.rtr ? 0x40000000ULL : 0ULL) | (frame.fd ? 0x20000000ULL : 0ULL);
}

} // namespace

TraceModel::TraceModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    m_flushTimer.setInterval(50);
    connect(&m_flushTimer, &QTimer::timeout, this, &TraceModel::FlushPending);
    m_flushTimer.start();
}

int TraceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

int TraceModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnTotal;
}

QVariant TraceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || static_cast<size_t>(index.row()) >= m_entries.size()) {
        return QVariant();
    }
    const Entry &entry = m_entries[static_cast<size_t>(index.row())];
    const CanFrame &frame = entry.frame;

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColumnNumber: return entry.number;
        case ColumnTime: return QString::number(static_cast<double>(entry.timeUs) / 1000000.0, 'f', 4);
        case ColumnDelta:
            return entry.deltaUs < 0 ? QString() : QString::number(static_cast<double>(entry.deltaUs) / 1000.0, 'f', 1);
        case ColumnDirection: return entry.direction == TraceDirection::Rx ? QStringLiteral("Rx") : QStringLiteral("Tx");
        case ColumnId: return FormatCanId(frame.id, frame.extended, m_idFormat);
        case ColumnType: return FormatFrameType(frame);
        case ColumnDlc: return frame.dlc;
        case ColumnData: return FormatFrameData(frame, m_dataFormat);
        case ColumnInterpretation: return m_interpreter ? m_interpreter(frame) : QString();
        default: break;
        }
    } else if (role == Qt::ForegroundRole && index.column() == ColumnDirection) {
        const QColor &color = entry.direction == TraceDirection::Rx ? m_rxColor : m_txColor;
        if (color.isValid()) {
            return color;
        }
    } else if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case ColumnNumber:
        case ColumnTime:
        case ColumnDelta:
        case ColumnDlc:
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        case ColumnDirection:
            return QVariant::fromValue(Qt::AlignCenter);
        default:
            return QVariant::fromValue(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    return QVariant();
}

QVariant TraceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section) {
    case ColumnNumber: return tr("No");
    case ColumnTime: return tr("Zaman (s)");
    case ColumnDelta: return tr("Δt (ms)");
    case ColumnDirection: return tr("Yön");
    case ColumnId: return m_idFormat == IdFormat::Hex ? tr("ID (hex)") : tr("ID (dec)");
    case ColumnType: return tr("Tip");
    case ColumnDlc: return tr("DLC");
    case ColumnData:
        return m_dataFormat == DataFormat::Hex ? tr("Veri (hex)")
               : m_dataFormat == DataFormat::Decimal ? tr("Veri (dec)") : tr("Veri (ASCII)");
    case ColumnInterpretation: return tr("CANopen");
    default: break;
    }
    return QVariant();
}

void TraceModel::AppendFrames(const QVector<CanFrame> &frames, TraceDirection direction, qint64 timeUs)
{
    if (m_paused) {
        return;
    }
    for (const CanFrame &frame : frames) {
        Entry entry;
        entry.frame = frame;
        entry.direction = direction;
        entry.timeUs = timeUs;
        m_pending.append(entry);
    }
}

void TraceModel::SetPaused(bool paused)
{
    m_paused = paused;
    if (paused) {
        m_pending.clear();
    }
}

bool TraceModel::IsPaused() const
{
    return m_paused;
}

void TraceModel::Clear()
{
    beginResetModel();
    m_entries.clear();
    m_pending.clear();
    m_lastTimeByKey.clear();
    m_nextNumber = 1;
    endResetModel();
}

void TraceModel::SetMaximumRows(int maximumRows)
{
    m_maximumRows = qMax(1000, maximumRows);
    FlushPending();
}

void TraceModel::SetInterpreter(InterpreterFunction interpreter)
{
    m_interpreter = std::move(interpreter);
    if (!m_entries.empty()) {
        emit dataChanged(index(0, ColumnInterpretation),
                         index(static_cast<int>(m_entries.size()) - 1, ColumnInterpretation));
    }
}

void TraceModel::SetDirectionColors(const QColor &rxColor, const QColor &txColor)
{
    m_rxColor = rxColor;
    m_txColor = txColor;
    if (!m_entries.empty()) {
        emit dataChanged(index(0, ColumnDirection), index(static_cast<int>(m_entries.size()) - 1, ColumnDirection),
                         {Qt::ForegroundRole});
    }
}

void TraceModel::SetDisplayFormat(IdFormat idFormat, DataFormat dataFormat)
{
    m_idFormat = idFormat;
    m_dataFormat = dataFormat;
    emit headerDataChanged(Qt::Horizontal, 0, ColumnTotal - 1);
    if (!m_entries.empty()) {
        emit dataChanged(index(0, ColumnId), index(static_cast<int>(m_entries.size()) - 1, ColumnData));
    }
}

const CanFrame *TraceModel::FramePointerAt(int row) const
{
    if (row < 0 || static_cast<size_t>(row) >= m_entries.size()) {
        return nullptr;
    }
    return &m_entries[static_cast<size_t>(row)].frame;
}

QString TraceModel::CsvHeader()
{
    return QStringLiteral("No;Zaman_s;Delta_ms;Yon;ID;Tip;DLC;Veri;CANopen");
}

QString TraceModel::CsvLine(int row) const
{
    const Entry &entry = m_entries[static_cast<size_t>(row)];
    QString interpretation = m_interpreter ? m_interpreter(entry.frame) : QString();
    interpretation.replace(QLatin1Char(';'), QLatin1Char(','));
    return QStringLiteral("%1;%2;%3;%4;%5;%6;%7;%8;%9")
        .arg(entry.number)
        .arg(QString::number(static_cast<double>(entry.timeUs) / 1000000.0, 'f', 6))
        .arg(entry.deltaUs < 0 ? QString() : QString::number(static_cast<double>(entry.deltaUs) / 1000.0, 'f', 3))
        .arg(entry.direction == TraceDirection::Rx ? QStringLiteral("Rx") : QStringLiteral("Tx"))
        .arg(FormatCanId(entry.frame.id, entry.frame.extended))
        .arg(FormatFrameType(entry.frame))
        .arg(entry.frame.dlc)
        .arg(FormatFrameData(entry.frame))
        .arg(interpretation);
}

void TraceModel::FlushPending()
{
    if (!m_pending.isEmpty()) {
        // Numara ve Δt değerlerini ekleme sırasında hesapla.
        for (Entry &entry : m_pending) {
            entry.number = m_nextNumber++;
            const quint64 key = MakeKey(entry.frame) | (entry.direction == TraceDirection::Tx ? 0x100000000ULL : 0ULL);
            const auto it = m_lastTimeByKey.constFind(key);
            entry.deltaUs = it == m_lastTimeByKey.constEnd() ? -1 : entry.timeUs - it.value();
            m_lastTimeByKey.insert(key, entry.timeUs);
        }

        const int first = static_cast<int>(m_entries.size());
        const int last = first + static_cast<int>(m_pending.size()) - 1;
        beginInsertRows(QModelIndex(), first, last);
        for (const Entry &entry : std::as_const(m_pending)) {
            m_entries.push_back(entry);
        }
        endInsertRows();
        m_pending.clear();
    }

    const int excess = static_cast<int>(m_entries.size()) - m_maximumRows;
    if (excess > 0) {
        beginRemoveRows(QModelIndex(), 0, excess - 1);
        m_entries.erase(m_entries.begin(), m_entries.begin() + excess);
        endRemoveRows();
    }
}
