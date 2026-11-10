#ifndef OBJECTDICTIONARY_H
#define OBJECTDICTIONARY_H

#include <QByteArray>
#include <QHash>
#include <QMap>
#include <QPair>
#include <QString>
#include <QVector>

#include <optional>

/// CiA 301 standart veri tipi kodları (EDS "DataType" alanı).
namespace CanOpenDataType {
constexpr quint16 Boolean = 0x0001;
constexpr quint16 Integer8 = 0x0002;
constexpr quint16 Integer16 = 0x0003;
constexpr quint16 Integer32 = 0x0004;
constexpr quint16 Unsigned8 = 0x0005;
constexpr quint16 Unsigned16 = 0x0006;
constexpr quint16 Unsigned32 = 0x0007;
constexpr quint16 Real32 = 0x0008;
constexpr quint16 VisibleString = 0x0009;
constexpr quint16 OctetString = 0x000A;
constexpr quint16 UnicodeString = 0x000B;
constexpr quint16 Domain = 0x000F;
constexpr quint16 Integer24 = 0x0010;
constexpr quint16 Real64 = 0x0011;
constexpr quint16 Integer40 = 0x0012;
constexpr quint16 Integer48 = 0x0013;
constexpr quint16 Integer56 = 0x0014;
constexpr quint16 Integer64 = 0x0015;
constexpr quint16 Unsigned24 = 0x0016;
constexpr quint16 Unsigned40 = 0x0018;
constexpr quint16 Unsigned48 = 0x0019;
constexpr quint16 Unsigned56 = 0x001A;
constexpr quint16 Unsigned64 = 0x001B;
} // namespace CanOpenDataType

/// CiA 306 nesne tipi kodları (EDS "ObjectType" alanı).
namespace CanOpenObjectType {
constexpr quint8 Null = 0x0;
constexpr quint8 DomainObject = 0x2;
constexpr quint8 DefType = 0x5;
constexpr quint8 DefStruct = 0x6;
constexpr quint8 Var = 0x7;
constexpr quint8 Array = 0x8;
constexpr quint8 Record = 0x9;
} // namespace CanOpenObjectType

/// Nesne sözlüğünde tek bir (index, subindex) girdisi.
struct OdEntry
{
    quint16 index = 0;
    quint8 subIndex = 0;
    QString name;
    quint16 dataType = 0;
    QString accessType;   ///< ro, wo, rw, rwr, rww, const (küçük harf)
    QString defaultValue; ///< EDS'teki ham metin; $NODEID içerebilir
    QString parameterValue; ///< DCF'teki "ParameterValue" (cihaza yazılacak ayar); EDS'te boş
    QString lowLimit;
    QString highLimit;
    bool pdoMapping = false;
};

/// Nesne sözlüğünde bir index'e karşılık gelen nesne (VAR, ARRAY, RECORD).
struct OdObject
{
    quint16 index = 0;
    QString name;
    quint8 objectType = CanOpenObjectType::Var;
    QVector<OdEntry> entries; ///< VAR için tek girdi (sub 0); ARRAY/RECORD için alt girdiler
};

/**
 * @brief EDS dosyasından oluşturulan CANopen nesne sözlüğü.
 *
 * Nesneler index'e göre sıralı tutulur. [FileInfo] ve [DeviceInfo]
 * bölümleri küçük harfli anahtarlarla saklanır.
 */
class ObjectDictionary
{
public:
    /// Bir bilgi bölümünün (anahtar, değer) satırları; dosyadaki sıra ve yazımla.
    using InfoEntries = QVector<QPair<QString, QString>>;

    /// Tüm nesneleri ve bilgi bölümlerini temizler.
    void Clear();

    /// Sözlükte hiç nesne yoksa true döndürür.
    bool IsEmpty() const;

    /// Nesneyi ekler; aynı index varsa üzerine yazar.
    void InsertObject(const OdObject &object);

    /**
     * @brief Index'e göre nesne arar.
     * @return Bulunamazsa nullptr.
     */
    const OdObject *FindObject(quint16 index) const;

    /**
     * @brief (index, subindex) çiftine göre girdi arar.
     * @return Bulunamazsa nullptr.
     */
    const OdEntry *FindEntry(quint16 index, quint8 subIndex) const;

    /// Tüm nesneleri index sırasıyla döndürür.
    const QMap<quint16, OdObject> &Objects() const;

    /// [FileInfo] bölümünü ayarlar (anahtarlar küçük harf).
    void SetFileInfo(const QHash<QString, QString> &info);

