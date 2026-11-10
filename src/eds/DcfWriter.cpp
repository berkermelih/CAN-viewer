#include "DcfWriter.h"

#include "ObjectDictionary.h"

#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStringList>

#include <cstring>

namespace DcfWriter {

namespace {

// Çeviri bağlamı için kısa yardımcı.
QString Tr(const char *text)
{
    return QCoreApplication::translate("DcfWriter", text);
}

// Little-endian baytlardan en fazla 8 baytlık işaretsiz değer okur.
quint64 ReadLittleEndian(const QByteArray &bytes, int size)
{
    quint64 value = 0;
    for (int i = 0; i < size && i < bytes.size() && i < 8; ++i) {
        value |= static_cast<quint64>(static_cast<quint8>(bytes.at(i))) << (8 * i);
    }
    return value;
}

// Satırın "Anahtar=" ile başlayıp başlamadığını büyük/küçük harf duyarsız kontrol eder.
bool IsKeyLine(const QString &line, const QString &key)
{
    const int separator = line.indexOf(QLatin1Char('='));
    return separator > 0 && line.left(separator).trimmed().compare(key, Qt::CaseInsensitive) == 0;
}

// Bölümün son dolu satırından sonrasına (sondaki boş satırlardan önce) satır ekler.
void InsertAtSectionEnd(QStringList &out, int sectionStart, const QString &line)
{
    int position = static_cast<int>(out.size());
    while (position > sectionStart + 1 && out.at(position - 1).trimmed().isEmpty()) {
        --position;
    }
    out.insert(position, line);
}

} // namespace

quint32 ValueKey(quint16 index, quint8 subIndex)
{
    return (static_cast<quint32>(index) << 8) | subIndex;
}

bool IsSupportedType(quint16 dataType)
{
    return dataType == CanOpenDataType::Boolean || IsSignedIntegerType(dataType) || IsUnsignedIntegerType(dataType)
           || dataType == CanOpenDataType::Real32 || dataType == CanOpenDataType::Real64
           || dataType == CanOpenDataType::VisibleString;
}

QString FormatParameterValue(quint16 dataType, const QByteArray &bytes)
{
    if (dataType == CanOpenDataType::VisibleString) {
        const int end = bytes.indexOf('\0');
        return QString::fromLatin1(end >= 0 ? bytes.left(end) : bytes);
    }
    const int size = DataTypeSize(dataType);
    if (size <= 0 || bytes.size() < size) {
        return QString();
    }
    if (dataType == CanOpenDataType::Boolean) {
        return bytes.at(0) != 0 ? QStringLiteral("1") : QStringLiteral("0");
    }
    if (dataType == CanOpenDataType::Real32) {
        const quint32 raw = static_cast<quint32>(ReadLittleEndian(bytes, 4));
        float value = 0.0f;
        std::memcpy(&value, &raw, sizeof(value));
        return QString::number(static_cast<double>(value), 'g', 9);
    }
    if (dataType == CanOpenDataType::Real64) {
        const quint64 raw = ReadLittleEndian(bytes, 8);
        double value = 0.0;
        std::memcpy(&value, &raw, sizeof(value));
        return QString::number(value, 'g', 17);
    }
    const quint64 raw = ReadLittleEndian(bytes, size);
    if (IsSignedIntegerType(dataType)) {
        const int bits = size * 8;
        qint64 value = static_cast<qint64>(raw);
        if (bits < 64 && (raw & (1ULL << (bits - 1)))) {
            value = static_cast<qint64>(raw | (~0ULL << bits)); // işaret genişletme
        }
        return QString::number(value);
    }
    if (IsUnsignedIntegerType(dataType)) {
        return QStringLiteral("0x%1").arg(QString::number(raw, 16).toUpper());
    }
    return QString();
}

bool WriteDcf(const QString &sourcePath, const QString &targetPath, const QHash<quint32, QString> &values, quint8 nodeId,
              QString &errorText)
{
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        errorText = Tr("Kaynak EDS okunamadı: %1").arg(source.errorString());
        return false;
    }
    const QByteArray raw = source.readAll();
    source.close();
    QString text = QString::fromUtf8(raw);
    const bool utf8 = !text.contains(QChar::ReplacementCharacter);
    if (!utf8) {
        text = QString::fromLatin1(raw);
    }
    const QString eol = raw.contains("\r\n") ? QStringLiteral("\r\n") : QStringLiteral("\n");
    QStringList lines = text.split(QLatin1Char('\n'));
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }

    static const QRegularExpression sectionPattern(QStringLiteral("^\\s*\\[([^\\]]+)\\]\\s*$"));
    static const QRegularExpression objectPattern(QStringLiteral("^([0-9a-f]{4})$"));
    static const QRegularExpression subPattern(QStringLiteral("^([0-9a-f]{4})sub([0-9a-f]{1,2})$"));

    // Alt bölümü olan index'ler ARRAY/RECORD'dur; "[xxxx]" başlığı bir girdi değildir.
    QSet<quint16> complexIndexes;
    for (const QString &line : std::as_const(lines)) {
        const QRegularExpressionMatch match = sectionPattern.match(line);
        if (match.hasMatch()) {
            const QRegularExpressionMatch sub = subPattern.match(match.captured(1).trimmed().toLower());
            if (sub.hasMatch()) {
                complexIndexes.insert(static_cast<quint16>(sub.captured(1).toUInt(nullptr, 16)));
            }
        }
    }

    const QString nodeLine = QStringLiteral("NodeID=0x%1").arg(QString::number(nodeId, 16).toUpper());
    QStringList out;
    int sectionStart = -1;
    qint64 currentKey = -1;
    bool currentWritten = false;
    bool inCommissioning = false;
    bool commissioningSeen = false;
    bool nodeWritten = false;
    bool currentIsContainer = false; // ObjectType ARRAY/RECORD olan başlık bölümü (değer taşımaz)

    // Bölüm biterken eksik ParameterValue / NodeID satırlarını ekler.
    const auto finishSection = [&]() {
        if (currentKey >= 0 && !currentWritten && !currentIsContainer && values.contains(static_cast<quint32>(currentKey))) {
            InsertAtSectionEnd(out, sectionStart,
                               QStringLiteral("ParameterValue=") + values.value(static_cast<quint32>(currentKey)));
        }
        if (inCommissioning && !nodeWritten) {
            InsertAtSectionEnd(out, sectionStart, nodeLine);
        }
    };

    for (const QString &line : std::as_const(lines)) {
        const QRegularExpressionMatch match = sectionPattern.match(line);
        if (match.hasMatch()) {
            finishSection();
            const QString name = match.captured(1).trimmed().toLower();
            currentKey = -1;
            currentWritten = false;
            currentIsContainer = false;
            inCommissioning = name == QLatin1String("devicecomissioning") || name == QLatin1String("devicecommissioning");
            commissioningSeen = commissioningSeen || inCommissioning;
            nodeWritten = false;
            const QRegularExpressionMatch object = objectPattern.match(name);
            const QRegularExpressionMatch sub = subPattern.match(name);
            if (object.hasMatch()) {
                const quint16 index = static_cast<quint16>(object.captured(1).toUInt(nullptr, 16));
                if (!complexIndexes.contains(index)) {
                    currentKey = ValueKey(index, 0);
                }
            } else if (sub.hasMatch()) {
                currentKey = ValueKey(static_cast<quint16>(sub.captured(1).toUInt(nullptr, 16)),
                                      static_cast<quint8>(sub.captured(2).toUInt(nullptr, 16)));
            }
            sectionStart = static_cast<int>(out.size());
            out << line;
            continue;
        }
        if (IsKeyLine(line, QStringLiteral("ObjectType"))) {
            const auto type = ParseEdsNumber(line.mid(line.indexOf(QLatin1Char('=')) + 1).trimmed());
            currentIsContainer = type && (*type == CanOpenObjectType::Array || *type == CanOpenObjectType::Record);
        }
        if (currentKey >= 0 && !currentIsContainer && values.contains(static_cast<quint32>(currentKey))
            && IsKeyLine(line, QStringLiteral("ParameterValue"))) {
            out << QStringLiteral("ParameterValue=") + values.value(static_cast<quint32>(currentKey));
            currentWritten = true;
            continue;
        }
        if (inCommissioning && IsKeyLine(line, QStringLiteral("NodeID"))) {
            out << nodeLine;
            nodeWritten = true;
            continue;
        }
        out << line;
    }
    finishSection();

    if (!commissioningSeen) {
        while (!out.isEmpty() && out.last().trimmed().isEmpty()) {
            out.removeLast();
        }
        out << QString() << QStringLiteral("[DeviceComissioning]") << nodeLine << QString();
    }

    QSaveFile target(targetPath);
    if (!target.open(QIODevice::WriteOnly)) {
        errorText = target.errorString();
        return false;
    }
    const QString result = out.join(eol);
    target.write(utf8 ? result.toUtf8() : result.toLatin1());
    if (!target.commit()) {
        errorText = target.errorString();
        return false;
    }
    return true;
}

} // namespace DcfWriter
