#include "ObjectDictionary.h"

#include "../can/CanFrame.h"

#include <QCoreApplication>
#include <QStringList>

#include <cstring>
#include <limits>

namespace {

// Little-endian bayt dizisinden en fazla count baytı işaretsiz sayı olarak okur.
quint64 ReadLittleEndian(const QByteArray &bytes, int count)
{
    quint64 value = 0;
    const int length = qMin(count, static_cast<int>(bytes.size()));
    for (int i = 0; i < length; ++i) {
        value |= static_cast<quint64>(static_cast<quint8>(bytes.at(i))) << (8 * i);
    }
    return value;
}

// Sayıyı count baytlık little-endian diziye yazar.
QByteArray WriteLittleEndian(quint64 value, int count)
{
    QByteArray bytes;
    for (int i = 0; i < count; ++i) {
        bytes.append(static_cast<char>((value >> (8 * i)) & 0xFF));
    }
    return bytes;
}

// Sayıyı hex metin olarak (büyük harf) biçimlendirir.
QString HexText(quint64 value)
{
    return QStringLiteral("0x") + QString::number(value, 16).toUpper();
}

} // namespace

void ObjectDictionary::Clear()
{
    m_objects.clear();
    m_fileInfo.clear();
    m_deviceInfo.clear();
    m_fileInfoEntries.clear();
    m_deviceInfoEntries.clear();
    m_commissioningEntries.clear();
    m_filePath.clear();
}

bool ObjectDictionary::IsEmpty() const
{
    return m_objects.isEmpty();
}

void ObjectDictionary::InsertObject(const OdObject &object)
{
    m_objects.insert(object.index, object);
}

const OdObject *ObjectDictionary::FindObject(quint16 index) const
{
    const auto it = m_objects.constFind(index);
    return it == m_objects.constEnd() ? nullptr : &it.value();
}

const OdEntry *ObjectDictionary::FindEntry(quint16 index, quint8 subIndex) const
{
    const OdObject *object = FindObject(index);
    if (object == nullptr) {
        return nullptr;
    }
    for (const OdEntry &entry : object->entries) {
        if (entry.subIndex == subIndex) {
            return &entry;
        }
    }
    return nullptr;
}

const QMap<quint16, OdObject> &ObjectDictionary::Objects() const
{
    return m_objects;
}

void ObjectDictionary::SetFileInfo(const QHash<QString, QString> &info)
{
    m_fileInfo = info;
}

void ObjectDictionary::SetDeviceInfo(const QHash<QString, QString> &info)
{
    m_deviceInfo = info;
}

QString ObjectDictionary::DeviceInfoValue(const QString &key) const
{
    return m_deviceInfo.value(key.toLower());
}

QString ObjectDictionary::FileInfoValue(const QString &key) const
{
    return m_fileInfo.value(key.toLower());
}

void ObjectDictionary::SetFileInfoEntries(const InfoEntries &entries)
{
    m_fileInfoEntries = entries;
}

const ObjectDictionary::InfoEntries &ObjectDictionary::FileInfoEntries() const
{
    return m_fileInfoEntries;
}

void ObjectDictionary::SetDeviceInfoEntries(const InfoEntries &entries)
{
    m_deviceInfoEntries = entries;
}

const ObjectDictionary::InfoEntries &ObjectDictionary::DeviceInfoEntries() const
{
    return m_deviceInfoEntries;
}
void ObjectDictionary::SetCommissioningEntries(const InfoEntries &entries)
{
    m_commissioningEntries = entries;
}

const ObjectDictionary::InfoEntries &ObjectDictionary::CommissioningEntries() const
{
    return m_commissioningEntries;
}

QString ObjectDictionary::CommissioningValue(const QString &key) const
{
    for (const auto &pair : m_commissioningEntries) {
        if (pair.first.compare(key, Qt::CaseInsensitive) == 0) {
            return pair.second;
        }
    }
    return QString();
}

