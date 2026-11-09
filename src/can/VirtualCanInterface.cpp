#include "VirtualCanInterface.h"

#include <QTimer>

VirtualCanInterface::VirtualCanInterface(QObject *parent)
    : ICanInterface(parent)
{
}

bool VirtualCanInterface::Open(const QString &channel, int bitrate, QString &errorText)
{
    Q_UNUSED(channel)
    Q_UNUSED(bitrate)
    Q_UNUSED(errorText)

    m_open = true;
    m_pendingFrames.clear();
    m_clock.start();
    emit BusStatusChanged(tr("Sanal bus (loopback)"));
    return true;
}

void VirtualCanInterface::Close()
{
    m_open = false;
    m_pendingFrames.clear();
}

bool VirtualCanInterface::IsOpen() const
{
    return m_open;
}

bool VirtualCanInterface::WriteFrame(const CanFrame &frame, QString &errorText)
{
    if (!m_open) {
        errorText = tr("Sanal kanal açık değil.");
        return false;
    }

    CanFrame looped = frame;
    looped.timestampUs = m_clock.nsecsElapsed() / 1000;
    m_pendingFrames.append(looped);

    if (!m_flushScheduled) {
        m_flushScheduled = true;
        QTimer::singleShot(0, this, &VirtualCanInterface::FlushPendingFrames);
    }
    return true;
}

QString VirtualCanInterface::Name() const
{
    return tr("Sanal (loopback)");
}

QString VirtualCanInterface::ChannelName()
{
    return QStringLiteral("VIRTUAL");
}

void VirtualCanInterface::FlushPendingFrames()
{
    m_flushScheduled = false;
    if (!m_open || m_pendingFrames.isEmpty()) {
        return;
    }
    QVector<CanFrame> frames;
    frames.swap(m_pendingFrames);
    emit FramesReceived(frames);
}
