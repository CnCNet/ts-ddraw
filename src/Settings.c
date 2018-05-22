#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include "IDirectDraw.h"
#include "main.h"

static bool GetBool(LPCTSTR key, bool defaultValue);
RendererType GetRenderer(LPCSTR key, char *defaultValue, bool *autoRenderer);
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
    Renderer = GetRenderer("Renderer", "opengl", &AutoRenderer);

    if (GetBool("VSync", false))
        SwapInterval = 1;
    else
        SwapInterval = 0;

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

RendererType GetRenderer(LPCSTR key, char *defaultValue, bool *autoRenderer)
{
    char value[256];
    GetString(key, defaultValue, value, 256);

    if (_strcmpi(value, "opengl") == 0)
    {
        *autoRenderer = false;
        return RENDERER_OPENGL;
    }
    else if (_strcmpi(value, "gdi") == 0)
    {
        *autoRenderer = false;
        return RENDERER_GDI;
    }
    else if (_strcmpi(value, "auto") == 0)
    {
        *autoRenderer = true;
        if (_strcmpi(defaultValue, "opengl") == 0)
            return RENDERER_OPENGL;
        else if (_strcmpi(defaultValue, "gdi") == 0)
            return RENDERER_GDI;
    }
    return RENDERER_GDI;
}
