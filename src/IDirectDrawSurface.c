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
#include "IDirectDrawClipper.h"
#include "IDirectDrawSurface.h"

static IDirectDrawSurfaceImplVtbl Vtbl;

/* the TS hack itself */
BOOL CALLBACK EnumChildProc(HWND hWnd, LPARAM lParam)
{
    IDirectDrawSurfaceImpl *this = (IDirectDrawSurfaceImpl *)lParam;

    HDC hDC = GetWindowDC(hWnd);

    RECT size;
    GetClientRect(hWnd, &size);

    RECT pos;
    GetWindowRect(hWnd, &pos);

    BitBlt(hDC, 0, 0, size.right, size.bottom, this->hDC, pos.left, pos.top, SRCCOPY);
    return FALSE;
}

DWORD WINAPI render(IDirectDrawSurfaceImpl *this)
{
    DWORD tick_start = 0;
    DWORD tick_end = 0;
    DWORD frame_len = 1000.0f / 60;

    while (this->thread)
    {
        tick_start = timeGetTime();

        EnterCriticalSection(&this->lock);

        BitBlt(this->dd->hDC, 0, 0, this->width, this->height, this->hDC, 0, 0, SRCCOPY);
        EnumChildWindows(this->dd->hWnd, EnumChildProc, (LPARAM)this);

        LeaveCriticalSection(&this->lock);

        tick_end = timeGetTime();

        if (tick_end - tick_start < frame_len)
        {
            Sleep( frame_len - (tick_end - tick_start) );
        }
    }

    return 0;
}

IDirectDrawSurfaceImpl *IDirectDrawSurfaceImpl_construct(IDirectDrawImpl *lpDDImpl, LPDDSURFACEDESC lpDDSurfaceDesc)
{
    IDirectDrawSurfaceImpl *this = calloc(1, sizeof(IDirectDrawSurfaceImpl));
    this->lpVtbl = &Vtbl;
    this->dd = lpDDImpl;

    this->bpp = this->dd->bpp;
    this->dwFlags = lpDDSurfaceDesc->dwFlags;

    if (lpDDSurfaceDesc->dwFlags & DDSD_CAPS)
    {
        this->dwCaps = lpDDSurfaceDesc->ddsCaps.dwCaps;

        if (lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
        {
            this->width = this->dd->width;
            this->height = this->dd->height;
            this->dwCaps |= DDSCAPS_FRONTBUFFER;
        }

        if (!(lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY))
        {
            // we are always in system memory for performance
            this->dwCaps |= DDSCAPS_SYSTEMMEMORY;
        }
    }

    if (!(lpDDSurfaceDesc->dwFlags & DDSD_CAPS) || !(lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) )
    {
        this->width = lpDDSurfaceDesc->dwWidth;
        this->height = lpDDSurfaceDesc->dwHeight;
    }

    if (this->width && this->height)
    {
        this->lXPitch = this->bpp / 8;
        this->lPitch = this->width * this->lXPitch;

        this->hDC = CreateCompatibleDC(this->dd->hDC);

        this->overlay = calloc(1, this->lPitch * this->height * this->lXPitch);

        this->bmi = calloc(1, sizeof(BITMAPINFO) + (this->lPitch * this->height * this->lXPitch));
        this->bmi->bmiHeader.biSize = sizeof(BITMAPINFO);
        this->bmi->bmiHeader.biWidth = this->width;
        this->bmi->bmiHeader.biHeight = -this->height;
        this->bmi->bmiHeader.biPlanes = 1;
        this->bmi->bmiHeader.biBitCount = this->bpp;
        this->bmi->bmiHeader.biCompression = BI_RGB;

        this->bitmap = CreateDIBSection(this->hDC, this->bmi, DIB_RGB_COLORS, (void **)&this->surface, NULL, 0);
        SelectObject(this->hDC, this->bitmap);
    }

    InitializeCriticalSection(&this->lock);

    if (this->dwCaps & DDSCAPS_PRIMARYSURFACE)
    {
        dprintf("Starting renderer.\n");
        this->frame = CreateEvent(NULL, TRUE, FALSE, NULL);
        this->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)render, (LPVOID)this, 0, NULL);
        if (SetThreadPriority(this->thread, THREAD_PRIORITY_ABOVE_NORMAL))
        {
            dprintf("Renderer set to higher priority.\n");
        }
    }

    dprintf("IDirectDrawSurface::construct() -> %p\n", this);
    dump_ddsurfacedesc(lpDDSurfaceDesc);
    this->ref++;
    return this;
}

