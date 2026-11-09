#include "PdoDecoder.h"

#include <QStringList>

namespace {

// PDO COB-ID nesnesindeki "PDO geçersiz" biti.
constexpr quint64 PdoInvalidBit = 0x80000000ULL;
// PDO COB-ID nesnesindeki "29 bit ID" biti.
constexpr quint64 PdoExtendedBit = 0x20000000ULL;

// Çerçeve verisinden startBit'ten başlayan bitLength bitlik değeri (little-endian) okur.
quint64 ExtractBits(const CanFrame &frame, int startBit, int bitLength)
{
    quint64 value = 0;
    for (int i = 0; i < bitLength; ++i) {
        const int bit = startBit + i;
        const quint8 byte = frame.data[static_cast<size_t>(bit / 8)];
        if ((byte >> (bit % 8)) & 0x01) {
            value |= 1ULL << i;
        }
    }
    return value;
}

} // namespace

void PdoDecoder::BuildFromDictionary(const ObjectDictionary &dictionary, quint8 nodeId)
{
    m_definitions.clear();
    for (int n = 0; n < 512; ++n) {
        AddPdo(dictionary, nodeId, static_cast<quint16>(0x1400 + n), static_cast<quint16>(0x1600 + n), false, n + 1);
        AddPdo(dictionary, nodeId, static_cast<quint16>(0x1800 + n), static_cast<quint16>(0x1A00 + n), true, n + 1);
    }
}

void PdoDecoder::Clear()
{
    m_definitions.clear();
}

bool PdoDecoder::IsEmpty() const
{
    return m_definitions.isEmpty();
}

int PdoDecoder::Count() const
{
    return m_definitions.size();
}

QString PdoDecoder::DecodeFrame(const CanFrame &frame) const
{
    if (frame.rtr) {
        return QString();
    }
    const auto it = m_definitions.constFind(MakeKey(frame.id, frame.extended));
    if (it == m_definitions.constEnd()) {
        return QString();
    }

    const PdoDefinition &pdo = it.value();
    QStringList parts;
    int bitOffset = 0;
    const int availableBits = frame.dlc * 8;
    for (const PdoMappedItem &item : pdo.items) {
        if (bitOffset + item.bitLength > availableBits) {
            parts << QStringLiteral("(eksik veri)");
            break;
        }
        const quint64 raw = ExtractBits(frame, bitOffset, item.bitLength);
        bitOffset += item.bitLength;

        if (item.index < 0x1000) {
            continue; // dummy eşleme (veri tipi index'leri), atla
        }

        QByteArray bytes;
        const int byteCount = (item.bitLength + 7) / 8;
        for (int i = 0; i < byteCount; ++i) {
            bytes.append(static_cast<char>((raw >> (8 * i)) & 0xFF));
        }
        parts << QStringLiteral("%1=%2").arg(item.name, DecodeValue(item.dataType, bytes, false));
    }

    return QStringLiteral("%1%2: %3")
        .arg(pdo.transmit ? QStringLiteral("TPDO") : QStringLiteral("RPDO"))
        .arg(pdo.number)
        .arg(parts.join(QStringLiteral(", ")));
}

void PdoDecoder::AddPdo(const ObjectDictionary &dictionary, quint8 nodeId, quint16 communicationIndex,
                        quint16 mappingIndex, bool transmit, int number)
{
    const OdEntry *cobEntry = dictionary.FindEntry(communicationIndex, 1);
    const OdEntry *countEntry = dictionary.FindEntry(mappingIndex, 0);
    if (cobEntry == nullptr || countEntry == nullptr) {
        return;
    }

    const auto cobValue = ParseEdsNumber(cobEntry->defaultValue, nodeId);
    const auto countValue = ParseEdsNumber(countEntry->defaultValue, nodeId);
    if (!cobValue || !countValue || (*cobValue & PdoInvalidBit) || *countValue == 0) {
        return; // geçersiz PDO veya eşleme yok
    }

    PdoDefinition pdo;
    pdo.extended = (*cobValue & PdoExtendedBit) != 0;
    pdo.cobId = static_cast<quint32>(*cobValue & (pdo.extended ? MaxExtendedCanId : MaxStandardCanId));
    pdo.transmit = transmit;
    pdo.number = number;

    const int count = static_cast<int>(qMin<quint64>(*countValue, 64));
    for (int sub = 1; sub <= count; ++sub) {
        const OdEntry *mapEntry = dictionary.FindEntry(mappingIndex, static_cast<quint8>(sub));
        if (mapEntry == nullptr) {
            break;
        }
        const auto mapValue = ParseEdsNumber(mapEntry->defaultValue, nodeId);
        if (!mapValue) {
            break;
        }

        PdoMappedItem item;
        item.index = static_cast<quint16>((*mapValue >> 16) & 0xFFFF);
        item.subIndex = static_cast<quint8>((*mapValue >> 8) & 0xFF);
        item.bitLength = static_cast<quint8>(*mapValue & 0xFF);
        if (item.bitLength == 0) {
            break;
        }

        const OdEntry *target = dictionary.FindEntry(item.index, item.subIndex);
        if (target != nullptr) {
            item.name = target->name;
            item.dataType = target->dataType;
        } else {
            item.name = QStringLiteral("0x%1:%2")
                            .arg(item.index, 4, 16, QLatin1Char('0'))
                            .arg(static_cast<uint>(item.subIndex), 2, 16, QLatin1Char('0'));
            item.dataType = 0;
        }
        pdo.items.append(item);
    }

    if (!pdo.items.isEmpty()) {
        m_definitions.insert(MakeKey(pdo.cobId, pdo.extended), pdo);
    }
}

quint32 PdoDecoder::MakeKey(quint32 id, bool extended)
{
    return id | (extended ? 0x80000000u : 0u);
}
