#ifndef TXLISTFILE_H
#define TXLISTFILE_H

#include "TxMessageModel.h"

#include <QByteArray>
#include <QString>
#include <QVector>

#include <optional>

/**
 * @brief Gönderim (Tx) listesinin dosya ve pano biçimi.
 *
 * Liste JSON olarak saklanır (uzantı .cantx). Aynı biçim, Tx tablosunda
 * kes/kopyala/yapıştır için pano MIME verisi olarak da kullanılır.
 */
namespace TxListFile {

/// Dosya iletişim kutularında kullanılan uzantı filtresi.
QString FileFilter();

/// Panoda Tx mesajlarını taşıyan MIME tipi.
QString MimeType();

/**
 * @brief Mesajları JSON belgesine çevirir.
 * @param messages Kaydedilecek mesajlar (sayaçlar kaydedilmez).
 * @return UTF-8 JSON metni.
 */
QByteArray Serialize(const QVector<TxMessage> &messages);

/**
 * @brief JSON belgesinden mesajları okur ve doğrular.
 * @param json Serialize() ile üretilmiş içerik.
 * @param errorText Hata durumunda açıklama (geçersiz JSON, hatalı ID/veri vb.).
 * @return Başarılıysa mesajlar, aksi halde std::nullopt.
 */
std::optional<QVector<TxMessage>> Deserialize(const QByteArray &json, QString &errorText);

/**
 * @brief Mesaj listesini dosyaya yazar.
 * @param filePath Hedef dosya yolu.
 * @param messages Kaydedilecek mesajlar.
 * @param errorText Hata durumunda açıklama.
 * @return Dosya yazıldıysa true.
 */
bool Save(const QString &filePath, const QVector<TxMessage> &messages, QString &errorText);

/**
 * @brief Mesaj listesini dosyadan okur.
 * @param filePath Okunacak dosya yolu.
 * @param errorText Hata durumunda açıklama.
 * @return Başarılıysa mesajlar, aksi halde std::nullopt.
 */
std::optional<QVector<TxMessage>> Load(const QString &filePath, QString &errorText);

} // namespace TxListFile

#endif // TXLISTFILE_H
