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

#include "main.h"
#include "IDirectDraw.h"
#include "IDirectDrawClipper.h"
#include "IDirectDrawSurface.h"

static IDirectDrawImplVtbl Vtbl;

IDirectDrawImpl *IDirectDrawImpl_construct()
{
    IDirectDrawImpl *this = calloc(1, sizeof(IDirectDrawImpl));
    this->lpVtbl = &Vtbl;
    this->dd = this;
    dprintf("IDirectDraw::construct() -> %p\n", this);
    this->ref++;
    InitializeCriticalSection(&this->cs);
    return this;
}

static HRESULT __stdcall _QueryInterface(IDirectDrawImpl *this, REFIID riid, void **obj)
{
    ENTER;
    HRESULT ret = IDirectDraw_QueryInterface(this->real, riid, obj);
    dprintf("IDirectDraw::QueryInterface(this=%p, riid=%p, obj=%p) -> %08X\n", this, riid, obj, (int)ret);
    LEAVE;
    return ret;
}

static ULONG __stdcall _AddRef(IDirectDrawImpl *this)
{
    ENTER;
    ULONG ret = IDirectDraw_AddRef(this->real);
    dprintf("IDirectDraw::AddRef(this=%p) -> %08X\n", this, (int)ret);
    LEAVE;
    return ret;
}

static ULONG __stdcall _Release(IDirectDrawImpl *this)
{
    ENTER;
    ULONG ret = IDirectDraw_Release(this->real);
    dprintf("IDirectDraw::Release(this=%p) -> %08X\n", this, (int)ret);
    LEAVE;
    return ret;
}

static HRESULT __stdcall _Compact(IDirectDrawImpl *this)
{
    HRESULT ret = IDirectDraw_Compact(this->real);
    dprintf("IDirectDraw::Compact(this=%p) -> %08X\n", this, (int)ret);
    return ret;
}

static HRESULT __stdcall _CreatePalette(IDirectDrawImpl *this, DWORD dwFlags, LPPALETTEENTRY lpDDColorArray, LPDIRECTDRAWPALETTE FAR * lplpDDPalette, IUnknown FAR * pUnkOuter)
{
    HRESULT ret = IDirectDraw_CreatePalette(this->real, dwFlags, lpDDColorArray, lplpDDPalette, pUnkOuter);
    dprintf("IDirectDraw::CreatePalette(this=%p, dwFlags=%d, lpDDColorArray=%p, lplpDDPalette=%p, pUnkOuter=%p) -> %08X\n", this, (int)dwFlags, lpDDColorArray, lplpDDPalette, pUnkOuter, (int)ret);
    return ret;
}

static HRESULT __stdcall _CreateClipper(IDirectDrawImpl *this, DWORD dwFlags, LPDIRECTDRAWCLIPPER FAR *lplpDDClipper, IUnknown FAR *pUnkOuter)
{
    ENTER;
    HRESULT ret = DD_OK;
    IDirectDrawClipperImpl *impl = IDirectDrawClipperImpl_construct();
    *lplpDDClipper = (IDirectDrawClipper *)impl;

    if (PROXY)
    {
        ret = IDirectDraw_CreateClipper(this->real, dwFlags, &impl->real, pUnkOuter);
    }

    dprintf("IDirectDraw::CreateClipper(this=%p, dwFlags=%d, lplpDDClipper=%p, unkOuter=%p) -> %08X\n", this, (int)dwFlags, lplpDDClipper, pUnkOuter, (int)ret);
    LEAVE;
    return ret;
}

