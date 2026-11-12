#include "PcanInterface.h"

#include <QCoreApplication>

#include <cstring>
#include <vector>

namespace {

// PCAN-Basic durum kodları (PCANBasic.h ile aynı değerler)
constexpr quint32 PcanErrorOk = 0x00000;
constexpr quint32 PcanErrorBusLight = 0x00004;
constexpr quint32 PcanErrorBusHeavy = 0x00008;
constexpr quint32 PcanErrorBusOff = 0x00010;
constexpr quint32 PcanErrorQueueReceiveEmpty = 0x00020;
constexpr quint32 PcanErrorBusPassive = 0x40000;
// Donanımın kaybolduğunu gösteren hatalar: sürücü yok, geçersiz handle (ILLHW/ILLNET/ILLCLIENT),
// kanal başlatılmamış.
constexpr quint32 PcanErrorNoDriver = 0x00200;
constexpr quint32 PcanErrorIllegalHandle = 0x01C00;
constexpr quint32 PcanErrorInitialize = 0x4000000;

// PCAN-Basic mesaj tipi bitleri
constexpr quint8 PcanMessageRtr = 0x01;
constexpr quint8 PcanMessageExtended = 0x02;
constexpr quint8 PcanMessageTypeFd = 0x04;
constexpr quint8 PcanMessageBrs = 0x08;
constexpr quint8 PcanMessageEsi = 0x10;
constexpr quint8 PcanMessageEcho = 0x20;
constexpr quint8 PcanMessageErrorFrame = 0x40;
constexpr quint8 PcanMessageStatus = 0x80;

// PCAN-Basic parametreleri (CAN_GetValue)
constexpr quint16 PcanNoneBus = 0x00;
constexpr quint8 PcanParameterChannelCondition = 0x0D;
constexpr quint8 PcanParameterChannelFeatures = 0x16;
constexpr quint8 PcanParameterAttachedChannelsCount = 0x2A;
constexpr quint8 PcanParameterAttachedChannels = 0x2B;

// Kanal durumu ve özellik değerleri
constexpr quint32 PcanChannelAvailable = 0x01;
constexpr quint32 PcanChannelOccupied = 0x02;
constexpr quint32 PcanFeatureFdCapable = 0x01;

// Hata metinleri için dil: İngilizce
constexpr quint16 PcanLanguageEnglish = 0x09;

// Tek seferde kuyruktan okunacak en fazla çerçeve (UI'nin donmaması için sınır)
constexpr int MaxFramesPerPoll = 2000;

// PCAN-Basic TPCANChannelInformation yapısının birebir karşılığı.
struct PcanChannelInformation
{
    quint16 channelHandle;
    quint8 deviceType;
    quint8 controllerNumber;
    quint32 deviceFeatures;
    char deviceName[33];
    quint32 deviceId;
    quint32 channelCondition;
};
static_assert(sizeof(PcanChannelInformation) == 52, "TPCANChannelInformation düzeni PCANBasic.h ile uyuşmuyor");

typedef quint32(PCAN_CALL *GetValueFunction)(quint16, quint8, void *, quint32);

struct BitrateInfo
{
    int bitrate;
    quint16 code; // BTR0/BTR1 değeri
};

const BitrateInfo Bitrates[] = {
    {1000000, 0x0014},
    {800000, 0x0016},
    {500000, 0x001C},
    {250000, 0x011C},
    {125000, 0x031C},
    {100000, 0x432F},
    {50000, 0x472F},
    {20000, 0x532F},
    {10000, 0x672F},
    {5000, 0x7F7F},
};

struct FdTiming
{
    int bitrate;
    const char *parameters;
};

// 80 MHz CAN saati için nominal faz zamanlamaları (örnekleme noktası ~%80).
const FdTiming FdNominalTimings[] = {
    {1000000, "nom_brp=2, nom_tseg1=31, nom_tseg2=8, nom_sjw=8"},
    {800000, "nom_brp=2, nom_tseg1=39, nom_tseg2=10, nom_sjw=10"},
    {500000, "nom_brp=2, nom_tseg1=63, nom_tseg2=16, nom_sjw=16"},
    {250000, "nom_brp=4, nom_tseg1=63, nom_tseg2=16, nom_sjw=16"},
    {125000, "nom_brp=8, nom_tseg1=63, nom_tseg2=16, nom_sjw=16"},
};

// 80 MHz CAN saati için veri fazı zamanlamaları.
const FdTiming FdDataTimings[] = {
    {1000000, "data_brp=2, data_tseg1=31, data_tseg2=8, data_sjw=8"},
    {2000000, "data_brp=2, data_tseg1=15, data_tseg2=4, data_sjw=4"},
    {4000000, "data_brp=1, data_tseg1=15, data_tseg2=4, data_sjw=4"},
    {5000000, "data_brp=1, data_tseg1=12, data_tseg2=3, data_sjw=3"},
    {8000000, "data_brp=1, data_tseg1=7, data_tseg2=2, data_sjw=2"},
    {10000000, "data_brp=1, data_tseg1=5, data_tseg2=2, data_sjw=2"},
};

// Tablodan bit hızına karşılık gelen zamanlama metnini bulur.
template <size_t N>
const char *FindTiming(const FdTiming (&table)[N], int bitrate)
{
    for (const FdTiming &timing : table) {
        if (timing.bitrate == bitrate) {
            return timing.parameters;
        }
    }
    return nullptr;
}

// PCAN-USB kanal handle değerleri (USBBUS1..16).
QList<quint16> UsbHandles()
{
    QList<quint16> handles;
    for (quint16 handle = 0x51; handle <= 0x58; ++handle) {
        handles.append(handle);
    }
    for (quint16 handle = 0x509; handle <= 0x510; ++handle) {
        handles.append(handle);
    }
    return handles;
}

// Kanal handle değerini PCAN-Basic adına çevirir (ör. 0x51 -> "USBBUS1").
QString HandleName(quint16 handle)
{
    if (handle >= 0x51 && handle <= 0x58) {
        return QStringLiteral("USBBUS%1").arg(handle - 0x51 + 1);
    }
    if (handle >= 0x509 && handle <= 0x510) {
        return QStringLiteral("USBBUS%1").arg(handle - 0x509 + 9);
    }
    return QStringLiteral("0x%1").arg(handle, 0, 16);
}

// Algılama için kullanılan, uygulama boyunca yüklü kalan kütüphane.
QLibrary &DetectionLibrary()
{
    static QLibrary library;
    return library;
}

} // namespace

