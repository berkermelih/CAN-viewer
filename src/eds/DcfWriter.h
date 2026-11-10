#ifndef DCFWRITER_H
#define DCFWRITER_H

#include <QByteArray>
#include <QHash>
#include <QString>

/**
 * @brief DCF (Device Configuration File, CiA 306) üretimi.
 *
 * DCF, EDS ile aynı INI biçimindedir; her nesneye cihazdaki gerçek değer
 * "ParameterValue" olarak, düğüm numarası da [DeviceComissioning] bölümüne
 * yazılır. Yazıcı kaynak EDS/DCF metnini satır satır korur; yalnızca bu
 * alanları ekler veya günceller. CompactSubObj ile tanımlanan nesnelere
 * değer yazılmaz.
 */
namespace DcfWriter {

/**
 * @brief (index, subindex) için değer tablosu anahtarı üretir.
 * @param index Nesne index'i.
 * @param subIndex Alt index.
 */
quint32 ValueKey(quint16 index, quint8 subIndex);

/**
 * @brief Veri tipinin DCF'e yazılabilir olup olmadığını döndürür.
 * @param dataType CiA 301 veri tipi kodu.
 * @return BOOLEAN, tamsayı, REAL32/64 ve VISIBLE_STRING için true; diğerleri (DOMAIN, OCTET_STRING ...) false.
 */
bool IsSupportedType(quint16 dataType);

/**
 * @brief Cihazdan okunan ham baytları DCF "ParameterValue" metnine çevirir.
 * @param dataType CiA 301 veri tipi kodu.
 * @param bytes Little-endian ham veri.
 * @return İşaretsiz tamsayılar "0x..." hex, işaretliler onluk, string düz metin; desteklenmiyorsa boş.
 */
QString FormatParameterValue(quint16 dataType, const QByteArray &bytes);

/**
 * @brief Kaynak EDS/DCF dosyasından, verilen değerlerle yeni bir DCF dosyası yazar.
 * @param sourcePath Yüklü EDS veya DCF dosyasının yolu.
 * @param targetPath Yazılacak DCF dosyası.
 * @param values ValueKey() anahtarlı ParameterValue metinleri.
 * @param nodeId [DeviceComissioning] NodeID değeri.
 * @param errorText Hata durumunda açıklama.
 * @return Dosya yazıldıysa true.
 */
bool WriteDcf(const QString &sourcePath, const QString &targetPath, const QHash<quint32, QString> &values, quint8 nodeId,
              QString &errorText);

} // namespace DcfWriter

#endif // DCFWRITER_H