static HRESULT __stdcall _CreateSurface(IDirectDrawImpl *this, LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE FAR *lplpDDSurface, IUnknown FAR * pUnkOuter)
{
    ENTER;
    HRESULT ret = DD_OK;
    IDirectDrawSurfaceImpl *impl = IDirectDrawSurfaceImpl_construct(this, lpDDSurfaceDesc);
    *lplpDDSurface = (IDirectDrawSurface *)impl;

    if (PROXY)
    {
        ret = IDirectDraw_CreateSurface(this->real, lpDDSurfaceDesc, &impl->real, pUnkOuter);
    }

    dprintf("IDirectDraw::CreateSurface(this=%p, lpDDSurfaceDesc=%p, lplpDDSurface=%p, pUnkOuter=%p) -> %08X\n", this, lpDDSurfaceDesc, lplpDDSurface, pUnkOuter, (int)ret);

    if (VERBOSE)
    {
        dprintf("    Surface description:\n");
        dump_ddsurfacedesc(lpDDSurfaceDesc);
    }

    LEAVE;
    return ret;
}

static HRESULT __stdcall _DuplicateSurface(IDirectDrawImpl *this, LPDIRECTDRAWSURFACE src, LPDIRECTDRAWSURFACE *dest)
{
    HRESULT ret = IDirectDraw_DuplicateSurface(this->real, src, dest);
    dprintf("IDirectDraw::DuplicateSurface(this=%p, src=%p, dest=%p) -> %08X\n", this, src, dest, (int)ret);
    return ret;
}

static HRESULT __stdcall _EnumDisplayModes(IDirectDrawImpl *this, DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumModesCallback)
{
    HRESULT ret = IDirectDraw_EnumDisplayModes(this->real, dwFlags, lpDDSurfaceDesc, lpContext, lpEnumModesCallback);
    dprintf("IDirectDraw::EnumDisplayModes(this=%p, dwFlags=%08X, lpDDSurfaceDesc=%p, lpContext=%p, lpEnumModesCallback=%p) -> %08X\n", this, (int)dwFlags, lpDDSurfaceDesc, lpContext, lpEnumModesCallback, (int)ret);
    return ret;
}

static HRESULT __stdcall _EnumSurfaces(IDirectDrawImpl *this, DWORD dwFlags, LPDDSURFACEDESC lpDDSD, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback)
{
    HRESULT ret = IDirectDraw_EnumSurfaces(this->real, dwFlags, lpDDSD, lpContext, lpEnumSurfacesCallback);
    dprintf("IDirectDraw::EnumSurfaces(this=%p, dwFlags=%08X, lpDDSD=%p, lpContext=%p, lpEnumSurfacesCallback=%p) -> %08X\n", this, (int)dwFlags, lpDDSD, lpContext, lpEnumSurfacesCallback, (int)ret);
    return ret;
}

static HRESULT __stdcall _FlipToGDISurface(IDirectDrawImpl *this)
{
    HRESULT ret = IDirectDraw_FlipToGDISurface(this->real);
    dprintf("IDirectDraw::FlipToGDISurface(this=%p) -> %08X\n", this, (int)ret);
    return ret;
}

static HRESULT __stdcall _GetCaps(IDirectDrawImpl *this, LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDEmulCaps)
{
    HRESULT ret = DD_OK;

    if (PROXY)
    {
        ret = IDirectDraw_GetCaps(this->real, lpDDDriverCaps, lpDDEmulCaps);
    }
    else
    {
        if (lpDDDriverCaps)
        {
            lpDDDriverCaps->dwSize = sizeof(DDCAPS);
            lpDDDriverCaps->dwCaps = DDCAPS_BLT|DDCAPS_PALETTE;
            lpDDDriverCaps->dwCKeyCaps = 0;
            lpDDDriverCaps->dwPalCaps = DDPCAPS_8BIT|DDPCAPS_PRIMARYSURFACE;
            lpDDDriverCaps->dwVidMemTotal = 16777216;
            lpDDDriverCaps->dwVidMemFree = 16777216;
            lpDDDriverCaps->dwMaxVisibleOverlays = 0;
            lpDDDriverCaps->dwCurrVisibleOverlays = 0;
            lpDDDriverCaps->dwNumFourCCCodes = 0;
            lpDDDriverCaps->dwAlignBoundarySrc = 0;
            lpDDDriverCaps->dwAlignSizeSrc = 0;
            lpDDDriverCaps->dwAlignBoundaryDest = 0;
            lpDDDriverCaps->dwAlignSizeDest = 0;
        }

        if (lpDDEmulCaps)
        {
            memset(lpDDEmulCaps, 0, sizeof(*lpDDEmulCaps));
            lpDDDriverCaps->dwSize = sizeof(*lpDDEmulCaps);
        }
    }

    dprintf("IDirectDraw::GetCaps(this=%p, lpDDDriverCaps=%p, lpDDEmulCaps=%p) -> %08X\n", this, lpDDDriverCaps, lpDDEmulCaps, (int)ret);

    if (lpDDDriverCaps && VERBOSE)
    {
        dprintf("    Hardware capabilities:\n");
        dump_ddcaps(lpDDDriverCaps);
    }

    if (lpDDEmulCaps && VERBOSE)
    {
        dprintf("    HEL capabilities:\n");
        dump_ddcaps(lpDDDriverCaps);
    }

    return ret;
}

