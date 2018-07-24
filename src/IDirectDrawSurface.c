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
#include <stdint.h>
#include <stdio.h>

DWORD WINAPI render(IDirectDrawSurfaceImpl *this);

static IDirectDrawSurfaceImplVtbl Vtbl;

/* the TS hack itself */

IDirectDrawSurfaceImpl *IDirectDrawSurfaceImpl_construct(IDirectDrawImpl *lpDDImpl, LPDDSURFACEDESC lpDDSurfaceDesc)
{
    dprintf("--> IDirectDrawSurface::construct()\n");

    IDirectDrawSurfaceImpl *this = calloc(1, sizeof(IDirectDrawSurfaceImpl));
    this->lpVtbl = &Vtbl;
    this->dd = lpDDImpl;

    this->bpp = this->dd->bpp;
    this->dwFlags = lpDDSurfaceDesc->dwFlags;

    if (lpDDSurfaceDesc->dwWidth && lpDDSurfaceDesc->dwHeight)
    {
        this->width = (lpDDSurfaceDesc->dwWidth +1) / 2 * 2;
        this->height = lpDDSurfaceDesc->dwHeight;
    }
    else
    {
        this->width = this->dd->screenWidth;
        this->height = this->dd->screenHeight;
    }

    if (lpDDSurfaceDesc->dwFlags & DDSD_CAPS)
    {
        this->dwCaps = lpDDSurfaceDesc->ddsCaps.dwCaps;

        if (lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
        {
            this->dwCaps |= DDSCAPS_FRONTBUFFER;
        }

        if (!(lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY))
        {
            // we are always in system memory for performance
            this->dwCaps |= DDSCAPS_SYSTEMMEMORY;
        }
    }

    this->lXPitch = this->bpp / 8;
    this->lPitch = this->width * this->lXPitch;

    /* Tiberian Sun sometimes tries to access lines that are past the bottom of the screen */
    int guardLines = 0; // doesn't work (yet)

    this->hDC = CreateCompatibleDC(this->dd->hDC);

    this->desc.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT | DDSD_LPSURFACE;
    this->desc.dwWidth = this->width;
    this->desc.dwHeight = this->height;
    this->desc.lPitch = this->lPitch;
    this->desc.ddpfPixelFormat.dwSize = 32;
    this->desc.ddpfPixelFormat.dwFlags = DDPF_RGB;
    this->desc.ddpfPixelFormat.dwRGBBitCount = this->bpp;
    this->desc.ddpfPixelFormat.dwRBitMask = 0xF800;
    this->desc.ddpfPixelFormat.dwGBitMask = 0x07E0;
    this->desc.ddpfPixelFormat.dwBBitMask = 0x001F;

    this->desc.dwFlags = 0x0000100F;
    this->desc.ddsCaps.dwCaps = this->dwCaps;

    this->bmi = calloc(1, sizeof(BITMAPINFO) + (sizeof(RGBQUAD) * 3));
    this->bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    this->bmi->bmiHeader.biWidth = this->width;
    this->bmi->bmiHeader.biHeight = -(this->height + guardLines);
    this->bmi->bmiHeader.biPlanes = 1;
    this->bmi->bmiHeader.biBitCount = this->bpp;
    this->bmi->bmiHeader.biCompression = BI_BITFIELDS;

    ((DWORD *)this->bmi->bmiColors)[0] = this->desc.ddpfPixelFormat.dwRBitMask;
    ((DWORD *)this->bmi->bmiColors)[1] = this->desc.ddpfPixelFormat.dwGBitMask;
    ((DWORD *)this->bmi->bmiColors)[2] = this->desc.ddpfPixelFormat.dwBBitMask;

    this->bitmap = CreateDIBSection(this->hDC, this->bmi, DIB_RGB_COLORS, (void **)&this->surface, NULL, 0);
    this->defaultBM = SelectObject(this->hDC, this->bitmap);

    this->usingPBO = false;
    this->systemSurface = this->surface;

    InitializeCriticalSection(&this->lock);

    if (this->dwCaps & DDSCAPS_PRIMARYSURFACE)
    {
        this->syncEvent = CreateEvent(NULL, true, false, NULL);
        this->pSurfaceReady = CreateEvent(NULL, true, false, NULL);
        this->pSurfaceDrawn = CreateEvent(NULL, true, false, NULL);

        dprintf("Starting renderer.\n");
        this->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)render, (LPVOID)this, 0, NULL);
        if (SetThreadPriority(this->thread, THREAD_PRIORITY_ABOVE_NORMAL))
        {
            dprintf("Renderer set to higher priority.\n");
        }
        WaitForSingleObject(this->pSurfaceReady, INFINITE);
    }


    dump_ddsurfacedesc(lpDDSurfaceDesc);
    this->ref++;

    dprintf("<-- IDirectDrawSurface::construct() -> %p\n", this);
    return this;
}

