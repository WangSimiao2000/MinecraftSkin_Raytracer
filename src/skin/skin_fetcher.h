#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// Fetches a Minecraft Java Edition player skin from Mojang API.
// Flow: username → UUID → profile (base64 textures) → skin PNG download.
class SkinFetcher : public QObject {
    Q_OBJECT

public:
    explicit SkinFetcher(QObject* parent = nullptr);

    // Start the fetch chain for the given username.
    void fetch(const QString& username);

signals:
    // Emitted when the skin PNG has been saved to a temp file.
    void finished(const QString& filePath);
    // Emitted on any error during the fetch chain.
    void error(const QString& message);

private slots:
    void onUuidReply(QNetworkReply* reply);
    void onProfileReply(QNetworkReply* reply);
    void onSkinReply(QNetworkReply* reply);

private:
    QNetworkAccessManager* nam_;
    QString username_;
};
