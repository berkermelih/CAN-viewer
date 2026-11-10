#include "SdoClient.h"

#include "CanOpenProtocol.h"

#include <utility>

namespace {

// SDO abort kodları (CiA 301)
constexpr quint32 AbortToggleBit = 0x05030000;
constexpr quint32 AbortTimeout = 0x05040000;
constexpr quint32 AbortInvalidCommand = 0x05040001;
constexpr quint32 AbortGeneralError = 0x08000000;

// Komut baytının üst 3 biti (ccs/scs)
constexpr quint8 CommandMask = 0xE0;

} // namespace

SdoClient::SdoClient(QObject *parent)
    : QObject(parent)
{
    m_timeoutTimer.setSingleShot(true);
    m_timeoutTimer.setInterval(1000);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &SdoClient::HandleTimeout);
}

void SdoClient::SetSendFunction(SendFunction function)
{
    m_send = std::move(function);
}

void SdoClient::SetTimeoutMs(int timeoutMs)
{
    m_timeoutTimer.setInterval(timeoutMs);
}

bool SdoClient::IsBusy() const
{
    return m_state != State::Idle;
}

bool SdoClient::StartUpload(quint8 nodeId, quint16 index, quint8 subIndex, QString &errorText)
{
    if (IsBusy()) {
        errorText = tr("Başka bir SDO aktarımı sürüyor.");
        return false;
    }
    if (nodeId < 1 || nodeId > CanOpen::MaxNodeId) {
        errorText = tr("Geçersiz düğüm numarası: %1").arg(static_cast<uint>(nodeId));
        return false;
    }

    m_nodeId = nodeId;
    m_index = index;
    m_subIndex = subIndex;
    m_buffer.clear();
    m_expectedSize = -1;
    m_toggle = false;

    std::array<quint8, 8> payload {};
    payload[0] = 0x40; // initiate upload
    payload[1] = static_cast<quint8>(index & 0xFF);
    payload[2] = static_cast<quint8>(index >> 8);
    payload[3] = subIndex;
    if (!SendRequest(payload, errorText)) {
        return false;
    }
    m_state = State::UploadInitiate;
    return true;
}

bool SdoClient::StartDownload(quint8 nodeId, quint16 index, quint8 subIndex, const QByteArray &data,
                              QString &errorText)
{
    if (IsBusy()) {
        errorText = tr("Başka bir SDO aktarımı sürüyor.");
        return false;
    }
    if (nodeId < 1 || nodeId > CanOpen::MaxNodeId) {
        errorText = tr("Geçersiz düğüm numarası: %1").arg(static_cast<uint>(nodeId));
        return false;
    }
    if (data.isEmpty()) {
        errorText = tr("Yazılacak veri boş.");
        return false;
    }

    m_nodeId = nodeId;
    m_index = index;
    m_subIndex = subIndex;
    m_buffer = data;
    m_offset = 0;
    m_toggle = false;

    std::array<quint8, 8> payload {};
    payload[1] = static_cast<quint8>(index & 0xFF);
    payload[2] = static_cast<quint8>(index >> 8);
    payload[3] = subIndex;

    if (data.size() <= 4) {
        // Expedited: e=1, s=1, n = kullanılmayan bayt sayısı
        payload[0] = static_cast<quint8>(0x23 | ((4 - data.size()) << 2));
        for (int i = 0; i < data.size(); ++i) {
            payload[static_cast<size_t>(4 + i)] = static_cast<quint8>(data.at(i));
        }
    } else {
        // Segmented: e=0, s=1, bayt 4-7 toplam boyut
        payload[0] = 0x21;
        const quint32 size = static_cast<quint32>(data.size());
        for (int i = 0; i < 4; ++i) {
            payload[static_cast<size_t>(4 + i)] = static_cast<quint8>((size >> (8 * i)) & 0xFF);
        }
    }

    if (!SendRequest(payload, errorText)) {
        return false;
    }
    m_state = State::DownloadInitiate;
    return true;
}