QString PcanChannelInfo::DisplayName() const
{
    const QString device = deviceName.isEmpty() ? QStringLiteral("PCAN") : deviceName;
    QString text = QCoreApplication::translate("PcanInterface", "%1 · Kanal %2 (%3)")
                       .arg(device)
                       .arg(channelNumber)
                       .arg(HandleName(handle));
    if (occupied) {
        text += QCoreApplication::translate("PcanInterface", " · kullanımda");
    }
    return text;
}

PcanInterface::PcanInterface(QObject *parent)
    : ICanInterface(parent)
{
    m_receiveTimer.setInterval(1);
    m_receiveTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_receiveTimer, &QTimer::timeout, this, &PcanInterface::PollReceiveQueue);

    m_statusTimer.setInterval(500);
    connect(&m_statusTimer, &QTimer::timeout, this, &PcanInterface::PollBusStatus);
}

PcanInterface::~PcanInterface()
{
    Close();
}

bool PcanInterface::Open(const CanBusSettings &settings, QString &errorText)
{
    if (m_open) {
        Close();
    }
    if (!LoadPcanLibrary(errorText)) {
        return false;
    }

    bool ok = false;
    const uint handleValue = settings.channel.toUInt(&ok);
    if (!ok || handleValue == 0 || handleValue > 0xFFFF) {
        errorText = tr("Geçersiz PCAN kanalı: %1").arg(settings.channel);
        return false;
    }
    const quint16 handle = static_cast<quint16>(handleValue);

    quint32 status = PcanErrorOk;
    if (settings.fd) {
        if (m_initializeFd == nullptr || m_readFd == nullptr || m_writeFd == nullptr) {
            errorText = tr("Yüklü PCAN-Basic sürümü CAN FD fonksiyonlarını içermiyor. Sürücüyü güncelleyin.");
            return false;
        }
        const char *nominal = FindTiming(FdNominalTimings, settings.bitrate);
        const char *data = FindTiming(FdDataTimings, settings.dataBitrate);
        if (nominal == nullptr || data == nullptr) {
            errorText = tr("Desteklenmeyen CAN FD bit hızı: %1 / %2 bit/s").arg(settings.bitrate).arg(settings.dataBitrate);
            return false;
        }
        if (settings.dataBitrate < settings.bitrate) {
            errorText = tr("CAN FD veri bit hızı nominal bit hızından düşük olamaz.");
            return false;
        }
        QByteArray parameters = QByteArrayLiteral("f_clock_mhz=80, ");
        parameters += nominal;
        parameters += ", ";
        parameters += data;
        status = m_initializeFd(handle, parameters.data());
    } else {
        quint16 bitrateCode = 0;
        for (const BitrateInfo &info : Bitrates) {
            if (info.bitrate == settings.bitrate) {
                bitrateCode = info.code;
            }
        }
        if (bitrateCode == 0) {
            errorText = tr("Desteklenmeyen bit hızı: %1 bit/s").arg(settings.bitrate);
            return false;
        }
        // Plug & play donanımda (USB) donanım tipi, port ve kesme parametreleri 0'dır.
        status = m_initialize(handle, bitrateCode, 0, 0, 0);
    }

    if (status != PcanErrorOk) {
        errorText = tr("%1 kanalı açılamadı: %2").arg(HandleName(handle), ErrorText(status));
        return false;
    }

    m_channelName = HandleName(handle);
    m_channelHandle = handle;
    m_open = true;
    m_fdMode = settings.fd;
    m_hasFirstTimestamp = false;
    m_lastBusStatus = 0xFFFFFFFFu;
    m_receiveTimer.start();
    m_statusTimer.start();
    PollBusStatus();
    return true;
}

