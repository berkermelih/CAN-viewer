#include "EdsParser.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>

#include <algorithm>

namespace {


} // namespace

std::optional<ObjectDictionary> EdsParser::OpenEdsFile(const QString &filePath, QString &errorText,
                                                       QStringList &warnings)
{
    IniDocument document;
    OrderedSections orderedSections;
    if (!ReadIniDocument(filePath, document, orderedSections, errorText)) {
        return std::nullopt;
    }

    static const QRegularExpression objectPattern(QStringLiteral("^([0-9a-f]{4})$"));
    static const QRegularExpression subPattern(QStringLiteral("^([0-9a-f]{4})sub([0-9a-f]{1,2})$"));

    // Nesne ve alt nesne bölümlerini index'e göre grupla.
    QMap<quint16, QString> objectSections;
    QMap<quint16, QMap<quint8, QString>> subSections;
    for (auto it = document.constBegin(); it != document.constEnd(); ++it) {
        const QString &name = it.key();
        const QRegularExpressionMatch objectMatch = objectPattern.match(name);
        if (objectMatch.hasMatch()) {
            objectSections.insert(static_cast<quint16>(objectMatch.captured(1).toUInt(nullptr, 16)), name);
            continue;
        }
        const QRegularExpressionMatch subMatch = subPattern.match(name);
        if (subMatch.hasMatch()) {
            const quint16 index = static_cast<quint16>(subMatch.captured(1).toUInt(nullptr, 16));
            const quint8 sub = static_cast<quint8>(subMatch.captured(2).toUInt(nullptr, 16));
            subSections[index].insert(sub, name);
        }
    }

    ObjectDictionary dictionary;
    dictionary.SetFilePath(filePath);
    dictionary.SetFileInfo(document.value(QStringLiteral("fileinfo")));
    dictionary.SetDeviceInfo(document.value(QStringLiteral("deviceinfo")));
    dictionary.SetFileInfoEntries(orderedSections.value(QStringLiteral("fileinfo")));
    dictionary.SetDeviceInfoEntries(orderedSections.value(QStringLiteral("deviceinfo")));
    // DCF: CiA 306 bölüm adı "DeviceComissioning" (standarttaki yazımla); "Commissioning" da kabul edilir.
    dictionary.SetCommissioningEntries(orderedSections.contains(QStringLiteral("devicecomissioning"))
                                           ? orderedSections.value(QStringLiteral("devicecomissioning"))
                                           : orderedSections.value(QStringLiteral("devicecommissioning")));

    for (auto it = objectSections.constBegin(); it != objectSections.constEnd(); ++it) {
        const quint16 index = it.key();
        const IniSection &section = document[it.value()];

        OdObject object;
        object.index = index;
        object.name = section.value(QStringLiteral("parametername"));
        object.objectType = static_cast<quint8>(
            ParseEdsNumber(section.value(QStringLiteral("objecttype"), QStringLiteral("0x7"))).value_or(CanOpenObjectType::Var));

        const bool isComplex = object.objectType == CanOpenObjectType::Array
                               || object.objectType == CanOpenObjectType::Record;
        if (!isComplex) {
            object.entries.append(ParseEntry(section, index, 0));
        } else {
            const int compactCount = static_cast<int>(
                ParseEdsNumber(section.value(QStringLiteral("compactsubobj"))).value_or(0));
            if (compactCount > 0) {
                ExpandCompactSubObjects(document, section, object, qMin(compactCount, 254));
            } else {
                const QMap<quint8, QString> subs = subSections.value(index);
                for (auto subIt = subs.constBegin(); subIt != subs.constEnd(); ++subIt) {
                    object.entries.append(ParseEntry(document[subIt.value()], index, subIt.key()));
                }
                const auto declared = ParseEdsNumber(section.value(QStringLiteral("subnumber")));
                if (declared && static_cast<int>(*declared) != object.entries.size()) {
                    warnings << QCoreApplication::translate("EdsParser", "0x%1: SubNumber=%2 fakat %3 alt nesne bulundu.")
                                    .arg(index, 4, 16, QLatin1Char('0'))
                                    .arg(*declared)
                                    .arg(object.entries.size());
                }
            }
        }

        if (object.name.isEmpty()) {
            warnings << QCoreApplication::translate("EdsParser", "0x%1: ParameterName eksik.").arg(index, 4, 16, QLatin1Char('0'));
        }
        dictionary.InsertObject(object);
    }

    if (dictionary.IsEmpty()) {
        errorText = QCoreApplication::translate("EdsParser", "Dosyada CANopen nesnesi bulunamadı; geçerli bir EDS dosyası değil.");
        return std::nullopt;
    }

    // CiA 301 zorunlu nesneleri
    const quint16 mandatory[] = {0x1000, 0x1001, 0x1018};
    for (const quint16 index : mandatory) {
        if (dictionary.FindObject(index) == nullptr) {
            warnings << QCoreApplication::translate("EdsParser", "Zorunlu nesne 0x%1 bulunamadı.").arg(index, 4, 16, QLatin1Char('0'));
        }
    }

    return dictionary;
}

