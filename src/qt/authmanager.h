#ifndef AUTHMANAGER_H
#define AUTHMANAGER_H

#include "wallet/wallet.h"

#include <string>
#include <QObject>
#include <QTimer>

class AuthManager : public QObject
{
    Q_OBJECT
public:
    static AuthManager& getInstance();
    AuthManager(AuthManager const&)     = delete;
    void operator=(AuthManager const&)  = delete;
private:
    AuthManager(QObject *parent);
    CWallet* wallet;
    QTimer* timer;
    static const int kTimeout = 5 * 60 * 1000;
public:
    void ConnectWallet(CWallet* wallet);
    bool Check(const std::string& pin);
    bool AuthRequested();
    void SetPinCode(const std::string& pin);
    void Reset();
    bool ShouldSet();
    void TriggerTimer();
    void RetriggerTimer();
Q_SIGNALS:
    void Authenticate(bool enteredPinInvalid = false);
    void Authenticated();
public Q_SLOTS:
    void RequestCheck(const std::string &pin);
private Q_SLOTS:
    void RequestAuthenticate();
};

#endif // AUTHMANAGER_H
