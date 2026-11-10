#ifndef CANFRAME_H
#define CANFRAME_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

#include <array>

/// CAN FD çerçevesinin taşıyabileceği en fazla veri baytı.
constexpr int MaxCanFdDataLength = 64;
/// Klasik CAN (CAN CC) çerçevesinin taşıyabileceği en fazla veri baytı.
constexpr int MaxClassicDataLength = 8;

/**
 * @brief Klasik CAN (CAN CC, 2.0A/2.0B) veya CAN FD çerçevesi.
 *
 * Donanımdan bağımsız ortak çerçeve yapısıdır; sürücü, CANopen ve model
 * katmanlarının tümü bu yapı üzerinden çalışır. @c dlc alanı DLC kodunu değil,
 * bayt cinsinden veri uzunluğunu tutar (CAN FD'de 0..8, 12, 16, 20, 24, 32, 48, 64).
 */
struct CanFrame
{
    quint32 id = 0;                                ///< CAN tanımlayıcısı (11 veya 29 bit)
    bool extended = false;                         ///< true ise 29 bit (extended) ID
    bool rtr = false;                              ///< Remote Transmission Request (yalnızca CAN CC)
    bool fd = false;                               ///< CAN FD çerçevesi
    bool brs = false;                              ///< Bit Rate Switch (yalnızca CAN FD)
    bool esi = false;                              ///< Error State Indicator (yalnızca CAN FD, alımda)
    quint8 dlc = 0;                                ///< Veri uzunluğu (bayt)
    std::array<quint8, MaxCanFdDataLength> data {}; ///< Veri baytları; yalnızca ilk dlc bayt geçerlidir
    qint64 timestampUs = 0;                        ///< Alış zamanı (bağlantı/ilk çerçeveden itibaren mikrosaniye)
};

/// Standart (11 bit) CAN ID'nin alabileceği en büyük değer.
constexpr quint32 MaxStandardCanId = 0x7FF;
/// Extended (29 bit) CAN ID'nin alabileceği en büyük değer.
constexpr quint32 MaxExtendedCanId = 0x1FFFFFFF;

/// Tablolarda CAN ID gösterim biçimi.
enum class IdFormat
{
    Hex,    ///< Onaltılık (varsayılan)
    Decimal ///< Onluk
};

/// Tablolarda veri baytları gösterim biçimi.
enum class DataFormat
{
    Hex,     ///< Onaltılık (varsayılan)
    Decimal, ///< Onluk
    Ascii    ///< Yazdırılabilir karakterler; diğerleri '.'
};

/**
 * @brief CAN ID'yi metne çevirir.
 * @param id CAN tanımlayıcısı.
 * @param extended Hex biçimde true ise 8 haneli, değilse 3 haneli gösterilir.
 * @param format Hex veya onluk.
 * @return Örn. "1A0", "18FF0001" veya onluk "416".
 */
QString FormatCanId(quint32 id, bool extended, IdFormat format = IdFormat::Hex);

/**
 * @brief Çerçevenin geçerli veri baytlarını metne çevirir.
 * @param frame Biçimlendirilecek çerçeve.
 * @param format Hex ("11 22 AA"), onluk ("17 34 170") veya ASCII.
 * @return Veri metni; RTR çerçevelerde "RTR".
 */
QString FormatFrameData(const CanFrame &frame, DataFormat format = DataFormat::Hex);

/**
 * @brief Çerçeve tipini kısa metin olarak döndürür.
 * @param frame Çerçeve.
 * @return Örn. "Std", "Ext RTR", "Std FD", "Ext FD BRS".
 */
QString FormatFrameType(const CanFrame &frame);

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

/**
 * @brief CAN FD DLC kodunu (0..15) bayt uzunluğuna çevirir.
 * @param dlcCode DLC kodu; 15'ten büyükse 64 kabul edilir.
 * @return 0..8, 12, 16, 20, 24, 32, 48 veya 64.
 */
int CanFdDlcToLength(quint8 dlcCode);

/**
 * @brief Bayt uzunluğunu CAN FD DLC koduna çevirir.
 * @param length Veri uzunluğu; geçerli FD uzunluğu değilse bir üstteki geçerli uzunluk alınır.
 * @return 0..15 arası DLC kodu.
 */
quint8 CanFdLengthToDlc(int length);

/**
 * @brief Uzunluğun geçerli bir CAN FD veri uzunluğu olup olmadığını döndürür.
 * @param length Bayt cinsinden uzunluk.
 */
bool IsValidCanFdLength(int length);

#endif // CANFRAME_H
