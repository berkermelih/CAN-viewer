#include "TxListFile.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace TxListFile {

namespace {

constexpr int FormatVersion = 1;

// Biçim adı; başka JSON dosyalarının yanlışlıkla açılmasını engeller.
QString FormatName()
{
    return QStringLiteral("CanbusAnalyzer.TxList");
}

// Çeviri bağlamı için kısa yardımcı.
QString Tr(const char *text)
{
    return QCoreApplication::translate("TxListFile", text);
}

// Tek bir mesajı JSON nesnesine çevirir.
QJsonObject MessageToJson(const TxMessage &message)
{
    const CanFrame &frame = message.frame;
    QJsonObject object;
    object.insert(QStringLiteral("id"), FormatCanId(frame.id, frame.extended));
    object.insert(QStringLiteral("extended"), frame.extended);
    object.insert(QStringLiteral("fd"), frame.fd);
    object.insert(QStringLiteral("brs"), frame.brs);
    object.insert(QStringLiteral("rtr"), frame.rtr);
    object.insert(QStringLiteral("length"), static_cast<int>(frame.dlc));
    CanFrame dataFrame = frame;
    dataFrame.rtr = false;
    object.insert(QStringLiteral("data"), FormatFrameData(dataFrame));
    object.insert(QStringLiteral("cycleMs"), message.periodMs);
    object.insert(QStringLiteral("paused"), !message.periodicEnabled);
    object.insert(QStringLiteral("comment"), message.comment);
    return object;
}

// JSON nesnesinden mesaj okur; alanları CAN kurallarına göre doğrular.
bool MessageFromJson(const QJsonObject &object, int number, TxMessage &message, QString &errorText)
{
    const QString prefix = Tr("%1. mesaj: ").arg(number);

    CanFrame frame;
    frame.extended = object.value(QStringLiteral("extended")).toBool();
    frame.fd = object.value(QStringLiteral("fd")).toBool();
    frame.brs = frame.fd && object.value(QStringLiteral("brs")).toBool();
    frame.rtr = !frame.fd && object.value(QStringLiteral("rtr")).toBool();

    quint32 id = 0;
    if (!ParseHexNumber(object.value(QStringLiteral("id")).toString(), id)
        || id > (frame.extended ? MaxExtendedCanId : MaxStandardCanId)) {
        errorText = prefix + Tr("geçersiz ID.");
        return false;
    }
    frame.id = id;

    const int length = object.value(QStringLiteral("length")).toInt(-1);
    const bool lengthValid = frame.fd ? IsValidCanFdLength(length) : (length >= 0 && length <= MaxClassicDataLength);
    if (!lengthValid) {
        errorText = prefix + Tr("geçersiz veri uzunluğu (%1).").arg(length);
        return false;
    }
    frame.dlc = static_cast<quint8>(length);

    if (!frame.rtr) {
        QByteArray bytes;
        if (!ParseHexBytes(object.value(QStringLiteral("data")).toString(), bytes) || bytes.size() > length) {
            errorText = prefix + Tr("geçersiz veri baytları.");
            return false;
        }
        for (int i = 0; i < bytes.size(); ++i) {
            frame.data[static_cast<size_t>(i)] = static_cast<quint8>(bytes.at(i));
        }
    }

    const int cycle = object.value(QStringLiteral("cycleMs")).toInt(0);
    if (cycle < 0 || cycle > 3600000) {
        errorText = prefix + Tr("geçersiz periyot (%1 ms).").arg(cycle);
        return false;
    }

    message = TxMessage();
    message.frame = frame;
    message.periodMs = cycle;
    message.periodicEnabled = !object.value(QStringLiteral("paused")).toBool();
    message.comment = object.value(QStringLiteral("comment")).toString();
    return true;
}

} // namespace

QString FileFilter()
{
    return Tr("Canbus Analyzer gönderim listesi (*.cantx);;Tüm dosyalar (*)");
}

QString MimeType()
{
    return QStringLiteral("application/x-canbusanalyzer-txlist");
}

QByteArray Serialize(const QVector<TxMessage> &messages)
{
    QJsonArray array;
    for (const TxMessage &message : messages) {
        array.append(MessageToJson(message));
    }
    QJsonObject root;
    root.insert(QStringLiteral("format"), FormatName());
    root.insert(QStringLiteral("version"), FormatVersion);
    root.insert(QStringLiteral("messages"), array);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

std::optional<QVector<TxMessage>> Deserialize(const QByteArray &json, QString &errorText)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        errorText = Tr("Dosya geçerli bir JSON belgesi değil: %1").arg(parseError.errorString());
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toString() != FormatName()) {
        errorText = Tr("Dosya bir Canbus Analyzer gönderim listesi değil.");
        return std::nullopt;
    }
    if (root.value(QStringLiteral("version")).toInt() > FormatVersion) {
        errorText = Tr("Dosya bu sürümden daha yeni bir biçimde kaydedilmiş.");
        return std::nullopt;
    }

    QVector<TxMessage> messages;
    const QJsonArray array = root.value(QStringLiteral("messages")).toArray();
    messages.reserve(array.size());
    for (int i = 0; i < array.size(); ++i) {
        TxMessage message;
        if (!MessageFromJson(array.at(i).toObject(), i + 1, message, errorText)) {
            return std::nullopt;
        }
        messages.append(message);
    }
    return messages;
}

bool Save(const QString &filePath, const QVector<TxMessage> &messages, QString &errorText)
{
    // QSaveFile: yazma yarıda kalırsa mevcut dosya bozulmaz.
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        errorText = file.errorString();
        return false;
    }
    file.write(Serialize(messages));
    if (!file.commit()) {
        errorText = file.errorString();
        return false;
    }
    return true;
}

std::optional<QVector<TxMessage>> Load(const QString &filePath, QString &errorText)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorText = file.errorString();
        return std::nullopt;
    }
    return Deserialize(file.readAll(), errorText);
}

} // namespace TxListFile
