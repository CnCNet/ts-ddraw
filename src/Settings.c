#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include "IDirectDraw.h"
#include "main.h"

static bool GetBool(LPCTSTR key, bool defaultValue);
#define GetInt(a,b) GetPrivateProfileInt(SettingsSection,a,b,SettingsPath)
#define GetString(a,b,c,d) GetPrivateProfileString(SettingsSection,a,b,c,d,SettingsPath)

static const char SettingsSection[] = "ddraw";
static const char SettingsPath[] = ".\\ddraw.ini";

void SettingsLoad()
{
    MaintainAspectRatio = GetBool("MaintainAspectRatio", MaintainAspectRatio);
    Windowboxing = GetBool("Windowboxing", Windowboxing);
    StretchToFullscreen = GetBool("StretchToFullscreen", StretchToFullscreen);
    StretchToWidth = GetInt("StretchToWidth", StretchToWidth);
    StretchToHeight = GetInt("StretchToHeight", StretchToHeight);
    DrawFPS = GetInt("DrawFPS", DrawFPS);
    if (( SingleProcAffinity = GetBool("SingleProcAffinity", true) ))
    {
        SetProcessAffinityMask(GetCurrentProcess(), 1);
    }
    else
    {
        DWORD systemAffinity;
        DWORD procAffinity;
        HANDLE proc = GetCurrentProcess();
        if (GetProcessAffinityMask(proc, &procAffinity, &systemAffinity))
            SetProcessAffinityMask(proc, systemAffinity);
    }
}

static bool GetBool(LPCTSTR key, bool defaultValue)
{
    char value[8];
    GetString(key, defaultValue ? "Yes" : "No", value, 8);
    return (_strcmpi(value, "yes") == 0 || _strcmpi(value, "true") == 0 || _strcmpi(value, "1") == 0);
}
