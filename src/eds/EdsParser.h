#ifndef EDSPARSER_H
#define EDSPARSER_H

#include "ObjectDictionary.h"

#include <QHash>
#include <QString>
#include <QStringList>

#include <optional>

/**
 * @brief CiA 306 EDS (Electronic Data Sheet) dosya ayrıştırıcısı.
 *
 * EDS dosyası INI biçimindedir. Bölüm ve anahtar adları büyük/küçük harf
 * duyarsız okunur. Desteklenenler: [FileInfo], [DeviceInfo], nesne bölümleri
 * ([1018], [1018sub1] ...) ve CompactSubObj ile tanımlanan diziler
 * ([xxxxName] / [xxxxValue] bölümleri dahil).
 */
class EdsParser
{
public:
    /**
     * @brief Verilen yoldaki EDS dosyasını okuyup nesne sözlüğünü oluşturur.
     * @param filePath Açılacak .eds dosyasının tam yolu.
     * @param errorText Dosya okunamaz veya hiç nesne yoksa açıklama yazılır.
     * @param warnings Ayrıştırmayı durdurmayan uyarılar (eksik zorunlu nesne vb.).
     * @return Başarılıysa nesne sözlüğü, hata durumunda std::nullopt.
     */
    static std::optional<ObjectDictionary> OpenEdsFile(const QString &filePath, QString &errorText,
                                                       QStringList &warnings);

private:
    using IniSection = QHash<QString, QString>;
    using IniDocument = QHash<QString, IniSection>;

    /**
     * @brief INI dosyasını bölüm -> (anahtar -> değer) yapısına okur.
     * @param filePath Dosya yolu.
     * @param document Okunan içerik; bölüm ve anahtar adları küçük harfe çevrilir.
     * @param errorText Dosya açılamazsa açıklama.
     * @return Dosya okunduysa true.
     */
    static bool ReadIniDocument(const QString &filePath, IniDocument &document, QString &errorText);

    /**
     * @brief Bir nesne/alt nesne bölümünden OdEntry oluşturur.
     * @param section Bölüm içeriği.
     * @param index Nesne index'i.
     * @param subIndex Alt index.
     */
    static OdEntry ParseEntry(const IniSection &section, quint16 index, quint8 subIndex);

    /**
     * @brief CompactSubObj ile tanımlanmış bir dizinin alt girdilerini üretir.
     * @param document Tüm EDS içeriği ([xxxxName] ve [xxxxValue] için).
     * @param section Ana nesne bölümü.
     * @param object Girdilerin ekleneceği nesne.
     * @param count Alt girdi sayısı (CompactSubObj değeri).
     */
    static void ExpandCompactSubObjects(const IniDocument &document, const IniSection &section,
                                        OdObject &object, int count);
};

#endif // EDSPARSER_H