void PcanInterface::Close()
{
    m_receiveTimer.stop();
    m_statusTimer.stop();
    if (m_open && m_uninitialize != nullptr) {
        m_uninitialize(m_channelHandle);
    }
    m_open = false;
    m_fdMode = false;
}

bool PcanInterface::IsOpen() const
{
    return m_open;
}

bool PcanInterface::IsFdMode() const
{
    return m_fdMode;
}

bool PcanInterface::WriteFrame(const CanFrame &frame, QString &errorText)
{
    if (!m_open) {
        errorText = tr("PCAN kanalı açık değil.");
        return false;
    }
    if (frame.fd && !m_fdMode) {
        errorText = tr("CAN FD mesajı gönderilemedi: kanal CAN CC (klasik) modunda açık. "
                       "CAN FD modunda yeniden bağlanın.");
        return false;
    }

    quint32 status = PcanErrorOk;
    if (m_fdMode) {
        PcanMessageFd message;
        std::memset(&message, 0, sizeof(message));
        message.id = frame.id;
        quint8 type = frame.extended ? PcanMessageExtended : 0;
        if (frame.fd) {
            type |= PcanMessageTypeFd;
            if (frame.brs) {
                type |= PcanMessageBrs;
            }
        } else if (frame.rtr) {
            type |= PcanMessageRtr;
        }
        message.messageType = type;
        const int length = frame.fd ? qMin<int>(frame.dlc, MaxCanFdDataLength) : qMin<int>(frame.dlc, MaxClassicDataLength);
        message.dlc = CanFdLengthToDlc(length);
        std::memcpy(message.data, frame.data.data(), static_cast<size_t>(CanFdDlcToLength(message.dlc)));
        status = m_writeFd(m_channelHandle, &message);
    } else {
        PcanMessage message;
        std::memset(&message, 0, sizeof(message));
        message.id = frame.id;
        message.messageType = static_cast<quint8>((frame.extended ? PcanMessageExtended : 0)
                                                  | (frame.rtr ? PcanMessageRtr : 0));
        message.length = qMin<quint8>(frame.dlc, MaxClassicDataLength);
        std::memcpy(message.data, frame.data.data(), sizeof(message.data));
        status = m_write(m_channelHandle, &message);
    }

    if (status != PcanErrorOk) {
        errorText = tr("Gönderim hatası: %1").arg(ErrorText(status));
        return false;
    }
    return true;
}

