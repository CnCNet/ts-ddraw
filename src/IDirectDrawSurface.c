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

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>

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

    // FIXME: very inefficient
    HDC bitmapDC = CreateCompatibleDC(hDC);
    HBITMAP bitmap = CreateCompatibleBitmap(hDC, this->width, this->height);
    SelectObject(bitmapDC, bitmap);
    SetDIBits(bitmapDC, bitmap, 0, this->height, this->renderSurface, this->bmi, DIB_RGB_COLORS);
    BitBlt(hDC, 0, 0, size.right, size.bottom, bitmapDC, pos.left, pos.top, SRCCOPY);
    DeleteObject(bitmap);
    DeleteDC(bitmapDC);

    return FALSE;
}

DWORD WINAPI render(IDirectDrawSurfaceImpl *this)
{
    DWORD tick_start = 0;
    DWORD tick_end = 0;
    DWORD target_fps = 60;
    DWORD frame_len = 1000.0f / target_fps;
#if _DEBUG
    double frame_time = 0, real_time = timeGetTime();
    int frames = 0;
#endif

    HGLRC hRC = wglCreateContext(this->dd->hDC);
    wglMakeCurrent(this->dd->hDC, hRC);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5, this->width, this->height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, this->renderSurface);

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);

    glEnable(GL_TEXTURE_2D);

    while (this->thread)
    {
        tick_start = timeGetTime();

        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, this->width, this->height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, this->renderSurface);

        glBegin(GL_TRIANGLE_FAN);

        glTexCoord2f(0,0); glVertex2f(-1, 1);
        glTexCoord2f(1,0); glVertex2f( 1, 1);
        glTexCoord2f(1,1); glVertex2f( 1, -1);        
        glTexCoord2f(0,1); glVertex2f(-1, -1);
        glEnd();

        glViewport(-this->dd->winRect.left, this->dd->winRect.bottom - this->height, this->width, this->height);
        SwapBuffers(this->dd->hDC);
        EnumChildWindows(this->dd->hWnd, EnumChildProc, (LPARAM)this);

        tick_end = timeGetTime();

#if _DEBUG
        frame_time += (tick_end - tick_start);
        frames++;
        if (frames >= target_fps)
        {
            printf("Timed FPS: %.2f\n", 1000.0f / (frame_time / frames));
            printf("Real  FPS: %.2f\n", 1000.0f / ((tick_end - real_time) / frames));
            frame_time = frames = 0;
            real_time = tick_end;
        }
#endif

        if (tick_end - tick_start < frame_len)
        {
            Sleep( frame_len - (tick_end - tick_start) );
        }
    }

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hRC);

    return 0;
}