static HRESULT __stdcall _QueryInterface(IDirectDrawSurfaceImpl *this, REFIID riid, void **obj)
{
    dprintf("--> IDirectDrawSurface::QueryInterface(this=%p, riid=%08X, obj=%p)\n", this, (unsigned int)riid, obj);

    HRESULT ret = DDERR_UNSUPPORTED;

    if (PROXY)
    {
        ret = IDirectDrawSurface_QueryInterface(this->real, riid, obj);
    }

    dprintf("<-- IDirectDrawSurface::QueryInterface(this=%p, riid=%08X, obj=%p) -> %08X\n", this, (unsigned int)riid, obj, (int)ret);
    return ret;
}

static ULONG __stdcall _AddRef(IDirectDrawSurfaceImpl *this)
{
    dprintf("IDirectDrawSurface::AddRef(this=%p) -> %d\n", this, this->ref);
    return this->ref;
}

static ULONG __stdcall _Release(IDirectDrawSurfaceImpl *this)
{
    dprintf("--> IDirectDrawSurface::Release(this=%p)\n", this);

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

        DeleteCriticalSection(&this->lock);
        DeleteObject(this->bitmap);
        DeleteDC(this->hDC);
        if (this->overlayBitmap)
        {
            this->overlay = NULL;
            DeleteObject(this->overlayBitmap);
        }
        if (this->overlayDC)
        {
            DeleteDC(this->overlayDC);
        }
        if (this->pboCount > 0)
        {
            free(this->pbo);
        }
        free(this);
    }

    dprintf("<-- IDirectDrawSurface::Release(this=%p) -> %08X\n", this, (int)ret);
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
    dprintf(
        "--> IDirectDrawSurface::Blt(this=%p, lpDestRect=%p, lpDDSrcSurface=%p, lpSrcRect=%p, dwFlags=%08X, lpDDBltFx=%p)\n",
        this, lpDestRect, lpDDSrcSurface, lpSrcRect, (int)dwFlags, lpDDBltFx);

    HRESULT ret = DD_OK;
    IDirectDrawSurfaceImpl *srcImpl = (IDirectDrawSurfaceImpl *)lpDDSrcSurface;

    if (PROXY)
    {
        ret = IDirectDrawSurface_Blt(this->real, lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    }
    else
    {
        static BOOL eventSet = false;
        if (this->dwCaps & DDSCAPS_PRIMARYSURFACE && !eventSet)
        {
            SetEvent(this->pSurfaceDrawn);
            eventSet = true;
        }

        RECT src = { 0, 0, srcImpl ? srcImpl->width : 0, srcImpl ? srcImpl->height : 0};
        RECT dst = { 0, 0, this->width, this->height };

        if (lpSrcRect)
        {
            memcpy(&src, lpSrcRect, sizeof(src));

            if (src.right > srcImpl->width)
                src.right = srcImpl->width;

            if (src.bottom > srcImpl->height)
                src.bottom = srcImpl->height;
        }

        if (lpDestRect)
        {
            memcpy(&dst, lpDestRect, sizeof(dst));

            if (dst.right > this->width)
                dst.right = this->width;

            if (dst.bottom > this->height)
                dst.bottom = this->height;
        }

        if (dwFlags & DDBLT_COLORFILL)
        {
            EnterCriticalSection(&this->lock);

            int dst_w = dst.right - dst.left;
            int dst_h = dst.bottom - dst.top;

            for (int y = 0; y < dst_h; y++)
            {
                int ydst = this->width * (y + dst.top);

                for (int x = 0; x < dst_w; x++)
                {
                    this->surface[x + dst.left + ydst] = lpDDBltFx->dwFillColor;
                }
            }

            LeaveCriticalSection(&this->lock);
        }

        if (lpDDSrcSurface)
        {
            EnterCriticalSection(&this->lock);

            int dst_w = dst.right - dst.left;
            int dst_h = dst.bottom - dst.top;

            int src_w = src.right - src.left;
            int src_h = src.bottom - src.top;

            int dst_byte_width = dst_w * this->lXPitch;

            if (dst_w == src_w && dst_h == src_h)
            {
                if (this->usingPBO)
                {
                    // Sometimes radar surface will have an odd lPitch, BitBlt won't work in those cases
                    uint8_t *dest_base = (uint8_t*)this->surface + (dst.left * this->lXPitch) + (this->lPitch * dst.top);
                    uint8_t *src_base = (uint8_t*)srcImpl->surface + (src.left * this->lXPitch) + (srcImpl->lPitch * src.top);

                    while (dst_h-- > 0)
                    {
                        memcpy((void *)dest_base, (void *)src_base, dst_byte_width);

                        dest_base += this->lPitch;
                        src_base += srcImpl->lPitch;
                    }
                }
                else
                    BitBlt(this->hDC, dst.left, dst.top, dst_w, dst_h, srcImpl->hDC, src.left, src.top, SRCCOPY);
            }
            else
            {
                StretchBlt(this->hDC, dst.left, dst.top, dst_w, dst_h, srcImpl->hDC, src.left, src.top, src_w, src_h, SRCCOPY);
            }
            LeaveCriticalSection(&this->lock);
        }
    }

    if (dwFlags)
        dprintf(" dwFlags:\n");

    if (dwFlags & DDBLT_COLORFILL)
    {
        dprintf("  DDBLT_COLORFILL\n");
    }

    if (dwFlags & DDBLT_DDFX)
    {
        dprintf("  DDBLT_DDFX\n");
    }

    if (dwFlags & DDBLT_DDROPS)
    {
        dprintf("  DDBLT_DDDROPS\n");
    }

    if (dwFlags & DDBLT_DEPTHFILL)
    {
        dprintf("  DDBLT_DEPTHFILL\n");
    }

    if (dwFlags & DDBLT_KEYDESTOVERRIDE)
    {
        dprintf("  DDBLT_KEYDESTOVERRIDE\n");
    }

    if (dwFlags & DDBLT_KEYSRCOVERRIDE)
    {
        dprintf("  DDBLT_KEYSRCOVERRIDE\n");
    }

    if (dwFlags & DDBLT_ROP)
    {
        dprintf("  DDBLT_ROP\n");
    }

    if (dwFlags & DDBLT_ROTATIONANGLE)
    {
        dprintf("  DDBLT_ROTATIONANGLE\n");
    }

    if (VERBOSE)
    {
        if (lpDestRect)
        {
            dprintf(" dest: l: %d t: %d r: %d b: %d\n", (int)lpDestRect->left, (int)lpDestRect->top, (int)lpDestRect->right, (int)lpDestRect->bottom);
            dprintf(" dest: w: %d h: %d bpp: %d\n", (int)this->width, (int)this->height, (int)this->bpp);
        }

        if (lpSrcRect)
        {
            dprintf(" src: l: %d t: %d r: %d b: %d\n", (int)lpSrcRect->left, (int)lpSrcRect->top, (int)lpSrcRect->right, (int)lpSrcRect->bottom);
            dprintf(" src: w: %d h: %d bpp: %d\n", (int)srcImpl->width, (int)srcImpl->height, (int)srcImpl->bpp);
        }
    }
    if (lpDDBltFx)
    {
        dump_ddbltfx(lpDDBltFx);
    }

    dprintf(
        "<-- IDirectDrawSurface::Blt(this=%p, lpDestRect=%p, lpDDSrcSurface=%p, lpSrcRect=%p, dwFlags=%08X, lpDDBltFx=%p) -> %08X\n",
        this, lpDestRect, lpDDSrcSurface, lpSrcRect, (int)dwFlags, lpDDBltFx, (int)ret);

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
    dprintf("--> IDirectDrawSurface::GetSurfaceDesc(this=%p, lpDDSurfaceDesc=%p)\n", this, lpDDSurfaceDesc);
    HRESULT ret = DD_OK;

    if (PROXY)
    {
        ret = IDirectDrawSurface_GetSurfaceDesc(this->real, lpDDSurfaceDesc);
    }
    else
    {
        lpDDSurfaceDesc->dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT | DDSD_LPSURFACE;
        lpDDSurfaceDesc->dwWidth = this->width;
        lpDDSurfaceDesc->dwHeight = this->height;
        lpDDSurfaceDesc->lPitch = this->lPitch;
        lpDDSurfaceDesc->ddpfPixelFormat.dwSize = 32;
        lpDDSurfaceDesc->ddpfPixelFormat.dwFlags = DDPF_RGB;
        lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = this->bpp;
        lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = 0xF800;
        lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = 0x07E0;
        lpDDSurfaceDesc->ddpfPixelFormat.dwBBitMask = 0x001F;

        lpDDSurfaceDesc->dwFlags = 0x0000100F;
        lpDDSurfaceDesc->ddsCaps.dwCaps = this->dwCaps;
    }

    dump_ddsurfacedesc(lpDDSurfaceDesc);
    dprintf("<-- IDirectDrawSurface::GetSurfaceDesc(this=%p, lpDDSurfaceDesc=%p) -> %08X\n", this, lpDDSurfaceDesc, (int)ret);
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
    else
    {
        if ((this->dwCaps & DDSCAPS_PRIMARYSURFACE) && !(this->dwCaps & DDSCAPS_BACKBUFFER)
            && this->thread)
        {
            EnterCriticalSection(&this->lock);
            LeaveCriticalSection(&this->lock);
            SetEvent(this->syncEvent);
        }
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
    dprintf("--> IDirectDrawSurface::GetDC(this=%p, lphDC=%p)\n", this, lphDC);

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
            this->overlayBitmap = CreateDIBSection(this->overlayDC, this->bmi, DIB_RGB_COLORS, (void **)&this->overlay, NULL, 0);
        }

        EnterCriticalSection(&this->lock);
        *lphDC = this->overlayDC;
        SelectObject(this->overlayDC, this->overlayBitmap);
    }

    dprintf("<-- IDirectDrawSurface::GetDC(this=%p, lphDC=%p) -> %08X\n", this, lphDC, (int)ret);
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
    dprintf("--> IDirectDrawSurface::GetPixelFormat(this=%p, lpDDPixelFormat=%p)\n", this, lpDDPixelFormat);

    lpDDPixelFormat->dwFlags = DDPF_RGB;
    lpDDPixelFormat->dwRGBBitCount = this->bpp;
    lpDDPixelFormat->dwRBitMask = 0xF800;
    lpDDPixelFormat->dwGBitMask = 0x07E0;
    lpDDPixelFormat->dwBBitMask = 0x001F;

    dprintf("<-- IDirectDrawSurface::GetPixelFormat(this=%p, lpDDPixelFormat=%p)\n", this, lpDDPixelFormat);
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
    dprintf(
        "--> IDirectDrawSurface::Lock(this=%p, lpDestRect=%p, lpDDSurfaceDesc=%p, dwFlags=%08X, hEvent=%p)\n",
        this, lpDestRect, lpDDSurfaceDesc, (int)dwFlags, hEvent);

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
        lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = 0xF800;
        lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = 0x07E0;
        lpDDSurfaceDesc->ddpfPixelFormat.dwBBitMask = 0x001F;
        lpDDSurfaceDesc->dwFlags = 0x0000100F;
        lpDDSurfaceDesc->ddsCaps.dwCaps = 0x10004000;
        lpDDSurfaceDesc->ddsCaps.dwCaps = this->dwCaps;

        EnterCriticalSection(&this->lock);
    }

    dump_ddsurfacedesc(lpDDSurfaceDesc);
    dprintf(
        "<-- IDirectDrawSurface::Lock(this=%p, lpDestRect=%p, lpDDSurfaceDesc=%p, dwFlags=%08X, hEvent=%p) -> %08X\n",
        this, lpDestRect, lpDDSurfaceDesc, (int)dwFlags, hEvent, (int)ret);

    LEAVE;
    return ret;
}

