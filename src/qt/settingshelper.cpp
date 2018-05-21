#include "settingshelper.h"

#include <QString>
#include <QSettings>

int SettingsHelper::IncrementAuthFailCount()
{
    int currentCount = SettingsHelper::GetAuthFailCount();
    SettingsHelper::SetAuthFailCount(++currentCount);
    return currentCount;
}

void SettingsHelper::ResetAuthFailCount()
{
    SettingsHelper::SetAuthFailCount(0);
}

int SettingsHelper::GetAuthFailCount()
{
    QSettings settings;
    return settings.value("nAuthFailCount", -1).toInt();
}

void SettingsHelper::SetAuthFailCount(int failCount)
{
    QSettings settings;
    settings.setValue("nAuthFailCount", failCount);
}

void SettingsHelper::PutPinCode(const std::string &pin)
{
    QSettings settings;
    settings.setValue("strPinCode", QString(pin.c_str()));
}

std::string SettingsHelper::GetPinCode()
{
    QSettings settings;
    QString pin = settings.value("strPinCode", "noPin").toString();
    return pin.toStdString();
}

void SettingsHelper::SetAuthRequested(bool authRequested)
{
    QSettings settings;
    settings.setValue("fAuthRequested", authRequested);
}

bool SettingsHelper::IsAuthRequested()
{
    QSettings settings;
    return settings.value("fAuthRequested", false).toBool();
}

bool SettingsHelper::ShouldSetNewPin()
{
    return SettingsHelper::GetPinCode().compare("noPin") == 0;
}