static HRESULT __stdcall _GetDisplayMode(IDirectDrawImpl *this, LPDDSURFACEDESC lpDDSurfaceDesc)
{
    HRESULT ret = IDirectDraw_GetDisplayMode(this->real, lpDDSurfaceDesc);
    dprintf("IDirectDraw::GetDisplayMode(this=%p, lpDDSurfaceDesc=%p) -> %08X\n", this, lpDDSurfaceDesc, (int)ret);
    return ret;
}

static HRESULT __stdcall _GetFourCCCodes(IDirectDrawImpl *this, LPDWORD lpNumCodes, LPDWORD lpCodes)
{
    HRESULT ret = IDirectDraw_GetFourCCCodes(this->real, lpNumCodes, lpCodes);
    dprintf("IDirectDraw::GetFourCCCodes(this=%p, lpNumCodes=%p, lpCodes=%p) -> %08X\n", this, lpNumCodes, lpCodes, (int)ret);
    return ret;
}

static HRESULT __stdcall _GetGDISurface(IDirectDrawImpl *this, LPDIRECTDRAWSURFACE *lplpGDIDDSSurface)
{
    HRESULT ret = IDirectDraw_GetGDISurface(this->real, lplpGDIDDSSurface);
    dprintf("IDirectDraw::GetGDISurface(this=%p, lplpGDIDDSSurface=%p) -> %08X\n", this, lplpGDIDDSSurface, (int)ret);
    return ret;
}

static HRESULT __stdcall _GetMonitorFrequency(IDirectDrawImpl *this, LPDWORD lpdwFrequency)
{
    HRESULT ret = IDirectDraw_GetMonitorFrequency(this->real, lpdwFrequency);
    dprintf("IDirectDraw::GetMonitorFrequency(this=%p, lpdwFrequency=%p) -> %08X\n", this, lpdwFrequency, (int)ret);
    return ret;
}

static HRESULT __stdcall _GetScanLine(IDirectDrawImpl *this, LPDWORD lpdwScanLine)
{
    HRESULT ret = IDirectDraw_GetScanLine(this->real, lpdwScanLine);
    dprintf("IDirectDraw::GetScanLine(this=%p, lpdwScanLine=%p) -> %08X\n", this, lpdwScanLine, (int)ret);
    return ret;
}

static HRESULT __stdcall _GetVerticalBlankStatus(IDirectDrawImpl *this, LPBOOL lpbIsInVB)
{
    HRESULT ret = IDirectDraw_GetVerticalBlankStatus(this->real, lpbIsInVB);
    dprintf("IDirectDraw::GetVerticalBlankStatus(this=%p, lpbIsInVB=%s) -> %08X\n", this, (lpbIsInVB ? "TRUE" : "FALSE"), (int)ret);
    return ret;
}

static HRESULT __stdcall _Initialize(IDirectDrawImpl *this, GUID *lpGUID)
{
    HRESULT ret = IDirectDraw_Initialize(this->real, lpGUID);
    dprintf("IDirectDraw::Initialize(this=%p, lpGUID=%p) -> %08X\n", this, lpGUID, (int)ret);
    return ret;
}

