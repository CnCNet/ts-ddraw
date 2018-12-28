#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include "IDirectDraw.h"
#include "main.h"

static bool GetBool(LPCTSTR key, bool defaultValue);
LONG GetRenderer(LPCSTR key, char *defaultValue, bool *autoRenderer);
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
    InterlockedExchange(&Renderer, GetRenderer("Renderer", "auto", &AutoRenderer));

    PrimarySurface2Tex = GetBool("PrimarySurface2Tex", PrimarySurface2Tex);
    GlFinish = GetBool("GlFinish", GlFinish);
    ConvertOnGPU = GetBool("ConvertOnGPU", true);

    TargetFPS = (double)GetInt("TargetFPS", (int)TargetFPS);
    TargetFrameLen = 1000.0 / TargetFPS;

    if (GetBool("VSync", false))
        SwapInterval = 1;
    else
        SwapInterval = 0;

    InterlockedExchange(&PrimarySurfacePBO, GetInt("PrimarySurfacePBO", PrimarySurfacePBO));

    char value[8] = {0};
    GetString("SingleProcAffinity", "", value, 8);

    SingleProcAffinity = GetBool("SingleProcAffinity", !ThreadSafe);
    char singProc = value[0] ? (SingleProcAffinity ? 1 : 0) : -1;

    HANDLE proc = GetCurrentProcess();
    GetProcessAffinityMask(proc, &ProcAffinity, &SystemAffinity);

    switch (singProc)
    {
    case -1:
        // If SingleProcAffinity wasn't set
        if (GetProcessAffinityMask(proc, &ProcAffinity, &SystemAffinity))
        {
            if (ProcAffinity != SystemAffinity)
                SetProcessAffinityMask(proc, SystemAffinity);
            else
                ProcAffinity = SystemAffinity = 0;
        }
        break;

    case 1:
        // If SingleProcAffinity was set to Yes
        SetProcessAffinityMask(GetCurrentProcess(), 1);
        ProcAffinity = SystemAffinity = 0;
        break;

    case 0:
        // If SingleProcAffinit was set to No
        SetProcessAffinityMask(GetCurrentProcess(), SystemAffinity);
        SystemAffinity = ProcAffinity = 0;
        break;
    }

    MonitorEdgeTimer = GetInt("MonitorEdgeTimer", MonitorEdgeTimer);

    GlFenceSync = GetBool("GlFenceSync", GlFenceSync);
}

static bool GetBool(LPCTSTR key, bool defaultValue)
{
    char value[8];
    GetString(key, defaultValue ? "Yes" : "No", value, 8);
    return (_strcmpi(value, "yes") == 0 || _strcmpi(value, "true") == 0 || _strcmpi(value, "1") == 0);
}

LONG GetRenderer(LPCSTR key, char *defaultValue, bool *autoRenderer)
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
        else if (_strcmpi(defaultValue, "auto") == 0)
            return RENDERER_OPENGL;
    }
    return RENDERER_GDI;
}
