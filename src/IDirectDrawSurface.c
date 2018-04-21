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
#include "scale_pattern.h"
#include <stdint.h>
#include <stdio.h>

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

	this->dd->render.invalidate = TRUE;

	BitBlt(hDC, 0, 0, size.right, size.bottom, this->hDC, pos.left, pos.top, SRCCOPY);

    ReleaseDC(hWnd, hDC);

    return FALSE;
}

DWORD WINAPI render(IDirectDrawSurfaceImpl *this)
{
    Sleep(500);

    DWORD tick_start = 0;
    DWORD tick_end = 0;
    TargetFrameLen = 1000 / TargetFPS;
    DWORD startTargetFPS = TargetFPS;
    DWORD tick_len = 0;
    DWORD showFPS = 0;

    DWORD avg_len = TargetFrameLen;
    DWORD recent_frames[FRAME_SAMPLES] = { 16 };
    DWORD render_time = 0;
    int dropFrames = 0;
    DWORD totalDroppedFrames = 0;
    int rIndex = 0;

    RECT textRect = (RECT){0,0,0,0};
    char fpsString[256] = {0};
    DWORD avg_fps = 0;

#ifdef _DEBUG
    double frame_time = 0, real_time = timeGetTime();
    int frames = 0;
#endif

    while (this->thread)
    {
        tick_start = timeGetTime();


        if (dropFrames > 0)
            dropFrames--;
        else
        {
            EnterCriticalSection(&this->lock);

			if (this->dd->render.stretched)
			{
				if (this->dd->render.invalidate)
				{
					this->dd->render.invalidate = FALSE;
					RECT rc = { 0, 0, this->dd->render.width, this->dd->render.height };
					FillRect(this->dd->hDC, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
				}
				else
				{
					StretchBlt(this->dd->hDC,
						this->dd->render.viewport.x, this->dd->render.viewport.y,
						this->dd->render.viewport.width, this->dd->render.viewport.height,
						this->hDC, 0, 0, this->dd->width, this->dd->height, SRCCOPY);
				}
			}
			else
				BitBlt(this->dd->hDC, 0, 0, this->width, this->height, this->hDC, 
					this->dd->winRect.left, this->dd->winRect.top, SRCCOPY);

            if (showFPS > tick_start || DrawFPS)
                DrawText(this->dd->hDC, fpsString, -1, &textRect, DT_NOCLIP);

            EnumChildWindows(this->dd->hWnd, EnumChildProc, (LPARAM)this);

            LeaveCriticalSection(&this->lock);
            dropFrames = -1;
        }

        tick_end = timeGetTime();

#ifdef _DEBUG
        frame_time += (tick_end - tick_start);
        frames++;

        if (frames >= TargetFPS)
        {
            printf("Timed FPS: %.2f\n", 1000.0f / (frame_time / frames));
            printf("Real  FPS: %.2f\n", 1000.0f / ((tick_end - real_time) / frames));
            frame_time = frames = 0;
            real_time = tick_end;
        }
#endif

        tick_len = tick_end - tick_start;

        if (dropFrames == -1)
        {
            recent_frames[rIndex++] = tick_len;

            if (rIndex >= FRAME_SAMPLES)
                rIndex = 0;

            render_time = 0;
            for (int i = 0; i < FRAME_SAMPLES; ++i)
            {
                render_time += recent_frames[i] < TargetFrameLen ? TargetFrameLen : recent_frames[i];
            }

            avg_len = render_time / FRAME_SAMPLES;
            avg_fps = 1000 / avg_len;

            _snprintf(fpsString, 254, "FPS: %li\nTGT: %li\nDropped: %li", avg_fps, TargetFPS, totalDroppedFrames);
        }

        if (startTargetFPS != TargetFPS)
        {
            // TargetFPS was changed externally
            TargetFrameLen = 1000 / TargetFPS;
            startTargetFPS = TargetFPS;
        }

        if (tick_len < TargetFrameLen)
        {
            DWORD sleepTime = timeGetTime() - tick_start;
            if (sleepTime < TargetFrameLen)
                Sleep(TargetFrameLen - (timeGetTime() - tick_start));
        }
        else if (tick_len > TargetFrameLen)
        {
            dropFrames = (tick_len * 2) / TargetFrameLen;

            if (dropFrames > TargetFPS / 30)
            {
                showFPS = tick_end + tick_len + 2000;
            }
            totalDroppedFrames += dropFrames;
        }
    }
    return 0;
}

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

    this->hDC = CreateCompatibleDC(this->dd->hDC);

    /* Tiberian Sun sometimes tries to access lines that are past the bottom of the screen */
    int guardLines = 0; // doesn't work (yet)

    this->overlay = calloc(1, this->lPitch * (this->height + guardLines));

    this->bmi = calloc(1, sizeof(BITMAPINFO) + (this->lPitch * (this->height + guardLines)));
    this->bmi->bmiHeader.biSize = sizeof(BITMAPINFO);
    this->bmi->bmiHeader.biWidth = this->width;
    this->bmi->bmiHeader.biHeight = -this->height;
    this->bmi->bmiHeader.biPlanes = 1;
    this->bmi->bmiHeader.biBitCount = this->bpp;
    this->bmi->bmiHeader.biCompression = BI_RGB;

    this->bitmap = CreateDIBSection(this->hDC, this->bmi, DIB_RGB_COLORS, (void **)&this->surface, NULL, 0);
    SelectObject(this->hDC, this->bitmap);

    InitializeCriticalSection(&this->lock);

    if (this->dwCaps & DDSCAPS_PRIMARYSURFACE)
    {
        dprintf("Starting renderer.\n");
        this->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)render, (LPVOID)this, 0, NULL);
        if (SetThreadPriority(this->thread, THREAD_PRIORITY_ABOVE_NORMAL))
        {
            dprintf("Renderer set to higher priority.\n");
        }
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
                // The simplest possiblity: no scaling needed
                uint16_t *dest_base = this->surface + dst.left + (this->width * dst.top);
                uint16_t *src_base = srcImpl->surface + src.left + (srcImpl->width * src.top);

                while (dst_h-- > 0)
                {
                    memcpy((void *)dest_base, (void *)src_base, dst_byte_width);

                    dest_base += this->width;
                    src_base += srcImpl->width;
                }
            }
            else
            {
                /* Linear scaling using integer math
                 * Since the scaling pattern for x will aways be the same, the pattern itself gets pre-calculated
                 * and stored in an array.
                 * Y scaling pattern gets calculated during the blit loop
                 */
                unsigned int x_ratio = (unsigned int)((src_w << 16) / dst_w) + 1;
                unsigned int y_ratio = (unsigned int)((src_h << 16) / dst_h) + 1;

                unsigned int src_x, src_y;
                unsigned int dest_base, source_base;

                scale_pattern *pattern = malloc((dst_w + 1) * (sizeof(scale_pattern)));
                int pattern_idx = 0;
                unsigned int last_src_x = 0;

                if (pattern != NULL)
                {
                    pattern[pattern_idx] = (scale_pattern){ ONCE, 0, 0, 1 };

                    /* Build the pattern! */
                    for (int x = 1; x < dst_w; x++) {
                        src_x = (x * x_ratio) >> 16;
                        if (src_x == last_src_x)
                        {
                            if (pattern[pattern_idx].type == REPEAT || pattern[pattern_idx].type == ONCE)
                            {
                                pattern[pattern_idx].type = REPEAT;
                                pattern[pattern_idx].count++;
                            }
                            else if (pattern[pattern_idx].type == SEQUENCE)
                            {
                                pattern_idx++;
                                pattern[pattern_idx] = (scale_pattern){ REPEAT, x, src_x, 1 };
                            }
                        }
                        else if (src_x == last_src_x + 1)
                        {
                            if (pattern[pattern_idx].type == SEQUENCE || pattern[pattern_idx].type == ONCE)
                            {
                                pattern[pattern_idx].type = SEQUENCE;
                                pattern[pattern_idx].count++;
                            }
                            else if (pattern[pattern_idx].type == REPEAT)
                            {
                                pattern_idx++;
                                pattern[pattern_idx] = (scale_pattern){ ONCE, x, src_x, 1 };
                            }
                        }
                        else
                        {
                            pattern_idx++;
                            pattern[pattern_idx] = (scale_pattern){ ONCE, x, src_x, 1 };
                        }
                        last_src_x = src_x;
                    }
                    pattern[pattern_idx+1] = (scale_pattern){ END, 0, 0, 0 };


                    /* Do the actual blitting */
                    uint16_t *d, *s, v;
                    int count = 0;

                    for (int y = 0; y < dst_h; y++) {

                        dest_base = dst.left + this->width * (y + dst.top);

                        src_y = (y * y_ratio) >> 16;

                        source_base = src.left + srcImpl->width * (src_y + src.top);

                        pattern_idx = 0;
                        scale_pattern *current = &pattern[pattern_idx];
                        do {
                            switch(current->type)
                            {
                            case ONCE:
                                this->surface[dest_base + current->dst_index] =
                                    srcImpl->surface[source_base + current->src_index];
                                break;

                            case REPEAT:
                                d = (this->surface + dest_base + current->dst_index);
                                v = srcImpl->surface[source_base + current->src_index];

                                count = current->count;
                                while (count-- > 0)
                                    *d++ = v;

                                break;

                            case SEQUENCE:
                                d = this->surface + dest_base + current->dst_index;
                                s = srcImpl->surface + source_base + current->src_index;

                                memcpy((void *)d, (void *)s, current->count * this->lXPitch);
                                break;

                            case END:
                            default:
                                break;
                            }

                            current = &pattern[++pattern_idx];
                        } while (current->type != END);
                    }
                    free(pattern);
                }
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
            this->overlayBitmap = CreateCompatibleBitmap(this->dd->hDC, this->width, this->height);
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
    lpDDPixelFormat->dwRBitMask = 0x7C00;
    lpDDPixelFormat->dwGBitMask = 0x03E0;
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
        lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = 0x7C00;
        lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = 0x03E0;
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
        GetDIBits(this->overlayDC, this->overlayBitmap, 0, this->height, this->overlay, this->bmi, DIB_RGB_COLORS);

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
