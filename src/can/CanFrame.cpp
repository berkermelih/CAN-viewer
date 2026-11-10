#include "CanFrame.h"

#include <QStringList>

namespace {

// CAN FD DLC kodlarının (9..15) karşılık geldiği uzunluklar.
constexpr int FdLengths[] = {12, 16, 20, 24, 32, 48, 64};

} // namespace

// CAN ID'yi sabit genişlikli büyük harfli hex veya onluk metne çevirir.
QString FormatCanId(quint32 id, bool extended, IdFormat format)
{
    if (format == IdFormat::Decimal) {
        return QString::number(id);
    }
    return QStringLiteral("%1").arg(id, extended ? 8 : 3, 16, QLatin1Char('0')).toUpper();
}

// Çerçevenin veri baytlarını seçilen biçimde metne çevirir.
QString FormatFrameData(const CanFrame &frame, DataFormat format)
{
    if (frame.rtr) {
        return QStringLiteral("RTR");
    }

    const int count = qMin<int>(frame.dlc, static_cast<int>(frame.data.size()));
    if (format == DataFormat::Ascii) {
        QString text;
        text.reserve(count);
        for (int i = 0; i < count; ++i) {
            const quint8 byte = frame.data[static_cast<size_t>(i)];
            text.append(byte >= 0x20 && byte < 0x7F ? QChar(byte) : QLatin1Char('.'));
        }
        return text;
    }

    QStringList parts;
    parts.reserve(count);
    for (int i = 0; i < count; ++i) {
        const uint byte = frame.data[static_cast<size_t>(i)];
        if (format == DataFormat::Decimal) {
            parts << QStringLiteral("%1").arg(byte, 3, 10, QLatin1Char(' '));
        } else {
            parts << QStringLiteral("%1").arg(byte, 2, 16, QLatin1Char('0')).toUpper();
        }
    }
    return parts.join(QLatin1Char(' '));
}

// Çerçeve tipini "Std/Ext" + "RTR" / "FD [BRS] [ESI]" biçiminde döndürür.
QString FormatFrameType(const CanFrame &frame)
{
    QString text = frame.extended ? QStringLiteral("Ext") : QStringLiteral("Std");
    if (frame.fd) {
        text += QStringLiteral(" FD");
        if (frame.brs) {
            text += QStringLiteral(" BRS");
        }
        if (frame.esi) {
            text += QStringLiteral(" ESI");
        }
    } else if (frame.rtr) {
        text += QStringLiteral(" RTR");
    }
    return text;
}

// Hex metni bayt dizisine çevirir; her parça 1-2 hane veya çift sayıda haneden oluşmalıdır.
bool ParseHexBytes(const QString &text, QByteArray &bytes)
{
    QString cleaned = text;
    cleaned.replace(QLatin1String("0x"), QString(), Qt::CaseInsensitive);
    cleaned.replace(QLatin1Char(','), QLatin1Char(' '));
    cleaned.replace(QLatin1Char(';'), QLatin1Char(' '));

    QByteArray result;
    const QStringList tokens = cleaned.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        if (token.size() > 2 && token.size() % 2 != 0) {
            return false;
        }
        const int step = token.size() <= 2 ? token.size() : 2;
        for (int pos = 0; pos < token.size(); pos += step) {
            bool ok = false;
            const uint value = token.mid(pos, step).toUInt(&ok, 16);
            if (!ok || value > 0xFF) {
                return false;
            }
            result.append(static_cast<char>(value));
        }
    }

    bytes = result;
    return true;
}

// Hex metni 32 bit sayıya çevirir.
bool ParseHexNumber(const QString &text, quint32 &value)
{
    QString trimmed = text.trimmed();
    if (trimmed.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
        trimmed = trimmed.mid(2);
    }
    if (trimmed.isEmpty()) {
        return false;
    }

    bool ok = false;
    const qulonglong parsed = trimmed.toULongLong(&ok, 16);
    if (!ok || parsed > 0xFFFFFFFFULL) {
        return false;
    }
    value = static_cast<quint32>(parsed);
    return true;
}

// CAN FD DLC kodunu bayt uzunluğuna çevirir.
int CanFdDlcToLength(quint8 dlcCode)
{
    if (dlcCode <= 8) {
        return dlcCode;
    }
    return dlcCode >= 15 ? 64 : FdLengths[dlcCode - 9];
}

// Bayt uzunluğunu CAN FD DLC koduna çevirir (gerekirse yukarı yuvarlar).
quint8 CanFdLengthToDlc(int length)
{
    if (length <= 8) {
        return static_cast<quint8>(qMax(0, length));
    }
    for (int i = 0; i < static_cast<int>(sizeof(FdLengths) / sizeof(FdLengths[0])); ++i) {
        if (length <= FdLengths[i]) {
            return static_cast<quint8>(9 + i);
        }
    }
    return 15;
}

// Uzunluğun geçerli bir CAN FD uzunluğu olup olmadığını döndürür.
bool IsValidCanFdLength(int length)
{
    return length >= 0 && length <= MaxCanFdDataLength && CanFdDlcToLength(CanFdLengthToDlc(length)) == length;
}