bool EdsParser::ReadIniDocument(const QString &filePath, IniDocument &document, OrderedSections &orderedSections,
                                QString &errorText)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorText = QCoreApplication::translate("EdsParser", "Dosya açılamadı: %1 (%2)").arg(QFileInfo(filePath).fileName(), file.errorString());
        return false;
    }

    // EDS dosyaları çoğunlukla ASCII/ANSI'dir; geçerli UTF-8 değilse Latin-1 kabul edilir.
    const QByteArray raw = file.readAll();
    QString text = QString::fromUtf8(raw);
    if (text.contains(QChar::ReplacementCharacter)) {
        text = QString::fromLatin1(raw);
    }

    QString currentSection;
    bool hasSection = false;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';')) || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            currentSection = line.mid(1, line.size() - 2).trimmed().toLower();
            document[currentSection]; // boş bölümleri de kaydet
            hasSection = true;
            continue;
        }
        const int separator = line.indexOf(QLatin1Char('='));
        if (!hasSection || separator <= 0) {
            continue;
        }
        const QString originalKey = line.left(separator).trimmed();
        const QString value = line.mid(separator + 1).trimmed();
        document[currentSection].insert(originalKey.toLower(), value);
        orderedSections[currentSection].append(qMakePair(originalKey, value));
    }
    return true;
}

OdEntry EdsParser::ParseEntry(const IniSection &section, quint16 index, quint8 subIndex)
{
    OdEntry entry;
    entry.index = index;
    entry.subIndex = subIndex;
    entry.name = section.value(QStringLiteral("parametername"));
    entry.dataType = static_cast<quint16>(ParseEdsNumber(section.value(QStringLiteral("datatype"))).value_or(0));
    entry.accessType = section.value(QStringLiteral("accesstype")).toLower();
    entry.defaultValue = section.value(QStringLiteral("defaultvalue"));
    entry.parameterValue = section.value(QStringLiteral("parametervalue"));
    entry.lowLimit = section.value(QStringLiteral("lowlimit"));
    entry.highLimit = section.value(QStringLiteral("highlimit"));
    entry.pdoMapping = ParseEdsNumber(section.value(QStringLiteral("pdomapping"))).value_or(0) != 0;
    return entry;
}

void EdsParser::ExpandCompactSubObjects(const IniDocument &document, const IniSection &section,
                                        OdObject &object, int count)
{
    const QString indexText = QStringLiteral("%1").arg(object.index, 4, 16, QLatin1Char('0'));
    const IniSection names = document.value(indexText + QStringLiteral("name"));
    const IniSection values = document.value(indexText + QStringLiteral("value"));

    // Sub 0: alt girdi sayısı
    OdEntry countEntry;
    countEntry.index = object.index;
    countEntry.subIndex = 0;
    countEntry.name = QCoreApplication::translate("EdsParser", "Number of entries");
    countEntry.dataType = CanOpenDataType::Unsigned8;
    countEntry.accessType = QStringLiteral("ro");
    countEntry.defaultValue = QString::number(count);
    object.entries.append(countEntry);

    const OdEntry templateEntry = ParseEntry(section, object.index, 0);
    for (int sub = 1; sub <= count; ++sub) {
        OdEntry entry = templateEntry;
        entry.subIndex = static_cast<quint8>(sub);
        const QString key = QString::number(sub);
        entry.name = names.value(key, QStringLiteral("%1%2").arg(object.name).arg(sub));
        entry.defaultValue = values.value(key, templateEntry.defaultValue);
        entry.parameterValue.clear(); // CompactSubObj nesnelerinde DCF değeri desteklenmez
        object.entries.append(entry);
    }
}