bool ObjectDictionary::HasParameterValues() const
{
    for (const OdObject &object : m_objects) {
        for (const OdEntry &entry : object.entries) {
            if (!entry.parameterValue.isEmpty()) {
                return true;
            }
        }
    }
    return false;
}

void ObjectDictionary::SetFilePath(const QString &filePath)
{
    m_filePath = filePath;
}

QString ObjectDictionary::FilePath() const
{
    return m_filePath;
}

QString DataTypeName(quint16 dataType)
{
    using namespace CanOpenDataType;
    switch (dataType) {
    case Boolean: return QStringLiteral("BOOLEAN");
    case Integer8: return QStringLiteral("INTEGER8");
    case Integer16: return QStringLiteral("INTEGER16");
    case Integer24: return QStringLiteral("INTEGER24");
    case Integer32: return QStringLiteral("INTEGER32");
    case Integer40: return QStringLiteral("INTEGER40");
    case Integer48: return QStringLiteral("INTEGER48");
    case Integer56: return QStringLiteral("INTEGER56");
    case Integer64: return QStringLiteral("INTEGER64");
    case Unsigned8: return QStringLiteral("UNSIGNED8");
    case Unsigned16: return QStringLiteral("UNSIGNED16");
    case Unsigned24: return QStringLiteral("UNSIGNED24");
    case Unsigned32: return QStringLiteral("UNSIGNED32");
    case Unsigned40: return QStringLiteral("UNSIGNED40");
    case Unsigned48: return QStringLiteral("UNSIGNED48");
    case Unsigned56: return QStringLiteral("UNSIGNED56");
    case Unsigned64: return QStringLiteral("UNSIGNED64");
    case Real32: return QStringLiteral("REAL32");
    case Real64: return QStringLiteral("REAL64");
    case VisibleString: return QStringLiteral("VISIBLE_STRING");
    case OctetString: return QStringLiteral("OCTET_STRING");
    case UnicodeString: return QStringLiteral("UNICODE_STRING");
    case Domain: return QStringLiteral("DOMAIN");
    default: break;
    }
    return dataType == 0 ? QString() : HexText(dataType);
}

QString ObjectTypeName(quint8 objectType)
{
    using namespace CanOpenObjectType;
    switch (objectType) {
    case Null: return QStringLiteral("NULL");
    case DomainObject: return QStringLiteral("DOMAIN");
    case DefType: return QStringLiteral("DEFTYPE");
    case DefStruct: return QStringLiteral("DEFSTRUCT");
    case Var: return QStringLiteral("VAR");
    case Array: return QStringLiteral("ARRAY");
    case Record: return QStringLiteral("RECORD");
    default: break;
    }
    return HexText(objectType);
}

int DataTypeSize(quint16 dataType)
{
    using namespace CanOpenDataType;
    switch (dataType) {
    case Boolean:
    case Integer8:
    case Unsigned8:
        return 1;
    case Integer16:
    case Unsigned16:
        return 2;
    case Integer24:
    case Unsigned24:
        return 3;
    case Integer32:
    case Unsigned32:
    case Real32:
        return 4;
    case Integer40:
    case Unsigned40:
        return 5;
    case Integer48:
    case Unsigned48:
        return 6;
    case Integer56:
    case Unsigned56:
        return 7;
    case Integer64:
    case Unsigned64:
    case Real64:
        return 8;
    default:
        break;
    }
    return 0;
}

bool IsSignedIntegerType(quint16 dataType)
{
    using namespace CanOpenDataType;
    return dataType == Integer8 || dataType == Integer16 || dataType == Integer24 || dataType == Integer32
           || dataType == Integer40 || dataType == Integer48 || dataType == Integer56 || dataType == Integer64;
}