QString PcanInterface::Name() const
{
    return m_open ? QStringLiteral("PCAN (%1)").arg(m_channelName) : QStringLiteral("PCAN");
}

QList<PcanChannelInfo> PcanInterface::DetectChannels(QString &errorText)
{
    QList<PcanChannelInfo> result;
    QLibrary &library = DetectionLibrary();
    if (!library.isLoaded()) {
#if defined(Q_OS_WIN)
        library.setFileName(QStringLiteral("PCANBasic"));
#else
        library.setFileName(QStringLiteral("pcanbasic"));
#endif
        if (!library.load()) {
            errorText = tr("PCAN-Basic kütüphanesi yüklenemedi (%1).").arg(library.errorString());
            return result;
        }
    }
    const auto getValue = reinterpret_cast<GetValueFunction>(library.resolve("CAN_GetValue"));
    if (getValue == nullptr) {
        errorText = tr("PCAN-Basic kütüphanesinde CAN_GetValue bulunamadı.");
        return result;
    }

    // 1) Sürücüden bağlı kanalların listesini iste (PCAN-Basic 4.2 ve sonrası).
    quint32 count = 0;
    if (getValue(PcanNoneBus, PcanParameterAttachedChannelsCount, &count, sizeof(count)) == PcanErrorOk) {
        if (count > 0 && count < 256) {
            std::vector<PcanChannelInformation> channels(count);
            std::memset(channels.data(), 0, channels.size() * sizeof(PcanChannelInformation));
            if (getValue(PcanNoneBus, PcanParameterAttachedChannels, channels.data(),
                         static_cast<quint32>(channels.size() * sizeof(PcanChannelInformation)))
                == PcanErrorOk) {
                for (const PcanChannelInformation &channel : channels) {
                    if ((channel.channelCondition & (PcanChannelAvailable | PcanChannelOccupied)) == 0) {
                        continue;
                    }
                    PcanChannelInfo info;
                    info.handle = channel.channelHandle;
                    info.deviceName = QString::fromLatin1(channel.deviceName,
                                                          static_cast<int>(qstrnlen(channel.deviceName,
                                                                                    sizeof(channel.deviceName))));
                    info.channelNumber = channel.controllerNumber + 1;
                    info.deviceId = channel.deviceId;
                    info.fdCapable = (channel.deviceFeatures & PcanFeatureFdCapable) != 0;
                    info.occupied = channel.channelCondition == PcanChannelOccupied;
                    result.append(info);
                }
                return result;
            }
        } else if (count == 0) {
            return result;
        }
    }

    // 2) Eski sürücü: PCAN-USB kanallarını tek tek yokla.
    const QList<quint16> handles = UsbHandles();
    for (const quint16 handle : handles) {
        quint32 condition = 0;
        if (getValue(handle, PcanParameterChannelCondition, &condition, sizeof(condition)) != PcanErrorOk) {
            continue;
        }
        if ((condition & (PcanChannelAvailable | PcanChannelOccupied)) == 0) {
            continue;
        }
        quint32 features = 0;
        getValue(handle, PcanParameterChannelFeatures, &features, sizeof(features));
        PcanChannelInfo info;
        info.handle = handle;
        info.deviceName = QStringLiteral("PCAN-USB");
        info.channelNumber = (handle >= 0x509 ? handle - 0x509 + 9 : handle - 0x51 + 1);
        info.fdCapable = (features & PcanFeatureFdCapable) != 0;
        info.occupied = condition == PcanChannelOccupied;
        result.append(info);
    }
    return result;
}

QList<int> PcanInterface::SupportedBitrates()
{
    QList<int> result;
    for (const BitrateInfo &info : Bitrates) {
        result.append(info.bitrate);
    }
    return result;
}

QList<int> PcanInterface::SupportedFdBitrates()
{
    QList<int> result;
    for (const FdTiming &timing : FdNominalTimings) {
        result.append(timing.bitrate);
    }
    return result;
}