IDirectDrawSurfaceImpl *IDirectDrawSurfaceImpl_construct(IDirectDrawImpl *lpDDImpl, LPDDSURFACEDESC lpDDSurfaceDesc)
{
    IDirectDrawSurfaceImpl *this = calloc(1, sizeof(IDirectDrawSurfaceImpl));
    this->lpVtbl = &Vtbl;
    this->dd = lpDDImpl;

    this->bpp = this->dd->bpp;
    this->dwFlags = lpDDSurfaceDesc->dwFlags;

    if (lpDDSurfaceDesc->dwWidth && lpDDSurfaceDesc->dwHeight)
    {
        this->width = lpDDSurfaceDesc->dwWidth;
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

    this->desc.dwFlags = DDSD_WIDTH|DDSD_HEIGHT|DDSD_PITCH|DDSD_PIXELFORMAT|DDSD_LPSURFACE;
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
    this->bmi->bmiHeader.biHeight = -this->height;
    this->bmi->bmiHeader.biPlanes = 1;
    this->bmi->bmiHeader.biBitCount = this->bpp;
    this->bmi->bmiHeader.biCompression = BI_BITFIELDS;

    ((DWORD *)this->bmi->bmiColors)[0] = this->desc.ddpfPixelFormat.dwRBitMask;
    ((DWORD *)this->bmi->bmiColors)[1] = this->desc.ddpfPixelFormat.dwGBitMask;
    ((DWORD *)this->bmi->bmiColors)[2] = this->desc.ddpfPixelFormat.dwBBitMask;

    if (this->dwCaps & DDSCAPS_PRIMARYSURFACE)
    {
        dprintf("Starting renderer.\n");
        this->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)render, (LPVOID)this, 0, NULL);
        if (SetThreadPriority(this->thread, THREAD_PRIORITY_ABOVE_NORMAL))
        {
            dprintf("Renderer set to higher priority.\n");
        }

        this->hfm = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, this->lPitch * this->height, NULL);
        this->surface = MapViewOfFile(this->hfm, FILE_MAP_WRITE, 0, 0, this->lPitch * this->height);
        this->renderSurface = MapViewOfFile(this->hfm, FILE_MAP_WRITE, 0, 0, this->lPitch * this->height);
    }
    else
    {
        this->surface = calloc(1, this->lPitch * this->height);
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

        if (this->dwCaps & DDSCAPS_PRIMARYSURFACE)
        {
            CloseHandle(this->hfm);
            UnmapViewOfFile(this->surface);
            UnmapViewOfFile(this->renderSurface);
        }
        else
        {
            free(this->surface);
        }

        if (this->overlayBitmap)
        {
            this->overlay = NULL;
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

    if (PROXY)
    {
        ret = IDirectDrawSurface_Blt(this->real, lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    }
    else
    {
        if (dwFlags & DDBLT_COLORFILL)
        {
            for (int x = 0; x < lpDestRect->right - lpDestRect->left; x++) {
                for (int y = 0; y < lpDestRect->bottom - lpDestRect->top; y++) {
                    this->surface[x + lpDestRect->left + (this->width * (y + lpDestRect->top))] = lpDDBltFx->dwFillColor;
                }
            }
        }

        if (lpDestRect && lpSrcRect && lpDestRect->right <= this->width && lpDestRect->bottom <= this->height)
        {
            for (int x = 0; x < lpSrcRect->right - lpSrcRect->left; x++) {
                for (int y = 0; y < lpSrcRect->bottom - lpSrcRect->top; y++) {
                    this->surface[x + lpDestRect->left + (this->width * (y + lpDestRect->top))] = srcImpl->surface[x + lpSrcRect->left + (srcImpl->width * (y + lpSrcRect->top))];
                }
            }
        }

        if (this->dwCaps & DDSCAPS_PRIMARYSURFACE)
        {
            FlushViewOfFile(this->surface, this->lPitch * this->height);
        }
    }

    dprintf("IDirectDrawSurface::Blt(this=%p, lpDestRect=%p, lpDDSrcSurface=%p, lpSrcRect=%p, dwFlags=%d, lpDDBltFx=%p) -> %08X\n", this, lpDestRect, lpDDSrcSurface, lpSrcRect, (int)dwFlags, lpDDBltFx, (int)ret);

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
        memcpy(lpDDSurfaceDesc, &this->desc, sizeof(*lpDDSurfaceDesc));
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
            this->overlayBitmap = CreateDIBSection(this->overlayDC, this->bmi, DIB_RGB_COLORS, (void **)&this->overlay, NULL, 0);
        }

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

    memcpy(lpDDPixelFormat, &this->desc.ddpfPixelFormat, sizeof(*lpDDPixelFormat));

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
        memcpy(lpDDSurfaceDesc, &this->desc, sizeof(*lpDDSurfaceDesc));
        lpDDSurfaceDesc->dwFlags |= DDSD_LPSURFACE;
        lpDDSurfaceDesc->lpSurface = this->surface;
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
        if (this->dwCaps & DDSCAPS_PRIMARYSURFACE)
        {
            FlushViewOfFile(this->surface, this->lPitch * this->height);
        }
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