static HRESULT __stdcall _QueryInterface(IDirectDrawSurfaceImpl *this, REFIID riid, void **obj)
{
    HRESULT ret = DDERR_UNSUPPORTED;

    if (PROXY)
    {
        ret = IDirectDrawSurface_QueryInterface(this->real, riid, obj);
    }

    dprintf("IDirectDrawSurface::QueryInterface(this=%p, riid=%08X, obj=%p) -> %08X\n", this, (unsigned int)riid, obj, (int)ret);
    return ret;
}

static ULONG __stdcall _AddRef(IDirectDrawSurfaceImpl *this)
{
    dprintf("IDirectDrawSurface::AddRef(this=%p)\n", this);
    return this->ref;
}

static ULONG __stdcall _Release(IDirectDrawSurfaceImpl *this)
{
    ULONG ret = --this->ref;

    if (PROXY)
    {
        ret = IDirectDrawSurface_Release(this->real);
    }

    if (this->ref == 0)
    {
        if (this->thread)
        {
            HANDLE thread = this->thread;
            this->thread = NULL;
            dprintf("Waiting for renderer to stop.\n");
            WaitForSingleObject(thread, INFINITE);
            dprintf("Renderer stopped.\n");
        }

        free(this->overlay);
        DeleteCriticalSection(&this->lock);
        DeleteObject(this->bitmap);
        DeleteDC(this->hDC);
        if (this->overlayBitmap)
        {
            DeleteObject(this->overlayBitmap);
        }
        if (this->overlayDC)
        {
            DeleteDC(this->overlayDC);
        }
        free(this);
    }

    dprintf("IDirectDrawSurface::Release(this=%p) -> %08X\n", this, (int)ret);
    return ret;
}

HRESULT __stdcall _AddAttachedSurface(IDirectDrawSurfaceImpl *this, LPDIRECTDRAWSURFACE lpDDSurface)
{
    dprintf("IDirectDrawSurface::AddAttachedSurface(this=%p, lpDDSurface=%p)\n", this, lpDDSurface);
    return DD_OK;
}

HRESULT __stdcall _AddOverlayDirtyRect(IDirectDrawSurfaceImpl *this, LPRECT a)
{
    dprintf("IDirectDrawSurface::AddOverlayDirtyRect(this=%p, ...)\n", this);
    return DD_OK;
}