QList<int> PcanInterface::SupportedFdDataBitrates()
{
    QList<int> result;
    for (const FdTiming &timing : FdDataTimings) {
        result.append(timing.bitrate);
    }
    return result;
}

bool PcanInterface::LoadPcanLibrary(QString &errorText)
{
    if (m_library.isLoaded() && m_initialize != nullptr) {
        return true;
    }

#if defined(Q_OS_WIN)
    m_library.setFileName(QStringLiteral("PCANBasic"));
#else
    m_library.setFileName(QStringLiteral("pcanbasic"));
#endif
    if (!m_library.load()) {
        errorText = tr("PCAN-Basic kütüphanesi yüklenemedi (%1).\n"
                       "PEAK-System PCAN-Basic sürücüsünün kurulu olduğundan emin olun.")
                        .arg(m_library.errorString());
        return false;
    }

    // QFunctionPointer (void(*)()) üzerinden dönüşüm GCC'de -Wcast-function-type uyarısı vermez.
    m_initialize = reinterpret_cast<InitializeFunction>(m_library.resolve("CAN_Initialize"));
    m_uninitialize = reinterpret_cast<UninitializeFunction>(m_library.resolve("CAN_Uninitialize"));
    m_read = reinterpret_cast<ReadFunction>(m_library.resolve("CAN_Read"));
    m_write = reinterpret_cast<WriteFunction>(m_library.resolve("CAN_Write"));
    m_getStatus = reinterpret_cast<GetStatusFunction>(m_library.resolve("CAN_GetStatus"));
    m_getErrorText = reinterpret_cast<GetErrorTextFunction>(m_library.resolve("CAN_GetErrorText"));
    // Kanal durumu (donanım söküldü mü) için kullanılır; yoksa yalnızca hata kodlarına bakılır.
    m_getValue = reinterpret_cast<ChannelGetValueFunction>(m_library.resolve("CAN_GetValue"));
    // CAN FD fonksiyonları eski sürümlerde yoktur; yalnızca FD modunda gereklidir.
    m_initializeFd = reinterpret_cast<InitializeFdFunction>(m_library.resolve("CAN_InitializeFD"));
    m_readFd = reinterpret_cast<ReadFdFunction>(m_library.resolve("CAN_ReadFD"));
    m_writeFd = reinterpret_cast<WriteFdFunction>(m_library.resolve("CAN_WriteFD"));

    if (m_initialize == nullptr || m_uninitialize == nullptr || m_read == nullptr || m_write == nullptr
        || m_getStatus == nullptr || m_getErrorText == nullptr) {
        errorText = tr("PCAN-Basic kütüphanesinde gerekli fonksiyonlar bulunamadı. "
                       "Sürücü sürümünü kontrol edin.");
        m_initialize = nullptr;
        m_library.unload();
        return false;
    }
    return true;
}

QString PcanInterface::ErrorText(quint32 status) const
{
    if (m_getErrorText != nullptr) {
        char buffer[256];
        std::memset(buffer, 0, sizeof(buffer));
        if (m_getErrorText(status, PcanLanguageEnglish, buffer) == PcanErrorOk) {
            return QString::fromLatin1(buffer);
        }
    }
    return tr("PCAN hata kodu 0x%1").arg(status, 0, 16);
}

QString PcanInterface::BusStatusText(quint32 status) const
{
    if (status == PcanErrorOk) {
        return QStringLiteral("Bus OK");
    }
    if (status & PcanErrorBusOff) {
        return QStringLiteral("Bus Off");
    }
    if (status & PcanErrorBusPassive) {
        return QStringLiteral("Bus Passive");
    }
    if (status & PcanErrorBusHeavy) {
        return QStringLiteral("Bus Warning");
    }
    if (status & PcanErrorBusLight) {
        return QStringLiteral("Bus Light");
    }
    return ErrorText(status);
}

qint64 PcanInterface::RelativeTimestamp(quint64 timeUs)
{
    if (!m_hasFirstTimestamp) {
        m_firstTimestampUs = timeUs;
        m_hasFirstTimestamp = true;
    }
    return static_cast<qint64>(timeUs - m_firstTimestampUs);
}