bool IsUnsignedIntegerType(quint16 dataType)
{
    using namespace CanOpenDataType;
    return dataType == Unsigned8 || dataType == Unsigned16 || dataType == Unsigned24 || dataType == Unsigned32
           || dataType == Unsigned40 || dataType == Unsigned48 || dataType == Unsigned56 || dataType == Unsigned64;
}

QString DecodeValue(quint16 dataType, const QByteArray &bytes, bool withHex)
{
    using namespace CanOpenDataType;

    if (dataType == Boolean) {
        return (!bytes.isEmpty() && bytes.at(0) != 0) ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
    }

    if (IsUnsignedIntegerType(dataType)) {
        const quint64 value = ReadLittleEndian(bytes, DataTypeSize(dataType));
        return withHex ? QStringLiteral("%1 (%2)").arg(value).arg(HexText(value)) : QString::number(value);
    }

    if (IsSignedIntegerType(dataType)) {
        const int size = qMin(DataTypeSize(dataType), static_cast<int>(bytes.size()));
        quint64 raw = ReadLittleEndian(bytes, size);
        const int bits = size * 8;
        if (bits > 0 && bits < 64 && (raw & (1ULL << (bits - 1)))) {
            raw |= ~0ULL << bits; // işaret genişletme
        }
        const qint64 value = static_cast<qint64>(raw);
        return withHex ? QStringLiteral("%1 (%2)").arg(value).arg(HexText(ReadLittleEndian(bytes, size)))
                       : QString::number(value);
    }

    if (dataType == Real32 && bytes.size() >= 4) {
        const quint32 raw = static_cast<quint32>(ReadLittleEndian(bytes, 4));
        float value = 0.0f;
        std::memcpy(&value, &raw, sizeof(value));
        return QString::number(static_cast<double>(value), 'g', 7);
    }

    if (dataType == Real64 && bytes.size() >= 8) {
        const quint64 raw = ReadLittleEndian(bytes, 8);
        double value = 0.0;
        std::memcpy(&value, &raw, sizeof(value));
        return QString::number(value, 'g', 15);
    }

    if (dataType == VisibleString) {
        QByteArray text = bytes;
        const int nullPos = text.indexOf('\0');
        if (nullPos >= 0) {
            text.truncate(nullPos);
        }
        return QStringLiteral("\"%1\"").arg(QString::fromLatin1(text));
    }

    if (dataType == UnicodeString) {
        QString text;
        for (int i = 0; i + 1 < bytes.size(); i += 2) {
            const char16_t unit = static_cast<char16_t>(static_cast<quint8>(bytes.at(i))
                                                        | (static_cast<quint8>(bytes.at(i + 1)) << 8));
            if (unit == 0) {
                break;
            }
            text.append(QChar(unit));
        }
        return QStringLiteral("\"%1\"").arg(text);
    }

    // OCTET_STRING, DOMAIN ve bilinmeyen tipler hex olarak gösterilir.
    QStringList parts;
    for (const char byte : bytes) {
        parts << QStringLiteral("%1").arg(static_cast<uint>(static_cast<quint8>(byte)), 2, 16, QLatin1Char('0')).toUpper();
    }
    return parts.join(QLatin1Char(' '));
}