HRESULT __stdcall _ReleaseDC(IDirectDrawSurfaceImpl *this, HDC hDC)
{
    ENTER;
    dprintf("--> IDirectDrawSurface::ReleaseDC(this=%p, hDC=%08X)\n", this, (int)hDC);

    HRESULT ret = DD_OK;

    if (PROXY)
    {
        ret = IDirectDrawSurface_ReleaseDC(this->real, hDC);
    }
    else
    {
        // FIXME: using black as magic transparency color
        for (int y = 0; y < this->height; y++)
        {
            int ydst = this->width * y;

            for (int x = 0; x < this->width; x++)
            {
                unsigned short px = this->overlay[x + ydst];
                if (px)
                {
                    this->surface[x + ydst] = px;
                }
            }
        }

        RECT rc = { 0, 0, this->width, this->height };
        FillRect(this->overlayDC, &rc, CreateSolidBrush(RGB(0,0,0)));
        LeaveCriticalSection(&this->lock);
    }

    dprintf("<-- IDirectDrawSurface::ReleaseDC(this=%p, hDC=%08X) -> %08X\n", this, (int)hDC, (int)ret);
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
    dprintf("--> IDirectDrawSurface::SetClipper(this=%p, lpDDClipper=%p)\n", this, lpDDClipper);

    HRESULT ret = DD_OK;
    IDirectDrawClipperImpl *impl = (IDirectDrawClipperImpl *)lpDDClipper;

    if (PROXY)
    {
        ret = IDirectDrawSurface_SetClipper(this->real, impl->real);
    }

    dprintf("<-- IDirectDrawSurface::SetClipper(this=%p, lpDDClipper=%p) -> %08X\n", this, lpDDClipper, (int)ret);
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
    dprintf("--> IDirectDrawSurface::Unlock(this=%p, lpRect=%p)\n", this, lpRect);

    HRESULT ret = DD_OK;

    if (PROXY)
    {
        ret = IDirectDrawSurface_Unlock(this->real, lpRect);
    }
    else
    {
        LeaveCriticalSection(&this->lock);

    }

    dprintf("<-- IDirectDrawSurface::Unlock(this=%p, lpRect=%p) -> %08X\n", this, lpRect, (int)ret);
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