void SdoClient::Cancel()
{
    if (!IsBusy()) {
        return;
    }
    SendAbort(AbortGeneralError);
    Fail(tr("Kullanıcı tarafından iptal edildi."));
}

void SdoClient::HandleFrame(const CanFrame &frame)
{
    if (m_state == State::Idle || frame.extended || frame.rtr || frame.fd
        || frame.id != CanOpen::SdoResponseBase + m_nodeId || frame.dlc < 1) {
        return;
    }

    const quint8 command = frame.data[0];

    // Sunucu abort'u
    if ((command & CommandMask) == 0x80) {
        if (!MatchesAddress(frame)) {
            return;
        }
        quint32 code = 0;
        for (int i = 0; i < 4; ++i) {
            code |= static_cast<quint32>(frame.data[static_cast<size_t>(4 + i)]) << (8 * i);
        }
        Fail(tr("Abort 0x%1: %2").arg(code, 8, 16, QLatin1Char('0')).arg(CanOpen::SdoAbortCodeText(code)));
        return;
    }

    switch (m_state) {
    case State::UploadInitiate: {
        if ((command & CommandMask) != 0x40 || !MatchesAddress(frame)) {
            return; // başka bir aktarıma ait olabilir
        }
        const bool expedited = (command & 0x02) != 0;
        const bool sizeIndicated = (command & 0x01) != 0;
        if (expedited) {
            const int size = sizeIndicated ? 4 - ((command >> 2) & 0x03) : 4;
            QByteArray data;
            for (int i = 0; i < size; ++i) {
                data.append(static_cast<char>(frame.data[static_cast<size_t>(4 + i)]));
            }
            m_timeoutTimer.stop();
            m_state = State::Idle;
            emit UploadFinished(m_nodeId, m_index, m_subIndex, data);
            return;
        }
        if (sizeIndicated) {
            quint32 size = 0;
            for (int i = 0; i < 4; ++i) {
                size |= static_cast<quint32>(frame.data[static_cast<size_t>(4 + i)]) << (8 * i);
            }
            m_expectedSize = static_cast<int>(size);
        }
        m_state = State::UploadSegment;
        m_toggle = false;
        RequestNextUploadSegment();
        return;
    }

    case State::UploadSegment: {
        if ((command & CommandMask) != 0x00) {
            SendAbort(AbortInvalidCommand);
            Fail(tr("Beklenmeyen SDO yanıtı (0x%1).").arg(static_cast<uint>(command), 2, 16, QLatin1Char('0')));
            return;
        }
        const bool toggle = (command & 0x10) != 0;
        if (toggle != m_toggle) {
            SendAbort(AbortToggleBit);
            Fail(CanOpen::SdoAbortCodeText(AbortToggleBit));
            return;
        }
        const int unused = (command >> 1) & 0x07;
        for (int i = 1; i <= 7 - unused; ++i) {
            m_buffer.append(static_cast<char>(frame.data[static_cast<size_t>(i)]));
        }
        if (command & 0x01) {
            m_timeoutTimer.stop();
            m_state = State::Idle;
            emit UploadFinished(m_nodeId, m_index, m_subIndex, m_buffer);
            return;
        }
        m_toggle = !m_toggle;
        RequestNextUploadSegment();
        return;
    }

    case State::DownloadInitiate: {
        if (command != 0x60 || !MatchesAddress(frame)) {
            return;
        }
        if (m_buffer.size() <= 4) {
            m_timeoutTimer.stop();
            m_state = State::Idle;
            emit DownloadFinished(m_nodeId, m_index, m_subIndex);
            return;
        }
        m_state = State::DownloadSegment;
        m_toggle = false;
        SendNextDownloadSegment();
        return;
    }

    case State::DownloadSegment: {
        if ((command & CommandMask) != 0x20) {
            SendAbort(AbortInvalidCommand);
            Fail(tr("Beklenmeyen SDO yanıtı (0x%1).").arg(static_cast<uint>(command), 2, 16, QLatin1Char('0')));
            return;
        }
        const bool toggle = (command & 0x10) != 0;
        if (toggle != m_toggle) {
            SendAbort(AbortToggleBit);
            Fail(CanOpen::SdoAbortCodeText(AbortToggleBit));
            return;
        }
        if (m_offset >= m_buffer.size()) {
            m_timeoutTimer.stop();
            m_state = State::Idle;
            emit DownloadFinished(m_nodeId, m_index, m_subIndex);
            return;
        }
        m_toggle = !m_toggle;
        SendNextDownloadSegment();
        return;
    }

    case State::Idle:
        break;
    }
}