static HRESULT __stdcall _Blt(IDirectDrawSurfaceImpl *this, LPRECT lpDestRect, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx)
{
    ENTER;
    HRESULT ret = DD_OK;
    IDirectDrawSurfaceImpl *srcImpl = (IDirectDrawSurfaceImpl *)lpDDSrcSurface;

#if 0
    // consistently fail this test to get consistent debug output for real and emul
    if (lpDDBltFx)
        return ret;
#endif

    if (PROXY)
    {
        ret = IDirectDrawSurface_Blt(this->real, lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    }
    else
    {
#if 0
        if (this->dwCaps & DDSCAPS_PRIMARYSURFACE)
        {
            EnterCriticalSection(&this->lock);
        }
#endif

#if 0
        if (lpDestRect && lpSrcRect)
        {
            for (int x = 0; x < lpDestRect->right - lpDestRect->left; x++) {
                for (int y = 0; y < lpDestRect->bottom - lpDestRect->top; y++) {
                    this->surface[x + lpDestRect->left + (this->width * (y + lpDestRect->top))] = srcImpl->surface[x + lpSrcRect->left + (srcImpl->width * (y + lpSrcRect->top))];
                }
            }
        }
#endif

#if 0
        if (this->dwCaps & DDSCAPS_PRIMARYSURFACE)
        {
            LeaveCriticalSection(&this->lock);
            SetEvent(this->frame);
        }
#endif
    }

    dprintf("IDirectDrawSurface::Blt(this=%p, lpDestRect=%p, lpDDSrcSurface=%p, lpSrcRect=%p, dwFlags=%d, lpDDBltFx=%p) -> %08X\n", this, lpDestRect, lpDDSrcSurface, lpSrcRect, (int)dwFlags, lpDDBltFx, (int)ret);

    if (lpDestRect)
    {
        dprintf(" dest: l: %d t: %d r: %d b: %d\n", (int)lpDestRect->left, (int)lpDestRect->top, (int)lpDestRect->right, (int)lpDestRect->bottom);
    }

    if (lpSrcRect)
    {
        dprintf("  src: l: %d t: %d r: %d b: %d\n", (int)lpSrcRect->left, (int)lpSrcRect->top, (int)lpSrcRect->right, (int)lpSrcRect->bottom);
    }

    if (lpDDBltFx)
    {
        dump_ddbltfx(lpDDBltFx);
    }

    LEAVE;
    return ret;
}

HRESULT __stdcall _BltBatch(IDirectDrawSurfaceImpl *this, LPDDBLTBATCH a, DWORD b, DWORD c)
{
    dprintf("IDirectDrawSurface::BltBatch(this=%p, ...)\n", this);
    return DD_OK;
}

HRESULT __stdcall _BltFast(IDirectDrawSurfaceImpl *this, DWORD a, DWORD b, LPDIRECTDRAWSURFACE c, LPRECT d, DWORD e)
{
    dprintf("IDirectDrawSurface::BltFast(this=%p, ...)\n", this);
    return DD_OK;
}

HRESULT __stdcall _DeleteAttachedSurface(IDirectDrawSurfaceImpl *this, DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSurface)
{
    dprintf("IDirectDrawSurface::DeleteAttachedSurface(this=%p, dwFlags=%d, lpDDSurface=%p)\n", this, (int)dwFlags, lpDDSurface);
    return DD_OK;
}

static HRESULT __stdcall _GetSurfaceDesc(IDirectDrawSurfaceImpl *this, LPDDSURFACEDESC lpDDSurfaceDesc)
{
    ENTER;
    HRESULT ret = DD_OK;

    if (PROXY)
    {
        ret = IDirectDrawSurface_GetSurfaceDesc(this->real, lpDDSurfaceDesc);
    }
    else
    {
        lpDDSurfaceDesc->dwFlags = DDSD_WIDTH|DDSD_HEIGHT|DDSD_PITCH|DDSD_PIXELFORMAT|DDSD_LPSURFACE;
        lpDDSurfaceDesc->dwWidth = this->width;
        lpDDSurfaceDesc->dwHeight = this->height;
        lpDDSurfaceDesc->lPitch = this->lPitch;
        lpDDSurfaceDesc->ddpfPixelFormat.dwSize = 32;
        lpDDSurfaceDesc->ddpfPixelFormat.dwFlags = DDPF_RGB;
        lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = this->bpp;
        lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = 0x7C00;
        lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = 0x03E0;
        lpDDSurfaceDesc->ddpfPixelFormat.dwBBitMask = 0x001F;


        lpDDSurfaceDesc->dwFlags = 0x0000100F;
        lpDDSurfaceDesc->ddsCaps.dwCaps = this->dwCaps;
    }

    dprintf("IDirectDrawSurface::GetSurfaceDesc(this=%p, lpDDSurfaceDesc=%p) -> %08X\n", this, lpDDSurfaceDesc, (int)ret);
    dump_ddsurfacedesc(lpDDSurfaceDesc);
    LEAVE;
    return ret;
}

HRESULT __stdcall _EnumAttachedSurfaces(IDirectDrawSurfaceImpl *this, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback)
{
    dprintf("IDirectDrawSurface::EnumAttachedSurfaces(this=%p, lpContext=%p, lpEnumSurfacesCallback=%p)\n", this, lpContext, lpEnumSurfacesCallback);
    return DD_OK;
}

HRESULT __stdcall _EnumOverlayZOrders(IDirectDrawSurfaceImpl *this, DWORD a, LPVOID b, LPDDENUMSURFACESCALLBACK c)
{
    dprintf("IDirectDrawSurface::EnumOverlayZOrders(this=%p, ...)\n", this);
    return DD_OK;
}

HRESULT __stdcall _Flip(IDirectDrawSurfaceImpl *this, LPDIRECTDRAWSURFACE a, DWORD b)
{
    dprintf("IDirectDrawSurface::Flip(this=%p, ...)\n", this);
    return DD_OK;
}

HRESULT __stdcall _GetAttachedSurface(IDirectDrawSurfaceImpl *this, LPDDSCAPS a, LPDIRECTDRAWSURFACE FAR *b)
{
    dprintf("IDirectDrawSurface::GetAttachedSurface(this=%p, ...)\n", this);
    return DD_OK;
}

static HRESULT __stdcall _GetBltStatus(IDirectDrawSurfaceImpl *this, DWORD dwFlags)
{
    ENTER;
    HRESULT ret = DD_OK;

    if (PROXY)
    {
        ret = IDirectDrawSurface_GetBltStatus(this->real, dwFlags);
    }

    if (VERBOSE)
    {
        dprintf("IDirectDrawSurface::GetBltStatus(this=%p, dwFlags=%08X) -> %08X\n", this, (int)dwFlags, (int)ret);
    }
    LEAVE;
    return ret;
}

HRESULT __stdcall _GetCaps(IDirectDrawSurfaceImpl *this, LPDDSCAPS lpDDSCaps)
{
    dprintf("IDirectDrawSurface::GetCaps(this=%p, lpDDSCaps=%p)\n", this, lpDDSCaps);
    return DD_OK;
}

HRESULT __stdcall _GetClipper(IDirectDrawSurfaceImpl *this, LPDIRECTDRAWCLIPPER FAR *a)
{
    dprintf("IDirectDrawSurface::GetClipper(this=%p, ...)\n", this);
    return DD_OK;
}

HRESULT __stdcall _GetColorKey(IDirectDrawSurfaceImpl *this, DWORD a, LPDDCOLORKEY b)
{
    dprintf("IDirectDrawSurface::GetColorKey(this=%p, ...)\n", this);
    return DD_OK;
}

HRESULT __stdcall _GetDC(IDirectDrawSurfaceImpl *this, HDC FAR *lphDC)
{
    HRESULT ret = DD_OK;

    if (PROXY)
    {
        ret = IDirectDrawSurface_GetDC(this->real, lphDC);
    }
    else
    {
        if (!this->overlayDC)
        {
            this->overlayDC = CreateCompatibleDC(this->dd->hDC);
            this->overlayBitmap = CreateCompatibleBitmap(this->dd->hDC, this->width, this->height);
        }

        EnterCriticalSection(&this->lock);
        *lphDC = this->overlayDC;
        SelectObject(this->overlayDC, this->overlayBitmap);
    }

    dprintf("IDirectDrawSurface::GetDC(this=%p, lphDC=%p) -> %08X\n", this, lphDC, (int)ret);
    return ret;
}

HRESULT __stdcall _GetFlipStatus(IDirectDrawSurfaceImpl *this, DWORD a)
{
    dprintf("IDirectDrawSurface::GetFlipStatus(this=%p, ...)\n", this);
    return DD_OK;
}

HRESULT __stdcall _GetOverlayPosition(IDirectDrawSurfaceImpl *this, LPLONG a, LPLONG b)
{
    dprintf("IDirectDrawSurface::GetOverlayPosition(this=%p, ...)\n", this);
    return DD_OK;
}

HRESULT __stdcall _GetPalette(IDirectDrawSurfaceImpl *this, LPDIRECTDRAWPALETTE FAR *lplpDDPalette)
{
    dprintf("IDirectDrawSurface::GetPalette(this=%p, lplpDDPalette=%p)\n", this, lplpDDPalette);
    return DD_OK;
}

HRESULT __stdcall _GetPixelFormat(IDirectDrawSurfaceImpl *this, LPDDPIXELFORMAT lpDDPixelFormat)
{
    dprintf("IDirectDrawSurface::GetPixelFormat(this=%p, lpDDPixelFormat=%p)\n", this, lpDDPixelFormat);

    lpDDPixelFormat->dwFlags = DDPF_RGB;
    lpDDPixelFormat->dwRGBBitCount = this->bpp;
    lpDDPixelFormat->dwRBitMask = 0x7C00;
    lpDDPixelFormat->dwGBitMask = 0x03E0;
    lpDDPixelFormat->dwBBitMask = 0x001F;

    return DD_OK;
}

HRESULT __stdcall _Initialize(IDirectDrawSurfaceImpl *this, LPDIRECTDRAW a, LPDDSURFACEDESC b)
{
    dprintf("IDirectDrawSurface::Initialize(this=%p, ...)\n", this);
    return DD_OK;
}

static HRESULT __stdcall _IsLost(IDirectDrawSurfaceImpl *this)
{
    ENTER;
    HRESULT ret = DD_OK;

    if (PROXY)
    {
        ret = IDirectDrawSurface_IsLost(this->real);
    }

    if (VERBOSE)
    {
        dprintf("IDirectDrawSurface::IsLost(this=%p) -> %08X\n", this, (int)ret);
    }

    LEAVE;
    return ret;
}

static HRESULT __stdcall _Lock(IDirectDrawSurfaceImpl *this, LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent)
{
    ENTER;
    HRESULT ret = DD_OK;

    if (PROXY)
    {
        ret = IDirectDrawSurface_Lock(this->real, lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
    }
    else
    {
        lpDDSurfaceDesc->dwFlags |= DDSD_WIDTH|DDSD_HEIGHT|DDSD_PITCH|DDSD_PIXELFORMAT|DDSD_LPSURFACE;
        lpDDSurfaceDesc->dwWidth = this->width;
        lpDDSurfaceDesc->dwHeight = this->height;
        lpDDSurfaceDesc->lPitch = this->lPitch;
        lpDDSurfaceDesc->lpSurface = this->surface;
        lpDDSurfaceDesc->ddpfPixelFormat.dwSize = 32;
        lpDDSurfaceDesc->ddpfPixelFormat.dwFlags = DDPF_RGB;
        lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = this->bpp;
        lpDDSurfaceDesc->dwFlags = 0x0000100F;
        lpDDSurfaceDesc->ddsCaps.dwCaps = 0x10004000;
        lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = 0x7C00;
        lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = 0x03E0;
        lpDDSurfaceDesc->ddpfPixelFormat.dwBBitMask = 0x001F;
        lpDDSurfaceDesc->ddsCaps.dwCaps = this->dwCaps;

        EnterCriticalSection(&this->lock);
    }

    dprintf("IDirectDrawSurface::Lock(this=%p, lpDestRect=%p, lpDDSurfaceDesc=%p, dwFlags=%08X, hEvent=%p) -> %08X\n", this, lpDestRect, lpDDSurfaceDesc, (int)dwFlags, hEvent, (int)ret);
    dump_ddsurfacedesc(lpDDSurfaceDesc);
    LEAVE;
    return ret;
}

HRESULT __stdcall _ReleaseDC(IDirectDrawSurfaceImpl *this, HDC hDC)
{
    ENTER;
    HRESULT ret = DD_OK;

    if (PROXY)
    {
        ret = IDirectDrawSurface_ReleaseDC(this->real, hDC);
    }
    else
    {
        GetDIBits(this->overlayDC, this->overlayBitmap, 0, this->height, this->overlay, this->bmi, DIB_RGB_COLORS);

        // FIXME: using black as magic transparency color
        for (int x = 0; x < this->width; x++) {
            for (int y = 0; y < this->height; y++) {
                unsigned short px = this->overlay[x + (this->width * y)];
                if (px)
                {
                    this->surface[x + (this->width * y)] = px;
                }
            }
        }

        RECT rc = { 0, 0, this->width, this->height };
        FillRect(this->overlayDC, &rc, CreateSolidBrush(RGB(0,0,0)));
        LeaveCriticalSection(&this->lock);
    }

    dprintf("IDirectDrawSurface::ReleaseDC(this=%p, hDC=%08X) -> %08X\n", this, (int)hDC, (int)ret);
    LEAVE;
    return ret;
}

HRESULT __stdcall _Restore(IDirectDrawSurfaceImpl *this)
{
    ENTER;
    dprintf("IDirectDrawSurface::Restore(this=%p)\n", this);
    LEAVE;
    return DD_OK;
}

static HRESULT __stdcall _SetClipper(IDirectDrawSurfaceImpl *this, LPDIRECTDRAWCLIPPER lpDDClipper)
{
    ENTER;
    HRESULT ret = DD_OK;
    IDirectDrawClipperImpl *impl = (IDirectDrawClipperImpl *)lpDDClipper;

    if (PROXY)
    {
        ret = IDirectDrawSurface_SetClipper(this->real, impl->real);
    }

    dprintf("IDirectDrawSurface::SetClipper(this=%p, lpDDClipper=%p) -> %08X\n", this, lpDDClipper, (int)ret);
    LEAVE;
    return ret;
}

HRESULT __stdcall _SetColorKey(IDirectDrawSurfaceImpl *this, DWORD a, LPDDCOLORKEY b)
{
    dprintf("IDirectDrawSurface::SetColorKey(this=%p, ...)\n", this);
    return DD_OK;
}

HRESULT __stdcall _SetOverlayPosition(IDirectDrawSurfaceImpl *this, LONG a, LONG b)
{
    dprintf("IDirectDrawSurface::SetOverlayPosition(this=%p, ...)\n", this);
    return DD_OK;
}

HRESULT __stdcall _SetPalette(IDirectDrawSurfaceImpl *this, LPDIRECTDRAWPALETTE lpDDPalette)
{
    dprintf("IDirectDrawSurface::SetPalette(this=%p, lpDDPalette=%p)\n", this, lpDDPalette);
    return DD_OK;
}

static HRESULT __stdcall _Unlock(IDirectDrawSurfaceImpl *this, LPVOID lpRect)
{
    ENTER;
    HRESULT ret = DD_OK;

    if (PROXY)
    {
        ret = IDirectDrawSurface_Unlock(this->real, lpRect);
    }
    else
    {
        LeaveCriticalSection(&this->lock);
    }

    dprintf("IDirectDrawSurface::Unlock(this=%p, lpRect=%p) -> %08X\n", this, lpRect, (int)ret);
    LEAVE;
    return ret;
}

HRESULT __stdcall _UpdateOverlay(IDirectDrawSurfaceImpl *this, LPRECT a, LPDIRECTDRAWSURFACE b, LPRECT c, DWORD d, LPDDOVERLAYFX e)
{
    dprintf("IDirectDrawSurface::UpdateOverlay(this=%p, ...)\n", this);
    return DD_OK;
}

HRESULT __stdcall _UpdateOverlayDisplay(IDirectDrawSurfaceImpl *this, DWORD a)
{
    dprintf("IDirectDrawSurface::UpdateOverlayDisplay(this=%p, ...)\n", this);
    return DD_OK;
}

HRESULT __stdcall _UpdateOverlayZOrder(IDirectDrawSurfaceImpl *this, DWORD a, LPDIRECTDRAWSURFACE b)
{
    dprintf("IDirectDrawSurface::UpdateOverlayZOrder(this=%p, ...)\n", this);
    return DD_OK;
}

static struct IDirectDrawSurfaceImplVtbl Vtbl =
{
    /* IUnknown */
    _QueryInterface,
    _AddRef,
    _Release,
    /* IDirectDrawSurface */
    _AddAttachedSurface,
    _AddOverlayDirtyRect,
    _Blt,
    _BltBatch,
    _BltFast,
    _DeleteAttachedSurface,
    _EnumAttachedSurfaces,
    _EnumOverlayZOrders,
    _Flip,
    _GetAttachedSurface,
    _GetBltStatus,
    _GetCaps,
    _GetClipper,
    _GetColorKey,
    _GetDC,
    _GetFlipStatus,
    _GetOverlayPosition,
    _GetPalette,
    _GetPixelFormat,
    _GetSurfaceDesc,
    _Initialize,
    _IsLost,
    _Lock,
    _ReleaseDC,
    _Restore,
    _SetClipper,
    _SetColorKey,
    _SetOverlayPosition,
    _SetPalette,
    _Unlock,
    _UpdateOverlay,
    _UpdateOverlayDisplay,
    _UpdateOverlayZOrder
};
