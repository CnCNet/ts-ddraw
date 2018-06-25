/*
 * Copyright (c) 2013 Toni Spets <toni.spets@iki.fi>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include "main.h"
#include "IDirectDraw.h"
#include "Settings.h"

void hook_init();

#ifdef _DEBUG
int PROXY = 0;
int VERBOSE = 1;
int TRACE = 0;
int FPS = 0;

static HANDLE real_dll = NULL;
static HRESULT (WINAPI *real_DirectDrawCreate)(GUID FAR*, LPDIRECTDRAW FAR*, IUnknown FAR*) = NULL;
#else
BOOL WINAPI DllMainCRTStartup(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) { return TRUE; }
#endif

bool TSDDRAW = true;
bool GameHandlesClose = false;
bool SingleProcAffinity = true;

#ifdef _DEBUG
int DrawFPS = 1;
#else
int DrawFPS = 0;
#endif

double TargetFPS = 60.0;
double TargetFrameLen = 1000.0 / 60.0;
LONG Renderer = RENDERER_OPENGL;
bool AutoRenderer = true;
int SwapInterval = 0;
LONG PrimarySurfacePBO = 0;
bool PrimarySurface2Tex = true;

HRESULT WINAPI DirectDrawCreate(GUID FAR* lpGUID, LPDIRECTDRAW FAR* lplpDD, IUnknown FAR* pUnkOuter)
{
    char buf[32];
    buf[0] = '\0';
#ifdef _DEBUG
    GetEnvironmentVariable("DDRAW_PROXY", buf, sizeof buf);
    if (buf[0]) PROXY = 1;
    buf[0] = '\0';
    GetEnvironmentVariable("DDRAW_LESS", buf, sizeof buf);
    if (buf[0]) VERBOSE = 0;
    buf[0] = '\0';
    GetEnvironmentVariable("DDRAW_TRACE", buf, sizeof buf);
    if (buf[0]) TRACE = 1;
    buf[0] = '\0';
    GetEnvironmentVariable("DDRAW_FPS", buf, sizeof buf);
    if (buf[0]) FPS = 1;

    if (TRACE)
    {
        freopen("stdout.txt", "w", stdout);
        setvbuf(stdout, NULL, _IOLBF, 1024);
    }
#endif
    dprintf("--> DirectDrawCreate(lpGUID=%p, lplpDD=%p, pUnkOuter=%p)\n", lpGUID, lplpDD, pUnkOuter);

    GetEnvironmentVariable("DDRAW_DRAW_FPS", buf, sizeof buf);
    if (buf[0]) DrawFPS = 1;
    buf[0] = '\0';
    GetEnvironmentVariable("DDRAW_TARGET_FPS", buf, sizeof buf);
    if (buf[0])
    {
        int tfps;
        if (sscanf(buf, "%i", &tfps) && tfps > 0)
        {
            TargetFPS = (double)(DWORD)tfps;
            TargetFrameLen = 1000.0 / TargetFPS;
        }
    }

    SettingsLoad();
    hook_init();

    IDirectDrawImpl *ddraw = IDirectDrawImpl_construct();

#ifdef _DEBUG
    if (PROXY)
    {
        real_dll = LoadLibrary("system32\\ddraw.dll");
        dprintf(" real_dll = %p\n", real_dll);
        real_DirectDrawCreate = (HRESULT(WINAPI *)(GUID FAR*, LPDIRECTDRAW FAR*, IUnknown FAR*))GetProcAddress(real_dll, "DirectDrawCreate");
        dprintf(" real_DirectDrawCreate = %p\n", real_DirectDrawCreate);
        real_DirectDrawCreate(lpGUID, &ddraw->real, pUnkOuter);
    }
    else
    {
        dprintf("\n\n");
    }
#endif

    *lplpDD = (IDirectDraw *)ddraw;
    dprintf(" lplpDD = %p\n", *lplpDD);
    dprintf("<-- DirectDrawCreate(lpGUID=%p, lplpDD=%p, pUnkOuter=%p)\n", lpGUID, lplpDD, pUnkOuter);
    return DD_OK;
}

#ifdef _DEBUG
int dprintf(const char *fmt, ...)
{
    va_list args;
    int ret;

    if (!TRACE) return 0;

    SYSTEMTIME st;
    GetLocalTime(&st);

    fprintf(stdout, "[%d] %02d:%02d:%02d.%03d ", GetCurrentThreadId(), st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    va_start(args, fmt);
    ret = vfprintf(stdout, fmt, args);
    va_end(args);
    return ret;
}
#endif

void DebugPrint(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char buffer[512];
    _vsnprintf(buffer, sizeof(buffer), format, args);
    OutputDebugStringA(buffer);
}

void dump_dwcaps(DWORD dwCaps)
{
    if (!VERBOSE) return;
    dprintf("-- DWCAPS --\n");
    if (dwCaps & DDCAPS_3D)
        dprintf("    DDCAPS_3D\n");

    if (dwCaps & DDCAPS_BLT)
        dprintf("    DDCAPS_BLT\n");

    dprintf("    Warning: Not all dwCaps printed!\n");
    dprintf("------------\n");
}

void dump_ddcaps(LPDDCAPS lpDDCaps)
{
    if (!VERBOSE) return;
    dprintf("-- DDCAPS --\n");
    dprintf("    dwSize     = %d\n", (int)lpDDCaps->dwSize);
    dprintf("    dwCaps     = %08X\n", (int)lpDDCaps->dwCaps);
    dprintf("    dwCaps2    = %08X\n", (int)lpDDCaps->dwCaps2);
    dprintf("------------\n");
    dump_dwcaps(lpDDCaps->dwCaps);
}

void dump_ddscaps(LPDDSCAPS lpDDSCaps)
{
    if (!VERBOSE) return;
    dprintf("-- DDSCAPS --\n");

    if (lpDDSCaps->dwCaps & DDSCAPS_ALPHA)
        dprintf("    DDSCAPS_ALPHA\n");

    if (lpDDSCaps->dwCaps & DDSCAPS_BACKBUFFER)
        dprintf("    DDSCAPS_BACKBUFFER\n");

#if 0
    if (lpDDSCaps->dwCaps & DDSCAPS_DYNAMIC)
        dprintf("    DDSCAPS_DYNAMIC\n");
#endif

    if (lpDDSCaps->dwCaps & DDSCAPS_FLIP)
        dprintf("    DDSCAPS_FLIP\n");

    if (lpDDSCaps->dwCaps & DDSCAPS_FRONTBUFFER)
        dprintf("    DDSCAPS_FRONTBUFFER\n");

    if (lpDDSCaps->dwCaps & DDSCAPS_OVERLAY)
        dprintf("    DDSCAPS_OVERLAY\n");

    if (lpDDSCaps->dwCaps & DDSCAPS_PALETTE)
        dprintf("    DDSCAPS_PALETTE\n");

    if (lpDDSCaps->dwCaps & DDSCAPS_PRIMARYSURFACE)
        dprintf("    DDSCAPS_PRIMARYSURFACE\n");

#if 0
    if (lpDDSCaps->dwCaps & DDSCAPS_READONLY)
        dprintf("    DDSCAPS_READONLY\n");
#endif

    if (lpDDSCaps->dwCaps & DDSCAPS_SYSTEMMEMORY)
        dprintf("    DDSCAPS_SYSTEMMEMORY\n");

    if (lpDDSCaps->dwCaps & DDSCAPS_VIDEOMEMORY)
        dprintf("    DDSCAPS_VIDEOMEMORY\n");

    if (lpDDSCaps->dwCaps & DDSCAPS_WRITEONLY)
        dprintf("    DDSCAPS_WRITEONLY\n");

    dprintf("-------------\n");
}

void dump_ddsurfacedesc(LPDDSURFACEDESC lpDDSurfaceDesc)
{
    if (!VERBOSE) return;
    dprintf("        dwSize             = %d\n", (int)lpDDSurfaceDesc->dwSize);
    dprintf("        dwFlags            = %08X\n", (int)lpDDSurfaceDesc->dwFlags);

    if (lpDDSurfaceDesc->dwFlags & DDSD_BACKBUFFERCOUNT)
        dprintf("            DDSD_BACKBUFFERCOUNT\n");

    if (lpDDSurfaceDesc->dwFlags & DDSD_CAPS)
        dprintf("            DDSD_CAPS\n");

    if (lpDDSurfaceDesc->dwFlags & DDSD_CKDESTBLT)
        dprintf("            DDSD_CKDESTBLT\n");

    if (lpDDSurfaceDesc->dwFlags & DDSD_CKDESTOVERLAY)
        dprintf("            DDSD_CKDESTOVERLAY\n");

    if (lpDDSurfaceDesc->dwFlags & DDSD_CKSRCBLT)
        dprintf("            DDSD_CKSRCBLT\n");

    if (lpDDSurfaceDesc->dwFlags & DDSD_CKSRCOVERLAY)
        dprintf("            DDSD_CKSRCOVERLAY\n");

    if (lpDDSurfaceDesc->dwFlags & DDSD_HEIGHT)
        dprintf("            DDSD_HEIGHT\n");

    if (lpDDSurfaceDesc->dwFlags & DDSD_LPSURFACE)
        dprintf("            DDSD_LPSURFACE\n");

    if (lpDDSurfaceDesc->dwFlags & DDSD_PITCH)
        dprintf("            DDSD_PITCH\n");

    if (lpDDSurfaceDesc->dwFlags & DDSD_PIXELFORMAT)
        dprintf("            DDSD_PIXELFORMAT\n");

    if (lpDDSurfaceDesc->dwFlags & DDSD_REFRESHRATE)
        dprintf("            DDSD_REFRESHRATE\n");

#if 0
    if (lpDDSurfaceDesc->dwFlags & DDSD_SURFACESIZE)
        dprintf("            DDSD_SURFACESIZE\n");
#endif

    if (lpDDSurfaceDesc->dwFlags & DDSD_WIDTH)
        dprintf("            DDSD_WIDTH\n");

#if 0
    if (lpDDSurfaceDesc->dwFlags & DDSD_XPITCH)
        dprintf("            DDSD_XPITCH\n");
#endif

    //if (lpDDSurfaceDesc->dwFlags & DDSD_WIDTH)
        dprintf("        dwWidth            = %d\n", (int)lpDDSurfaceDesc->dwWidth);

    //if (lpDDSurfaceDesc->dwFlags & DDSD_HEIGHT)
        dprintf("        dwHeight           = %d\n", (int)lpDDSurfaceDesc->dwHeight);

    //if (lpDDSurfaceDesc->dwFlags & DDSD_PITCH)
        dprintf("        lPitch             = %d\n", (int)lpDDSurfaceDesc->lPitch);

    //if (lpDDSurfaceDesc->dwFlags & DDSD_BACKBUFFERCOUNT )
        dprintf("        dwBackBufferCount  = %d\n", (int)lpDDSurfaceDesc->lPitch);

    //if (lpDDSurfaceDesc->dwFlags & DDSD_REFRESHRATE)
        dprintf("        dwRefreshRate      = %d\n", (int)lpDDSurfaceDesc->dwRefreshRate);

    //if (lpDDSurfaceDesc->dwFlags & DDSD_LPSURFACE)
        dprintf("        lpSurface          = %p\n", lpDDSurfaceDesc->lpSurface);

    //if (lpDDSurfaceDesc->dwFlags & DDSD_CAPS)
        dump_ddscaps(&lpDDSurfaceDesc->ddsCaps);

    dump_ddpixelformat(&lpDDSurfaceDesc->ddpfPixelFormat);
}

void dump_ddpixelformat(LPDDPIXELFORMAT pfd)
{
    dprintf("       pfd->dwSize             = %d\n", (int)pfd->dwSize);
    dprintf("       pfd->dwFlags            = %08X\n", (int)pfd->dwFlags);
    dprintf("       pfd->dwFourCC           = %08X\n", (int)pfd->dwFourCC);
    dprintf("       pfd->dwRGBBitCount      = %08X\n", (int)pfd->dwRGBBitCount);
    dprintf("       pfd->dwRBitMask         = %08X\n", (int)pfd->dwRBitMask);
    dprintf("       pfd->dwGBitMask         = %08X\n", (int)pfd->dwGBitMask);
    dprintf("       pfd->dwBBitMask         = %08X\n", (int)pfd->dwBBitMask);
    dprintf("       pfd->dwRGBAlphaBitMask  = %08X\n", (int)pfd->dwRGBAlphaBitMask);
}

void dump_ddbltfx(LPDDBLTFX lpDDBltFx)
{
    if (!VERBOSE) return;

    dprintf("       lpDDBltFx->dwSize           = %d\n", (int)lpDDBltFx->dwSize);
    dprintf("       lpDDBltFx->dwDDFX           = %08X\n", (int)lpDDBltFx->dwDDFX);
    dprintf("       lpDDBltFx->dwROP            = %08X\n", (int)lpDDBltFx->dwROP);
    dprintf("       lpDDBltFx->dwDDROP          = %08X\n", (int)lpDDBltFx->dwDDROP);
    dprintf("       lpDDBltFx->dwRotationAngle  = %08X\n", (int)lpDDBltFx->dwRotationAngle);
    dprintf("       lpDDBltFx->dwFillColor      = %08X\n", (int)lpDDBltFx->dwFillColor);
}

BOOL IsWindowsXp()
{
    DWORD version = GetVersion();
    DWORD major = (DWORD)(LOBYTE(LOWORD(version)));
    DWORD minor = (DWORD)(HIBYTE(LOWORD(version)));
    if ((major == 5) && (minor == 1))
    {
        // Check for wine
        HANDLE dll = GetModuleHandleA("ntdll.dll");
        return !GetProcAddress(dll, "wine_get_version");
    }
    return false;
}
