#ifndef AUTHMANAGER_H
#define AUTHMANAGER_H

#include "wallet/wallet.h"

#include <string>
#include <QObject>

class AuthManager : public QObject
{
    Q_OBJECT
public:
    static AuthManager& getInstance();
    AuthManager(AuthManager const&)     = delete;
    void operator=(AuthManager const&)  = delete;
private:
    AuthManager(QObject *parent) : QObject(parent), wallet(nullptr) {}
    CWallet* wallet;
public:
    void ConnectWallet(CWallet* wallet);
    bool Check(const std::string& pin);
    bool AuthRequested();
    void SetPinCode(const std::string& pin);
    void Reset();
    bool ShouldSet();
    void TriggerTimer();
Q_SIGNALS:
    void Authenticate(bool enteredPinInvalid = false);
    void Authenticated();
public Q_SLOTS:
    void RequestCheck(const std::string &pin);
private Q_SLOTS:
    void RequestAuthenticate();
};

#endif // AUTHMANAGER_H
