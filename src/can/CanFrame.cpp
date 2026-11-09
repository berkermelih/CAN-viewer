#include "CanFrame.h"

#include <QStringList>

// CAN ID'yi sabit genişlikli büyük harfli hex metne çevirir.
QString FormatCanId(quint32 id, bool extended)
{
    return QStringLiteral("%1").arg(id, extended ? 8 : 3, 16, QLatin1Char('0')).toUpper();
}

// Çerçevenin veri baytlarını boşlukla ayrılmış hex metne çevirir.
QString FormatFrameData(const CanFrame &frame)
{
    if (frame.rtr) {
        return QStringLiteral("RTR");
    }

    QStringList parts;
    const int count = qMin<int>(frame.dlc, static_cast<int>(frame.data.size()));
    for (int i = 0; i < count; ++i) {
        parts << QStringLiteral("%1")
                     .arg(static_cast<uint>(frame.data[static_cast<size_t>(i)]), 2, 16, QLatin1Char('0'))
                     .toUpper();
    }
    return parts.join(QLatin1Char(' '));
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
