#include "authmanager.h"
#include "settingshelper.h"

#include <qtimer.h>

AuthManager& AuthManager::getInstance()
{
    static AuthManager instance(nullptr);
    return instance;
}

bool AuthManager::Check(const std::string& pin)
{
    std::string storedPin = SettingsHelper::GetPinCode();
    bool fMatch = storedPin.compare(pin) == 0;
    if (!fMatch)
    {
        if (SettingsHelper::IncrementAuthFailCount() >= 3)
        {
            // TODO: Perform back-off action (i.e. disable wallet temporary)
        }
    }
    else
    {
        SettingsHelper::ResetAuthFailCount();
    }
    return fMatch;
}

bool AuthManager::AuthRequested()
{
    return SettingsHelper::IsAuthRequested();
}

void AuthManager::SetPinCode(const std::string& pin)
{
    SettingsHelper::ResetAuthFailCount();
    SettingsHelper::PutPinCode(pin); // Should store last pin used time as well
    QTimer::singleShot(5 * 60 * 1000, this, SLOT(RequestAuthenticate()));
}

void AuthManager::Reset()
{
    SettingsHelper::ResetAuthFailCount();
    SettingsHelper::PutPinCode("noPin");
}

void AuthManager::RequestCheck(const std::string &pin)
{
    if (Check(pin))
        Q_EMIT Authenticated();
    else
        Q_EMIT Authenticate();
}

void AuthManager::RequestAuthenticate()
{
    Q_EMIT Authenticate();
    SettingsHelper::SetAuthRequested(true);
}
