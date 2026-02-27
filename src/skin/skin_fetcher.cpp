#include "skin/skin_fetcher.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QNetworkRequest>
#include <QDir>
#include <QStandardPaths>
#include <QFile>

SkinFetcher::SkinFetcher(QObject* parent)
    : QObject(parent)
    , nam_(new QNetworkAccessManager(this))
{
}

void SkinFetcher::fetch(const QString& username) {
    username_ = username;

    // Step 1: username → UUID
    QUrl url(QStringLiteral("https://api.mojang.com/users/profiles/minecraft/") + username);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onUuidReply(reply);
    });
}

void SkinFetcher::onUuidReply(QNetworkReply* reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 204 || status == 404) {
            emit error(tr("找不到用户: %1").arg(username_));
        } else {
            emit error(tr("查询 UUID 失败: %1").arg(reply->errorString()));
        }
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) {
        emit error(tr("Mojang API 返回了无效的 JSON"));
        return;
    }

    QString uuid = doc.object().value("id").toString();
    if (uuid.isEmpty()) {
        emit error(tr("找不到用户: %1").arg(username_));
        return;
    }

    // Step 2: UUID → profile (contains base64 textures)
    QUrl profileUrl(QStringLiteral("https://sessionserver.mojang.com/session/minecraft/profile/") + uuid);
    QNetworkRequest req(profileUrl);

    auto* profileReply = nam_->get(req);
    connect(profileReply, &QNetworkReply::finished, this, [this, profileReply]() {
        onProfileReply(profileReply);
    });
}

void SkinFetcher::onProfileReply(QNetworkReply* reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit error(tr("查询玩家档案失败: %1").arg(reply->errorString()));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) {
        emit error(tr("玩家档案返回了无效的 JSON"));
        return;
    }

    // Parse properties array, find "textures" entry
    QJsonArray properties = doc.object().value("properties").toArray();
    QString texturesBase64;
    for (const auto& prop : properties) {
        QJsonObject obj = prop.toObject();
        if (obj.value("name").toString() == "textures") {
            texturesBase64 = obj.value("value").toString();
            break;
        }
    }

    if (texturesBase64.isEmpty()) {
        emit error(tr("该玩家没有皮肤数据"));
        return;
    }

    // Decode base64 → JSON → skin URL
    QByteArray decoded = QByteArray::fromBase64(texturesBase64.toUtf8());
    QJsonDocument texDoc = QJsonDocument::fromJson(decoded);
    if (!texDoc.isObject()) {
        emit error(tr("皮肤纹理数据解析失败"));
        return;
    }

    QJsonObject textures = texDoc.object().value("textures").toObject();
    QJsonObject skinObj = textures.value("SKIN").toObject();
    QString skinUrl = skinObj.value("url").toString();

    if (skinUrl.isEmpty()) {
        emit error(tr("该玩家未设置自定义皮肤"));
        return;
    }

    // Step 3: download the skin PNG
    QNetworkRequest req{QUrl(skinUrl)};
    auto* skinReply = nam_->get(req);
    connect(skinReply, &QNetworkReply::finished, this, [this, skinReply]() {
        onSkinReply(skinReply);
    });
}

void SkinFetcher::onSkinReply(QNetworkReply* reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit error(tr("下载皮肤失败: %1").arg(reply->errorString()));
        return;
    }

    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        emit error(tr("下载的皮肤文件为空"));
        return;
    }

    // Save to temp directory
    QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString filePath = dir + QStringLiteral("/mcskin_") + username_ + QStringLiteral(".png");

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit error(tr("无法保存皮肤文件: %1").arg(filePath));
        return;
    }
    file.write(data);
    file.close();

    emit finished(filePath);
}
