#include "CanOpenProtocol.h"

#include <QCoreApplication>

namespace CanOpen {

namespace {

// Çeviri bağlamıyla metin üretir.
QString Tr(const char *text)
{
    return QCoreApplication::translate("CanOpen", text);
}

// Çerçeve verisinden little-endian 16 bit değer okur.
quint16 ReadU16(const CanFrame &frame, int offset)
{
    return static_cast<quint16>(frame.data[static_cast<size_t>(offset)]
                                | (frame.data[static_cast<size_t>(offset + 1)] << 8));
}

// Çerçeve verisinden little-endian 32 bit değer okur.
quint32 ReadU32(const CanFrame &frame, int offset)
{
    quint32 value = 0;
    for (int i = 0; i < 4; ++i) {
        value |= static_cast<quint32>(frame.data[static_cast<size_t>(offset + i)]) << (8 * i);
    }
    return value;
}

// SDO çerçevesindeki "index:sub" bilgisini metne çevirir.
QString SdoAddressText(const CanFrame &frame)
{
    return QStringLiteral("0x")
           + QStringLiteral("%1:%2")
                 .arg(ReadU16(frame, 1), 4, 16, QLatin1Char('0'))
                 .arg(static_cast<uint>(frame.data[3]), 2, 16, QLatin1Char('0'))
                 .toUpper();
}

// SDO istemci isteğini (0x600 + node) yorumlar.
QString DescribeSdoRequest(const CanFrame &frame)
{
    const quint8 command = frame.data[0];
    switch (command >> 5) {
    case 1: return Tr("Yazma isteği %1").arg(SdoAddressText(frame));
    case 2: return Tr("Okuma isteği %1").arg(SdoAddressText(frame));
    case 0: return Tr("Yazma segmenti");
    case 3: return Tr("Okuma segment isteği");
    case 4: return Tr("Abort %1: %2").arg(SdoAddressText(frame), SdoAbortCodeText(ReadU32(frame, 4)));
    case 5: return Tr("Blok okuma");
    case 6: return Tr("Blok yazma");
    default: break;
    }
    return Tr("Bilinmeyen komut");
}

// SDO sunucu yanıtını (0x580 + node) yorumlar.
QString DescribeSdoResponse(const CanFrame &frame)
{
    const quint8 command = frame.data[0];
    switch (command >> 5) {
    case 2: return Tr("Okuma yanıtı %1").arg(SdoAddressText(frame));
    case 3: return Tr("Yazma onayı %1").arg(SdoAddressText(frame));
    case 0: return Tr("Okuma segmenti");
    case 1: return Tr("Yazma segment onayı");
    case 4: return Tr("Abort %1: %2").arg(SdoAddressText(frame), SdoAbortCodeText(ReadU32(frame, 4)));
    case 5: return Tr("Blok yazma yanıtı");
    case 6: return Tr("Blok okuma yanıtı");
    default: break;
    }
    return Tr("Bilinmeyen yanıt");
}

// EMCY hata kodunun üst hanesine göre hata grubunu döndürür.
QString EmergencyGroupName(quint16 code)
{
    if (code == 0x0000) {
        return Tr("Hata yok / reset");
    }
    switch (code >> 12) {
    case 0x1: return Tr("Genel hata");
    case 0x2: return Tr("Akım");
    case 0x3: return Tr("Gerilim");
    case 0x4: return Tr("Sıcaklık");
    case 0x5: return Tr("Cihaz donanımı");
    case 0x6: return Tr("Cihaz yazılımı");
    case 0x7: return Tr("Ek modüller");
    case 0x8: return Tr("İzleme / haberleşme");
    case 0x9: return Tr("Harici hata");
    case 0xF: return code >= 0xFF00 ? Tr("Cihaza özgü") : Tr("Ek fonksiyonlar");
    default: break;
    }
    return Tr("Bilinmeyen grup");
}

} // namespace

CanFrame BuildNmtFrame(NmtCommand command, quint8 nodeId)
{
    CanFrame frame;
    frame.id = 0x000;
    frame.dlc = 2;
    frame.data[0] = static_cast<quint8>(command);
    frame.data[1] = nodeId;
    return frame;
}

CanFrame BuildSyncFrame()
{
    CanFrame frame;
    frame.id = 0x080;
    frame.dlc = 0;
    return frame;
}

QString NmtCommandName(quint8 command)
{
    switch (command) {
    case 0x01: return QStringLiteral("Start");
    case 0x02: return QStringLiteral("Stop");
    case 0x80: return QStringLiteral("Pre-operational");
    case 0x81: return QStringLiteral("Reset node");
    case 0x82: return QStringLiteral("Reset communication");
    default: break;
    }
    return QStringLiteral("0x%1").arg(static_cast<uint>(command), 2, 16, QLatin1Char('0'));
}

QString NmtStateName(quint8 state)
{
    switch (state & 0x7F) {
    case 0x00: return QStringLiteral("Boot-up");
    case 0x04: return QStringLiteral("Stopped");
    case 0x05: return QStringLiteral("Operational");
    case 0x7F: return QStringLiteral("Pre-operational");
    default: break;
    }
    return QStringLiteral("0x%1").arg(static_cast<uint>(state), 2, 16, QLatin1Char('0'));
}

QString SdoAbortCodeText(quint32 code)
{
    switch (code) {
    case 0x05030000: return Tr("Toggle biti değişmedi");
    case 0x05040000: return Tr("SDO zaman aşımı");
    case 0x05040001: return Tr("Geçersiz komut");
    case 0x05040002: return Tr("Geçersiz blok boyutu");
    case 0x05040003: return Tr("Geçersiz sıra numarası");
    case 0x05040004: return Tr("CRC hatası");
    case 0x05040005: return Tr("Bellek yetersiz");
    case 0x06010000: return Tr("Nesneye bu erişim desteklenmiyor");
    case 0x06010001: return Tr("Salt yazılır nesne okunmaya çalışıldı");
    case 0x06010002: return Tr("Salt okunur nesneye yazılmaya çalışıldı");
    case 0x06020000: return Tr("Nesne sözlükte yok");
    case 0x06040041: return Tr("Nesne PDO'ya eşlenemez");
    case 0x06040042: return Tr("PDO uzunluğu aşıldı");
    case 0x06040043: return Tr("Genel parametre uyumsuzluğu");
    case 0x06040047: return Tr("Genel dahili uyumsuzluk");
    case 0x06060000: return Tr("Donanım hatası nedeniyle erişim başarısız");
    case 0x06070010: return Tr("Veri tipi / uzunluk uyuşmuyor");
    case 0x06070012: return Tr("Veri uzunluğu çok büyük");
    case 0x06070013: return Tr("Veri uzunluğu çok küçük");
    case 0x06090011: return Tr("Alt index yok");
    case 0x06090030: return Tr("Değer aralık dışında");
    case 0x06090031: return Tr("Değer çok büyük");
    case 0x06090032: return Tr("Değer çok küçük");
    case 0x06090036: return Tr("En büyük değer en küçükten küçük");
    case 0x060A0023: return Tr("Kaynak kullanılamıyor");
    case 0x08000000: return Tr("Genel hata");
    case 0x08000020: return Tr("Veri aktarılamıyor / saklanamıyor");
    case 0x08000021: return Tr("Yerel kontrol nedeniyle veri aktarılamıyor");
    case 0x08000022: return Tr("Cihaz durumu nedeniyle veri aktarılamıyor");
    case 0x08000023: return Tr("Nesne sözlüğü mevcut değil");
    case 0x08000024: return Tr("Veri yok");
    default: break;
    }
    return Tr("Bilinmeyen abort kodu 0x%1").arg(code, 8, 16, QLatin1Char('0'));
}

QString DescribeFrame(const CanFrame &frame)
{
    if (frame.extended || frame.id > MaxStandardCanId) {
        return QString();
    }

    const quint32 functionCode = frame.id & 0x780;
    const quint8 nodeId = static_cast<quint8>(frame.id & 0x7F);
    const QString nodeText = Tr("Node %1").arg(static_cast<uint>(nodeId));

    if (frame.id == 0x000) {
        if (frame.dlc >= 2) {
            const QString target = frame.data[1] == 0 ? Tr("tüm düğümler")
                                                      : Tr("Node %1").arg(static_cast<uint>(frame.data[1]));
            return Tr("NMT %1 → %2").arg(NmtCommandName(frame.data[0]), target);
        }
        return QStringLiteral("NMT");
    }
    if (frame.id == 0x080) {
        return frame.dlc == 0 ? QStringLiteral("SYNC") : Tr("SYNC (sayaç %1)").arg(static_cast<uint>(frame.data[0]));
    }
    if (frame.id == 0x100) {
        return QStringLiteral("TIME");
    }
    if (frame.id == 0x7E4) {
        return Tr("LSS yanıt");
    }
    if (frame.id == 0x7E5) {
        return Tr("LSS istek");
    }
    if (nodeId == 0) {
        return QString();
    }

    switch (functionCode) {
    case 0x080:
        if (frame.dlc >= 3) {
            const quint16 code = ReadU16(frame, 0);
            return Tr("EMCY %1: 0x%2 %3, reg 0x%4")
                .arg(nodeText)
                .arg(code, 4, 16, QLatin1Char('0'))
                .arg(EmergencyGroupName(code))
                .arg(static_cast<uint>(frame.data[2]), 2, 16, QLatin1Char('0'));
        }
        return Tr("EMCY %1").arg(nodeText);
    case 0x180: return Tr("TPDO1 %1").arg(nodeText);
    case 0x200: return Tr("RPDO1 %1").arg(nodeText);
    case 0x280: return Tr("TPDO2 %1").arg(nodeText);
    case 0x300: return Tr("RPDO2 %1").arg(nodeText);
    case 0x380: return Tr("TPDO3 %1").arg(nodeText);
    case 0x400: return Tr("RPDO3 %1").arg(nodeText);
    case 0x480: return Tr("TPDO4 %1").arg(nodeText);
    case 0x500: return Tr("RPDO4 %1").arg(nodeText);
    case 0x580:
        return frame.dlc >= 8 ? Tr("SDO yanıt %1: %2").arg(nodeText, DescribeSdoResponse(frame))
                              : Tr("SDO yanıt %1").arg(nodeText);
    case 0x600:
        return frame.dlc >= 8 ? Tr("SDO istek %1: %2").arg(nodeText, DescribeSdoRequest(frame))
                              : Tr("SDO istek %1").arg(nodeText);
    case 0x700:
        if (frame.rtr) {
            return Tr("Node guarding isteği %1").arg(nodeText);
        }
        if (frame.dlc >= 1) {
            return Tr("Heartbeat %1: %2").arg(nodeText, NmtStateName(frame.data[0]));
        }
        return Tr("Heartbeat %1").arg(nodeText);
    default:
        break;
    }
    return QString();
}

} // namespace CanOpen