    /// [DeviceInfo] bölümünü ayarlar (anahtarlar küçük harf).
    void SetDeviceInfo(const QHash<QString, QString> &info);

    /// [DeviceInfo] içinden bir değeri döndürür (ör. "productname").
    QString DeviceInfoValue(const QString &key) const;

    /// [FileInfo] içinden bir değeri döndürür (ör. "filename").
    QString FileInfoValue(const QString &key) const;

    /// [FileInfo] satırlarını dosyadaki sıra ve yazımla ayarlar (görüntüleme için).
    void SetFileInfoEntries(const InfoEntries &entries);

    /// [FileInfo] satırlarını dosyadaki sıra ve yazımla döndürür.
    const InfoEntries &FileInfoEntries() const;

    /// [DeviceInfo] satırlarını dosyadaki sıra ve yazımla ayarlar (görüntüleme için).
    void SetDeviceInfoEntries(const InfoEntries &entries);

    /// [DeviceInfo] satırlarını dosyadaki sıra ve yazımla döndürür.
    const InfoEntries &DeviceInfoEntries() const;

    /// DCF [DeviceComissioning] satırlarını ayarlar (NodeID, Baudrate ...).
    void SetCommissioningEntries(const InfoEntries &entries);

    /// DCF [DeviceComissioning] satırlarını döndürür; EDS dosyasında boştur.
    const InfoEntries &CommissioningEntries() const;

    /**
     * @brief [DeviceComissioning] içinden bir değeri büyük/küçük harf duyarsız arar.
     * @param key Anahtar (ör. "NodeID").
     * @return Bulunamazsa boş metin.
     */
    QString CommissioningValue(const QString &key) const;

    /// Herhangi bir girdide ParameterValue varsa (dosya DCF ise) true döndürür.
    bool HasParameterValues() const;

    /// Sözlüğün yüklendiği dosya yolunu ayarlar.
    void SetFilePath(const QString &filePath);

    /// Sözlüğün yüklendiği dosya yolunu döndürür.
    QString FilePath() const;

private:
    QMap<quint16, OdObject> m_objects;
    QHash<QString, QString> m_fileInfo;
    QHash<QString, QString> m_deviceInfo;
    InfoEntries m_fileInfoEntries;
    InfoEntries m_deviceInfoEntries;
    InfoEntries m_commissioningEntries;
    QString m_filePath;
};

/// Veri tipi kodunun okunabilir adını döndürür (ör. "UNSIGNED16").
QString DataTypeName(quint16 dataType);

/// Nesne tipi kodunun okunabilir adını döndürür (ör. "RECORD").
QString ObjectTypeName(quint8 objectType);

/**
 * @brief Veri tipinin bayt cinsinden sabit boyutunu döndürür.
 * @return Sabit boyut; değişken uzunluklu tipler (string, domain) için 0.
 */
int DataTypeSize(quint16 dataType);

/// Veri tipi işaretli tamsayı ise true döndürür.
bool IsSignedIntegerType(quint16 dataType);

/// Veri tipi işaretsiz tamsayı ise true döndürür.
bool IsUnsignedIntegerType(quint16 dataType);

/**
 * @brief Little-endian ham baytları veri tipine göre okunabilir metne çevirir.
 * @param dataType CiA 301 veri tipi kodu.
 * @param bytes Ham veri (CANopen'da little-endian).
 * @param withHex true ise tamsayılar "123 (0x7B)" biçiminde gösterilir.
 */
QString DecodeValue(quint16 dataType, const QByteArray &bytes, bool withHex = true);

/**
 * @brief Kullanıcının girdiği metni veri tipine göre little-endian baytlara çevirir.
 * @param dataType CiA 301 veri tipi kodu; bilinmiyorsa metin hex bayt olarak yorumlanır.
 * @param text Değer metni (ondalık, "0x" önekli hex, TRUE/FALSE, string ...).
 * @param errorText Hata durumunda açıklama.
 * @return Başarılıysa kodlanmış baytlar, değilse std::nullopt.
 */
std::optional<QByteArray> EncodeValue(quint16 dataType, const QString &text, QString &errorText);

/**
 * @brief EDS sayısal değerini çözer; "$NODEID+0x180" gibi ifadeleri destekler.
 * @param text EDS'teki ham değer (ondalık, 0x hex veya 0 önekli sekizlik).
 * @param nodeId $NODEID yerine konacak düğüm numarası.
 * @return Çözülemezse std::nullopt.
 */
std::optional<quint64> ParseEdsNumber(const QString &text, quint8 nodeId = 0);

#endif // OBJECTDICTIONARY_H