static HRESULT __stdcall _RestoreDisplayMode(IDirectDrawImpl *this)
{
    HRESULT ret = IDirectDraw_RestoreDisplayMode(this->real);
    dprintf("IDirectDraw::RestoreDisplayMode(this=%p) -> %08X\n", this, (int)ret);
    return ret;
}

static HRESULT __stdcall _SetDisplayMode(IDirectDrawImpl *this, DWORD width, DWORD height, DWORD bpp)
{
    ENTER;
    HRESULT ret = DD_OK;

    if (PROXY)
    {
        ret = IDirectDraw_SetDisplayMode(this->real, width, height, bpp);
    }
    else
    {
        if (bpp != 16)
            return DDERR_INVALIDMODE;

        this->width = width;
        this->height = height;
        this->bpp = bpp;

        PIXELFORMATDESCRIPTOR pfd;

        memset(&pfd, 0, sizeof(pfd));
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW|PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = this->bpp;
        pfd.iLayerType = PFD_MAIN_PLANE;
        if (!SetPixelFormat( this->hDC, ChoosePixelFormat( this->hDC, &pfd ), &pfd ))
        {
            dprintf("SetPixelFormat failed!\n");
            ret = DDERR_UNSUPPORTED;
        }

        SetWindowPos(this->hWnd, HWND_TOPMOST, 0, 0, this->width, this->height, SWP_SHOWWINDOW);

        DEVMODE mode;
        memset(&mode, 0, sizeof(mode));
        mode.dmSize = sizeof(mode);
        mode.dmFields = DM_PELSWIDTH|DM_PELSHEIGHT;
        mode.dmPelsWidth = this->width;
        mode.dmPelsHeight = this->height;
        /*
        mode.dmFields |= DM_BITSPERPEL;
        mode.dmBitsPerPel = this->bpp;
        */

        if (ChangeDisplaySettings(&mode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
        {
            dprintf("    mode change failed!\n");
            return DDERR_INVALIDMODE;
        }
    }

    dprintf("IDirectDraw::SetDisplayMode(this=%p, width=%d, height=%d, bpp=%d) -> %08X\n", this, (int)width, (int)height, (int)bpp, (int)ret);
    LEAVE;
    return ret;
}

static HRESULT __stdcall _SetCooperativeLevel(IDirectDrawImpl *this, HWND hWnd, DWORD dwFlags)
{
    ENTER;
    HRESULT ret = DD_OK;

    if (PROXY)
    {
        ret = IDirectDraw_SetCooperativeLevel(this->real, hWnd, dwFlags);
    }
    else
    {
        this->hWnd = hWnd;
        this->hDC = GetDC(this->hWnd);
    }

    dprintf("IDirectDraw::SetCooperativeLevel(this=%p, hWnd=%08X, dwFlags=%08X) -> %08X\n", this, (int)hWnd, (int)dwFlags, (int)ret);
    LEAVE;
    return ret;
}

static HRESULT __stdcall _WaitForVerticalBlank(IDirectDrawImpl *this, DWORD dwFlags, HANDLE hEvent)
{
    HRESULT ret = IDirectDraw_WaitForVerticalBlank(this->real, dwFlags, hEvent);
    dprintf("IDirectDraw::WaitForVerticalBlank(this=%p, dwFlags=%08X, hEvent=%08X) -> %08X\n", this, (int)dwFlags, (int)hEvent, (int)ret);
    return ret;
}

static IDirectDrawImplVtbl Vtbl =
{
    /* IUnknown */
    _QueryInterface,
    _AddRef,
    _Release,
    /* IDirectDrawImpl */
    _Compact,
    _CreateClipper,
    _CreatePalette,
    _CreateSurface,
    _DuplicateSurface,
    _EnumDisplayModes,
    _EnumSurfaces,
    _FlipToGDISurface,
    _GetCaps,
    _GetDisplayMode,
    _GetFourCCCodes,
    _GetGDISurface,
    _GetMonitorFrequency,
    _GetScanLine,
    _GetVerticalBlankStatus,
    _Initialize,
    _RestoreDisplayMode,
    _SetCooperativeLevel,
    _SetDisplayMode,
    _WaitForVerticalBlank
};