std::optional<QByteArray> EncodeValue(quint16 dataType, const QString &text, QString &errorText)
{
    using namespace CanOpenDataType;
    const QString trimmed = text.trimmed();

    if (dataType == Boolean) {
        const QString lower = trimmed.toLower();
        if (lower == QLatin1String("1") || lower == QLatin1String("true")) {
            return QByteArray(1, '\x01');
        }
        if (lower == QLatin1String("0") || lower == QLatin1String("false")) {
            return QByteArray(1, '\x00');
        }
        errorText = QCoreApplication::translate("ObjectDictionary", "BOOLEAN için 0/1 veya TRUE/FALSE girin.");
        return std::nullopt;
    }

    if (IsUnsignedIntegerType(dataType) || IsSignedIntegerType(dataType)) {
        const int size = DataTypeSize(dataType);
        const int bits = size * 8;
        const quint64 unsignedMax = bits >= 64 ? std::numeric_limits<quint64>::max() : ((1ULL << bits) - 1);

        bool ok = false;
        if (IsSignedIntegerType(dataType)) {
            const qint64 value = trimmed.toLongLong(&ok, 0);
            const qint64 minValue = bits >= 64 ? std::numeric_limits<qint64>::min() : -(1LL << (bits - 1));
            const qint64 maxValue = bits >= 64 ? std::numeric_limits<qint64>::max() : ((1LL << (bits - 1)) - 1);
            if (ok && value >= minValue && value <= maxValue) {
                return WriteLittleEndian(static_cast<quint64>(value), size);
            }
            // Hex ile girilen ham değerlere (ör. 0xFFFF) izin ver.
            if (!trimmed.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
                errorText = QCoreApplication::translate("ObjectDictionary", "%1 için geçerli aralık %2 .. %3.")
                                .arg(DataTypeName(dataType)).arg(minValue).arg(maxValue);
                return std::nullopt;
            }
        }

        const quint64 value = trimmed.toULongLong(&ok, 0);
        if (!ok || value > unsignedMax) {
            errorText = QCoreApplication::translate("ObjectDictionary", "%1 için geçerli bir değer girin (0 .. %2).")
                            .arg(DataTypeName(dataType)).arg(unsignedMax);
            return std::nullopt;
        }
        return WriteLittleEndian(value, size);
    }

    if (dataType == Real32 || dataType == Real64) {
        bool ok = false;
        const double value = trimmed.toDouble(&ok);
        if (!ok) {
            errorText = QCoreApplication::translate("ObjectDictionary", "Geçerli bir ondalık sayı girin (ör. 1.25).");
            return std::nullopt;
        }
        if (dataType == Real32) {
            const float single = static_cast<float>(value);
            quint32 raw = 0;
            std::memcpy(&raw, &single, sizeof(raw));
            return WriteLittleEndian(raw, 4);
        }
        quint64 raw = 0;
        std::memcpy(&raw, &value, sizeof(raw));
        return WriteLittleEndian(raw, 8);
    }

    if (dataType == VisibleString) {
        return text.toLatin1();
    }

    if (dataType == UnicodeString) {
        QByteArray bytes;
        for (const QChar ch : text) {
            bytes.append(static_cast<char>(ch.unicode() & 0xFF));
            bytes.append(static_cast<char>((ch.unicode() >> 8) & 0xFF));
        }
        return bytes;
    }

    // OCTET_STRING, DOMAIN veya bilinmeyen tip: hex bayt dizisi beklenir.
    QByteArray bytes;
    if (!ParseHexBytes(trimmed, bytes) || bytes.isEmpty()) {
        errorText = QCoreApplication::translate("ObjectDictionary", "Hex bayt dizisi girin (ör. 01 02 0A).");
        return std::nullopt;
    }
    return bytes;
}

std::optional<quint64> ParseEdsNumber(const QString &text, quint8 nodeId)
{
    QString cleaned = text.trimmed();
    cleaned.remove(QLatin1Char(' '));
    if (cleaned.isEmpty()) {
        return std::nullopt;
    }

    quint64 sum = 0;
    const QStringList terms = cleaned.split(QLatin1Char('+'), Qt::SkipEmptyParts);
    if (terms.isEmpty()) {
        return std::nullopt;
    }
    for (const QString &term : terms) {
        if (term.compare(QLatin1String("$NODEID"), Qt::CaseInsensitive) == 0) {
            sum += nodeId;
            continue;
        }
        bool ok = false;
        const quint64 value = term.toULongLong(&ok, 0);
        if (ok) {
            sum += value;
            continue;
        }
        const qint64 signedValue = term.toLongLong(&ok, 0);
        if (!ok) {
            return std::nullopt;
        }
        sum += static_cast<quint64>(signedValue);
    }
    return sum;
}
