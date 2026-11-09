#ifndef CANOPENPROTOCOL_H
#define CANOPENPROTOCOL_H

#include "../can/CanFrame.h"

#include <QString>

/**
 * @brief CiA 301 CANopen protokolü için yardımcı fonksiyonlar.
 *
 * Fonksiyonlar yalnızca CanFrame üzerinde çalışır ve donanımdan bağımsızdır.
 * Önceden tanımlı bağlantı kümesi (predefined connection set) varsayılır.
 */
namespace CanOpen {

/// NMT komut kodları.
enum class NmtCommand : quint8
{
    Start = 0x01,
    Stop = 0x02,
    PreOperational = 0x80,
    ResetNode = 0x81,
    ResetCommunication = 0x82
};

/// SDO istek COB-ID tabanı (istemci -> sunucu).
constexpr quint32 SdoRequestBase = 0x600;
/// SDO yanıt COB-ID tabanı (sunucu -> istemci).
constexpr quint32 SdoResponseBase = 0x580;
/// Heartbeat / node guarding COB-ID tabanı.
constexpr quint32 HeartbeatBase = 0x700;
/// Geçerli en büyük CANopen düğüm numarası.
constexpr quint8 MaxNodeId = 127;

/**
 * @brief NMT komut çerçevesi oluşturur (COB-ID 0x000).
 * @param command Gönderilecek komut.
 * @param nodeId Hedef düğüm; 0 tüm düğümler anlamına gelir.
 */
CanFrame BuildNmtFrame(NmtCommand command, quint8 nodeId);

/// Veri içermeyen SYNC çerçevesi oluşturur (COB-ID 0x080).
CanFrame BuildSyncFrame();

/// NMT komutunun okunabilir adını döndürür.
QString NmtCommandName(quint8 command);

/**
 * @brief Heartbeat durum baytının okunabilir adını döndürür.
 * @param state Heartbeat verisinin ilk baytı (toggle biti yok sayılır).
 */
QString NmtStateName(quint8 state);

/**
 * @brief SDO abort kodunun okunabilir açıklamasını döndürür.
 * @param code 32 bit abort kodu (ör. 0x06020000).
 */
QString SdoAbortCodeText(quint32 code);

/**
 * @brief Çerçeveyi önceden tanımlı bağlantı kümesine göre yorumlar.
 *
 * NMT, SYNC, EMCY, TIME, PDO, SDO, Heartbeat ve LSS çerçevelerini tanır.
 * PDO içeriği burada çözülmez; bkz. PdoDecoder.
 * @param frame Yorumlanacak çerçeve.
 * @return Kısa açıklama; tanınmayan veya extended çerçeveler için boş metin.
 */
QString DescribeFrame(const CanFrame &frame);

} // namespace CanOpen

#endif // CANOPENPROTOCOL_H
