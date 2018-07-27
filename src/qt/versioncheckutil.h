#ifndef VERSIONCHECKUTIL_H
#define VERSIONCHECKUTIL_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

class VersionCheckUtil : public QObject
{
    Q_OBJECT
public:
    ~VersionCheckUtil();
    static VersionCheckUtil& getInstance();
    VersionCheckUtil(VersionCheckUtil const&) = delete;
    void operator=(VersionCheckUtil const&)   = delete;
private:
    VersionCheckUtil(QObject *parent);
public:
    void Check();
Q_SIGNALS:
    void UpdateNeeded();
private Q_SLOTS:
    void ProcessNetworkReply(QNetworkReply *networkReply);
private:
    QNetworkAccessManager *networkAccessManager;
    QNetworkRequest networkRequest;
    static bool versionChecked;
};

#endif // VERSIONCHECKUTIL_H
