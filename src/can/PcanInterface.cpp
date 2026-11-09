#include "PcanInterface.h"

#include <cstring>

namespace {

// PCAN-Basic durum kodları (PCANBasic.h ile aynı değerler)
constexpr quint32 PcanErrorOk = 0x00000;
constexpr quint32 PcanErrorBusLight = 0x00004;
constexpr quint32 PcanErrorBusHeavy = 0x00008;
constexpr quint32 PcanErrorBusOff = 0x00010;
constexpr quint32 PcanErrorQueueReceiveEmpty = 0x00020;
constexpr quint32 PcanErrorBusPassive = 0x40000;

// PCAN-Basic mesaj tipi bitleri
constexpr quint8 PcanMessageRtr = 0x01;
constexpr quint8 PcanMessageExtended = 0x02;
constexpr quint8 PcanMessageFd = 0x04;
constexpr quint8 PcanMessageErrorFrame = 0x40;
constexpr quint8 PcanMessageStatus = 0x80;

// Hata metinleri için dil: İngilizce
constexpr quint16 PcanLanguageEnglish = 0x09;

// Tek seferde kuyruktan okunacak en fazla çerçeve (UI'nin donmaması için sınır)
constexpr int MaxFramesPerPoll = 2000;

struct ChannelInfo
{
    const char *name;
    const char *displayName;
    quint16 handle;
};

const ChannelInfo Channels[] = {
    {"PCAN_USBBUS1", "PCAN-USB 1", 0x51},
    {"PCAN_USBBUS2", "PCAN-USB 2", 0x52},
    {"PCAN_USBBUS3", "PCAN-USB 3", 0x53},
    {"PCAN_USBBUS4", "PCAN-USB 4", 0x54},
    {"PCAN_USBBUS5", "PCAN-USB 5", 0x55},
    {"PCAN_USBBUS6", "PCAN-USB 6", 0x56},
    {"PCAN_USBBUS7", "PCAN-USB 7", 0x57},
    {"PCAN_USBBUS8", "PCAN-USB 8", 0x58},
};

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

} // namespace

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

bool PcanInterface::Open(const QString &channel, int bitrate, QString &errorText)
{
    if (m_open) {
        Close();
    }
    if (!LoadPcanLibrary(errorText)) {
        return false;
    }

    quint16 handle = 0;
    for (const ChannelInfo &info : Channels) {
        if (channel == QLatin1String(info.name)) {
            handle = info.handle;
        }
    }
    if (handle == 0) {
        errorText = tr("Bilinmeyen PCAN kanalı: %1").arg(channel);
        return false;
    }

    quint16 bitrateCode = 0;
    for (const BitrateInfo &info : Bitrates) {
        if (info.bitrate == bitrate) {
            bitrateCode = info.code;
        }
    }
    if (bitrateCode == 0) {
        errorText = tr("Desteklenmeyen bit hızı: %1 bit/s").arg(bitrate);
        return false;
    }

    // Plug & play donanımda (USB) donanım tipi, port ve kesme parametreleri 0'dır.
    const quint32 status = m_initialize(handle, bitrateCode, 0, 0, 0);
    if (status != PcanErrorOk) {
        errorText = tr("%1 kanalı açılamadı: %2").arg(channel, ErrorText(status));
        return false;
    }

    m_channelName = channel;
    m_channelHandle = handle;
    m_open = true;
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
}

bool PcanInterface::IsOpen() const
{
    return m_open;
}

bool PcanInterface::WriteFrame(const CanFrame &frame, QString &errorText)
{
    if (!m_open) {
        errorText = tr("PCAN kanalı açık değil.");
        return false;
    }

    PcanMessage message;
    std::memset(&message, 0, sizeof(message));
    message.id = frame.id;
    message.messageType = static_cast<quint8>((frame.extended ? PcanMessageExtended : 0)
                                              | (frame.rtr ? PcanMessageRtr : 0));
    message.length = qMin<quint8>(frame.dlc, 8);
    std::memcpy(message.data, frame.data.data(), sizeof(message.data));

    const quint32 status = m_write(m_channelHandle, &message);
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

QList<QPair<QString, QString>> PcanInterface::AvailableChannels()
{
    QList<QPair<QString, QString>> result;
    for (const ChannelInfo &info : Channels) {
        result.append(qMakePair(QString::fromLatin1(info.name), QString::fromLatin1(info.displayName)));
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
    return QStringLiteral("PCAN hata kodu 0x%1").arg(status, 0, 16);
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

void PcanInterface::PollReceiveQueue()
{
    if (!m_open) {
        return;
    }

    QVector<CanFrame> frames;
    for (int i = 0; i < MaxFramesPerPoll; ++i) {
        PcanMessage message;
        PcanTimestamp timestamp;
        std::memset(&message, 0, sizeof(message));
        std::memset(&timestamp, 0, sizeof(timestamp));

        const quint32 status = m_read(m_channelHandle, &message, &timestamp);
        if (status & PcanErrorQueueReceiveEmpty) {
            break;
        }
        if (status != PcanErrorOk) {
            // Okuma hatası bus durumuyla ilgilidir; durumu güncelleyip bu turu bitir.
            PollBusStatus();
            break;
        }
        if (message.messageType & PcanMessageStatus) {
            PollBusStatus();
            continue;
        }
        if (message.messageType & (PcanMessageErrorFrame | PcanMessageFd)) {
            continue; // Hata çerçeveleri ve CAN FD bu sürümde gösterilmez.
        }

        CanFrame frame;
        frame.id = message.id;
        frame.extended = (message.messageType & PcanMessageExtended) != 0;
        frame.rtr = (message.messageType & PcanMessageRtr) != 0;
        frame.dlc = qMin<quint8>(message.length, 8);
        std::memcpy(frame.data.data(), message.data, sizeof(message.data));

        const quint64 timeUs = static_cast<quint64>(timestamp.micros)
                               + 1000ULL * timestamp.millis
                               + 0x100000000ULL * 1000ULL * timestamp.millisOverflow;
        if (!m_hasFirstTimestamp) {
            m_firstTimestampUs = timeUs;
            m_hasFirstTimestamp = true;
        }
        frame.timestampUs = static_cast<qint64>(timeUs - m_firstTimestampUs);
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
    if (status != m_lastBusStatus) {
        m_lastBusStatus = status;
        emit BusStatusChanged(BusStatusText(status));
    }
}