bool SdoClient::SendRequest(const std::array<quint8, 8> &payload, QString &errorText)
{
    if (!m_send) {
        errorText = tr("SDO gönderim fonksiyonu ayarlanmamış.");
        return false;
    }

    CanFrame frame;
    frame.id = CanOpen::SdoRequestBase + m_nodeId;
    frame.dlc = 8;
    std::copy(payload.begin(), payload.end(), frame.data.begin());
    if (!m_send(frame, errorText)) {
        return false;
    }
    m_timeoutTimer.start();
    return true;
}

void SdoClient::SendNextDownloadSegment()
{
    const int remaining = static_cast<int>(m_buffer.size()) - m_offset;
    const int count = qMin(7, remaining);
    const bool last = (m_offset + count) >= m_buffer.size();

    std::array<quint8, 8> payload {};
    payload[0] = static_cast<quint8>((m_toggle ? 0x10 : 0x00) | ((7 - count) << 1) | (last ? 0x01 : 0x00));
    for (int i = 0; i < count; ++i) {
        payload[static_cast<size_t>(1 + i)] = static_cast<quint8>(m_buffer.at(m_offset + i));
    }
    m_offset += count;

    QString errorText;
    if (!SendRequest(payload, errorText)) {
        Fail(errorText);
    }
}

void SdoClient::RequestNextUploadSegment()
{
    std::array<quint8, 8> payload {};
    payload[0] = static_cast<quint8>(0x60 | (m_toggle ? 0x10 : 0x00));

    QString errorText;
    if (!SendRequest(payload, errorText)) {
        Fail(errorText);
    }
}

void SdoClient::SendAbort(quint32 abortCode)
{
    if (!m_send) {
        return;
    }
    CanFrame frame;
    frame.id = CanOpen::SdoRequestBase + m_nodeId;
    frame.dlc = 8;
    frame.data[0] = 0x80;
    frame.data[1] = static_cast<quint8>(m_index & 0xFF);
    frame.data[2] = static_cast<quint8>(m_index >> 8);
    frame.data[3] = m_subIndex;
    for (int i = 0; i < 4; ++i) {
        frame.data[static_cast<size_t>(4 + i)] = static_cast<quint8>((abortCode >> (8 * i)) & 0xFF);
    }
    QString ignored;
    m_send(frame, ignored);
}

void SdoClient::Fail(const QString &reason)
{
    m_timeoutTimer.stop();
    m_state = State::Idle;
    emit TransferFailed(m_nodeId, m_index, m_subIndex, reason);
}

void SdoClient::HandleTimeout()
{
    if (!IsBusy()) {
        return;
    }
    SendAbort(AbortTimeout);
    Fail(CanOpen::SdoAbortCodeText(AbortTimeout));
}

bool SdoClient::MatchesAddress(const CanFrame &frame) const
{
    if (frame.dlc < 4) {
        return false;
    }
    const quint16 index = static_cast<quint16>(frame.data[1] | (frame.data[2] << 8));
    return index == m_index && frame.data[3] == m_subIndex;
}