void PcanInterface::PollReceiveQueue()
{
    if (!m_open) {
        return;
    }

    QVector<CanFrame> frames;
    for (int i = 0; i < MaxFramesPerPoll; ++i) {
        CanFrame frame;
        quint8 type = 0;
        quint32 status = PcanErrorOk;
        quint64 timeUs = 0;

        if (m_fdMode) {
            PcanMessageFd message;
            std::memset(&message, 0, sizeof(message));
            status = m_readFd(m_channelHandle, &message, &timeUs);
            if (status == PcanErrorOk) {
                type = message.messageType;
                frame.id = message.id;
                frame.dlc = static_cast<quint8>(CanFdDlcToLength(message.dlc));
                if (!(type & PcanMessageTypeFd)) {
                    frame.dlc = qMin<quint8>(frame.dlc, MaxClassicDataLength);
                }
                std::memcpy(frame.data.data(), message.data, sizeof(message.data));
            }
        } else {
            PcanMessage message;
            PcanTimestamp timestamp;
            std::memset(&message, 0, sizeof(message));
            std::memset(&timestamp, 0, sizeof(timestamp));
            status = m_read(m_channelHandle, &message, &timestamp);
            if (status == PcanErrorOk) {
                type = message.messageType;
                frame.id = message.id;
                frame.dlc = qMin<quint8>(message.length, MaxClassicDataLength);
                std::memcpy(frame.data.data(), message.data, sizeof(message.data));
                timeUs = static_cast<quint64>(timestamp.micros) + 1000ULL * timestamp.millis
                         + 0x100000000ULL * 1000ULL * timestamp.millisOverflow;
            }
        }

        if (status & PcanErrorQueueReceiveEmpty) {
            break;
        }
        if (status != PcanErrorOk) {
            // Okuma hatası bus durumuyla ilgilidir; durumu güncelleyip bu turu bitir.
            PollBusStatus();
            break;
        }
        if (type & PcanMessageStatus) {
            PollBusStatus();
            continue;
        }
        if (type & (PcanMessageErrorFrame | PcanMessageEcho)) {
            continue; // Hata çerçeveleri ve yankı çerçeveleri bu sürümde gösterilmez.
        }

        frame.extended = (type & PcanMessageExtended) != 0;
        frame.fd = (type & PcanMessageTypeFd) != 0;
        frame.rtr = !frame.fd && (type & PcanMessageRtr) != 0;
        frame.brs = frame.fd && (type & PcanMessageBrs) != 0;
        frame.esi = frame.fd && (type & PcanMessageEsi) != 0;
        frame.timestampUs = RelativeTimestamp(timeUs);
        frames.append(frame);
    }

    if (!frames.isEmpty()) {
        emit FramesReceived(frames);
    }
}

void PcanInterface::PollBusStatus()
{
    if (!m_open) {
        return;
    }
    const quint32 status = m_getStatus(m_channelHandle);
    if (IsDeviceLost(status)) {
        HandleDeviceLost(tr("%1 adaptörüne erişilemiyor; donanım sökülmüş veya sürücü bağlantısı kesilmiş olabilir.")
                             .arg(m_channelName));
        return;
    }
    if (status != m_lastBusStatus) {
        m_lastBusStatus = status;
        emit BusStatusChanged(BusStatusText(status));
    }
}

bool PcanInterface::IsDeviceLost(quint32 lastStatus) const
{
    if ((lastStatus & (PcanErrorNoDriver | PcanErrorIllegalHandle | PcanErrorInitialize)) != 0) {
        return true;
    }
    if (m_getValue == nullptr) {
        return false;
    }
    quint32 condition = 0;
    const quint32 result = m_getValue(m_channelHandle, PcanParameterChannelCondition, &condition, sizeof(condition));
    if (result == PcanErrorOk) {
        return (condition & (PcanChannelAvailable | PcanChannelOccupied)) == 0;
    }
    return (result & (PcanErrorNoDriver | PcanErrorIllegalHandle | PcanErrorInitialize)) != 0;
}

void PcanInterface::HandleDeviceLost(const QString &reason)
{
    if (!m_open) {
        return;
    }
    Close();
    emit ConnectionLost(reason);
}
