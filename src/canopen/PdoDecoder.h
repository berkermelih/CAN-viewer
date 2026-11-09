#ifndef PDODECODER_H
#define PDODECODER_H

#include "../can/CanFrame.h"
#include "../eds/ObjectDictionary.h"

#include <QHash>
#include <QString>
#include <QVector>

/// PDO içine eşlenmiş tek bir nesne.
struct PdoMappedItem
{
    quint16 index = 0;
    quint8 subIndex = 0;
    quint8 bitLength = 0;
    quint16 dataType = 0;
    QString name;
};

/// EDS'ten çözülmüş bir PDO tanımı.
struct PdoDefinition
{
    quint32 cobId = 0;
    bool extended = false;
    bool transmit = true; ///< true: TPDO (cihaz gönderir), false: RPDO (cihaz alır)
    int number = 0;       ///< PDO numarası (1'den başlar)
    QVector<PdoMappedItem> items;
};

/**
 * @brief EDS'teki varsayılan PDO iletişim/eşleme parametrelerine göre PDO çözücü.
 *
 * 0x1400-0x15FF / 0x1600-0x17FF (RPDO) ve 0x1800-0x19FF / 0x1A00-0x1BFF (TPDO)
 * nesnelerinin DefaultValue alanları kullanılır. Cihazın çalışma zamanında
 * değiştirilmiş eşlemesi bilinmez; yalnızca EDS varsayılanları çözülür.
 */
class PdoDecoder
{
public:
    /**
     * @brief Nesne sözlüğünden PDO tanımlarını oluşturur.
     * @param dictionary Yüklenmiş EDS nesne sözlüğü.
     * @param nodeId $NODEID ifadelerinde kullanılacak düğüm numarası.
     */
    void BuildFromDictionary(const ObjectDictionary &dictionary, quint8 nodeId);

    /// Tüm PDO tanımlarını siler.
    void Clear();

    /// Hiç PDO tanımı yoksa true döndürür.
    bool IsEmpty() const;

    /// Tanımlı PDO sayısını döndürür.
    int Count() const;

    /**
     * @brief Çerçeve bilinen bir PDO ise eşlenmiş değerlerini metne çevirir.
     * @param frame Çözülecek çerçeve.
     * @return Örn. "TPDO1: Statusword=0x0237, Position=1200"; PDO değilse boş metin.
     */
    QString DecodeFrame(const CanFrame &frame) const;

private:
    /**
     * @brief Tek bir PDO'nun iletişim ve eşleme nesnelerini okuyup tanıma ekler.
     * @param dictionary Nesne sözlüğü.
     * @param nodeId Düğüm numarası.
     * @param communicationIndex İletişim parametresi index'i (ör. 0x1800).
     * @param mappingIndex Eşleme parametresi index'i (ör. 0x1A00).
     * @param transmit TPDO ise true.
     * @param number PDO numarası.
     */
    void AddPdo(const ObjectDictionary &dictionary, quint8 nodeId, quint16 communicationIndex,
                quint16 mappingIndex, bool transmit, int number);

    /// Çerçeve ID'si ve tipinden arama anahtarı üretir.
    static quint32 MakeKey(quint32 id, bool extended);

    QHash<quint32, PdoDefinition> m_definitions;
};

#endif // PDODECODER_H
