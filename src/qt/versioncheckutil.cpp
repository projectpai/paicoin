#include "versioncheckutil.h"
#include "util.h"
#include "clientversion.h"
#include "paicoin-config.h"

#include <QJsonDocument>
#include <QJsonObject>

bool VersionCheckUtil::versionChecked = false;

VersionCheckUtil::VersionCheckUtil(QObject *parent) : QObject(parent)
{
    networkAccessManager = new QNetworkAccessManager();

    connect(networkAccessManager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(ProcessNetworkReply(QNetworkReply*)));
}

VersionCheckUtil::~VersionCheckUtil()
{
    delete networkAccessManager;
}

VersionCheckUtil& VersionCheckUtil::getInstance()
{
    static VersionCheckUtil instance(nullptr);
    return instance;
}

void VersionCheckUtil::Check()
{
    if (!VersionCheckUtil::versionChecked)
    {
        networkRequest.setUrl(QString(CLIENT_VERSION_CHECK_URL));
        networkAccessManager->get(networkRequest);
    }
}

void VersionCheckUtil::ProcessNetworkReply(QNetworkReply *networkReply)
{
    if (networkReply->error())
    {
        LogPrintf("Version check failed: %s", networkReply->errorString().toStdString());
    }
    else
    {
        QByteArray responseData = networkReply->readAll();
        QJsonDocument jsonDocument = QJsonDocument::fromJson(responseData);
        if (jsonDocument.isObject())
        {
            QJsonObject jsonObject = jsonDocument.object();
            QJsonObject jsonObjectStatus = jsonObject["status"].toObject();
            if (jsonObjectStatus["status"].toString() == QString("SUCCESS"))
            {
                QString version = jsonObject["content"].toString();
                LogPrintf("Version received: %s\n", version.toStdString());
                if (ToClientVersion(version.toStdString()) > CLIENT_VERSION)
                {
                    Q_EMIT UpdateNeeded();
                }
            }
        }
    }
    networkReply->deleteLater();
    VersionCheckUtil::versionChecked = true;
}
