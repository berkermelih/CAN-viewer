#ifndef CANFRAME_H
#define CANFRAME_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

#include <array>

/**
 * @brief Klasik CAN (CAN 2.0A / 2.0B) çerçevesi.
 *
 * Donanımdan bağımsız ortak çerçeve yapısıdır; sürücü, CANopen ve model
 * katmanlarının tümü bu yapı üzerinden çalışır. CAN FD bu sürümde desteklenmez.
 */
struct CanFrame
{
    quint32 id = 0;                ///< CAN tanımlayıcısı (11 veya 29 bit)
    bool extended = false;         ///< true ise 29 bit (extended) ID
    bool rtr = false;              ///< Remote Transmission Request çerçevesi
    quint8 dlc = 0;                ///< Veri uzunluğu (0..8)
    std::array<quint8, 8> data {}; ///< Veri baytları; yalnızca ilk dlc bayt geçerlidir
    qint64 timestampUs = 0;        ///< Alış zamanı (bağlantı/ilk çerçeveden itibaren mikrosaniye)
};

/// Standart (11 bit) CAN ID'nin alabileceği en büyük değer.
constexpr quint32 MaxStandardCanId = 0x7FF;
/// Extended (29 bit) CAN ID'nin alabileceği en büyük değer.
constexpr quint32 MaxExtendedCanId = 0x1FFFFFFF;

/**
 * @brief CAN ID'yi büyük harfli hex metne çevirir.
 * @param id CAN tanımlayıcısı.
 * @param extended true ise 8 haneli, değilse 3 haneli gösterilir.
 * @return Örn. "1A0" veya "18FF0001".
 */
QString FormatCanId(quint32 id, bool extended);

/**
 * @brief Çerçevenin geçerli veri baytlarını "11 22 AA" biçiminde metne çevirir.
 * @param frame Biçimlendirilecek çerçeve.
 * @return Veri metni; RTR çerçevelerde "RTR".
 */
QString FormatFrameData(const CanFrame &frame);

/**
 * @brief "11 22 AA" veya "1122AA" biçimindeki hex metni bayt dizisine çevirir.
 * @param text Çözümlenecek metin; boşluk, virgül ve "0x" önekleri yok sayılır.
 * @param bytes Başarılıysa çözülen baytlar buraya yazılır.
 * @return Metin geçerli hex ise true.
 */
bool ParseHexBytes(const QString &text, QByteArray &bytes);

/**
 * @brief Hex metni (ör. "1A0" veya "0x1A0") 32 bit sayıya çevirir.
 * @param text Çözümlenecek metin.
 * @param value Başarılıysa sonuç buraya yazılır.
 * @return Metin geçerli bir 32 bit hex sayı ise true.
 */
bool ParseHexNumber(const QString &text, quint32 &value);

#endif // CANFRAME_H
