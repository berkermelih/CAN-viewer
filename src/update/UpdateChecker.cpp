#include "UpdateChecker.h"

#include "../AppInfo.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

QVersionNumber UpdateChecker::CurrentVersion()
{
    return QVersionNumber::fromString(QString::fromLatin1(AppInfo::Version));
}

void UpdateChecker::CheckForUpdates()
{
    if (m_busy) {
        return;
    }
    m_busy = true;
    const QUrl url(QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest")
                       .arg(QString::fromLatin1(AppInfo::GitHubOwner), QString::fromLatin1(AppInfo::GitHubRepository)));
    QNetworkRequest request(url);
    // GitHub API bir User-Agent ister; genel bir uygulama adı kullanılır.
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("CanbusAnalyzer-UpdateCheck"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(10000);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { HandleReply(reply); });
}

void UpdateChecker::HandleReply(QNetworkReply *reply)
{
    reply->deleteLater();
    m_busy = false;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 404) {
        emit NoReleases();
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        emit CheckFailed(reply->errorString());
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
    ReleaseInfo release;
    release.tag = root.value(QStringLiteral("tag_name")).toString();
    release.name = root.value(QStringLiteral("name")).toString();
    release.notes = root.value(QStringLiteral("body")).toString();
    release.pageUrl = QUrl(root.value(QStringLiteral("html_url")).toString());
    QString versionText = release.tag;
    if (versionText.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        versionText.remove(0, 1);
    }
    release.version = QVersionNumber::fromString(versionText);
    const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &asset : assets) {
        const QString name = asset.toObject().value(QStringLiteral("name")).toString();
        if (name.endsWith(QLatin1String(".zip"), Qt::CaseInsensitive)) {
            release.downloadUrl = QUrl(asset.toObject().value(QStringLiteral("browser_download_url")).toString());
            break;
        }
    }

    if (release.tag.isEmpty() || release.version.isNull()) {
        emit CheckFailed(tr("GitHub yanıtında geçerli bir sürüm etiketi yok (%1).").arg(release.tag));
        return;
    }
    if (release.version > CurrentVersion()) {
        emit UpdateAvailable(release);
    } else {
        emit UpToDate(release);
    }
}
