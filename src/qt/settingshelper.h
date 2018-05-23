#ifndef SETTINGSHELPER_H
#define SETTINGSHELPER_H

#include <string>

class SettingsHelper
{
public:
    static int IncrementAuthFailCount();
    static void ResetAuthFailCount();
    static int GetAuthFailCount();
    static void SetAuthFailCount(int failCount);
    static void PutPinCode(const std::string &pin);
    static std::string GetPinCode();
    static void SetAuthRequested(bool authRequested);
    static bool IsAuthRequested();
private:
    SettingsHelper() {}
};

#endif // SETTINGSHELPER_H
