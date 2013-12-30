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

#include <windows.h>
#include <ddraw.h>
#include <stdbool.h>
#include "main.h"
#include "IDirectDraw.h"

typedef struct IDirectDrawSurfaceImplVtbl IDirectDrawSurfaceImplVtbl;
typedef struct IDirectDrawSurfaceImpl IDirectDrawSurfaceImpl;

struct IDirectDrawSurfaceImpl
{
    IDirectDrawSurfaceImplVtbl *lpVtbl;

    IDirectDrawImpl *dd;
    IDirectDrawSurface *real;

    int ref;
    int bpp;
    int width;
    int height;
    int lPitch;
    int lXPitch;

    DWORD dwFlags;
    DWORD dwCaps;

    HANDLE hfm;
    unsigned short *surface;
    unsigned short *renderSurface;
    DDSURFACEDESC desc;
    PBITMAPINFO bmi;
    HBITMAP bitmap;
    HDC hDC;
    HANDLE thread;
    HANDLE frame;

    unsigned short *overlay;
    HDC overlayDC;
    HBITMAP overlayBitmap;
};

struct IDirectDrawSurfaceImplVtbl
{
    /* IUnknown */
    HRESULT (__stdcall *QueryInterface)(IDirectDrawSurfaceImpl*, REFIID, void**);
    ULONG (__stdcall *AddRef)(IDirectDrawSurfaceImpl*);
    ULONG (__stdcall *Release)(IDirectDrawSurfaceImpl*);

    /* IDirectDrawSurface */
    HRESULT (__stdcall *AddAttachedSurface)(IDirectDrawSurfaceImpl*, LPDIRECTDRAWSURFACE);
    HRESULT (__stdcall *AddOverlayDirtyRect)(IDirectDrawSurfaceImpl*, LPRECT);
    HRESULT (__stdcall *Blt)(IDirectDrawSurfaceImpl*, LPRECT,LPDIRECTDRAWSURFACE, LPRECT,DWORD, LPDDBLTFX);
    HRESULT (__stdcall *BltBatch)(IDirectDrawSurfaceImpl*, LPDDBLTBATCH, DWORD, DWORD );
    HRESULT (__stdcall *BltFast)(IDirectDrawSurfaceImpl*, DWORD,DWORD,LPDIRECTDRAWSURFACE, LPRECT,DWORD);
    HRESULT (__stdcall *DeleteAttachedSurface)(IDirectDrawSurfaceImpl*, DWORD,LPDIRECTDRAWSURFACE);
    HRESULT (__stdcall *EnumAttachedSurfaces)(IDirectDrawSurfaceImpl*, LPVOID,LPDDENUMSURFACESCALLBACK);
    HRESULT (__stdcall *EnumOverlayZOrders)(IDirectDrawSurfaceImpl*, DWORD,LPVOID,LPDDENUMSURFACESCALLBACK);
    HRESULT (__stdcall *Flip)(IDirectDrawSurfaceImpl*, LPDIRECTDRAWSURFACE, DWORD);
    HRESULT (__stdcall *GetAttachedSurface)(IDirectDrawSurfaceImpl*, LPDDSCAPS, LPDIRECTDRAWSURFACE FAR *);
    HRESULT (__stdcall *GetBltStatus)(IDirectDrawSurfaceImpl*, DWORD);
    HRESULT (__stdcall *GetCaps)(IDirectDrawSurfaceImpl*, LPDDSCAPS);
    HRESULT (__stdcall *GetClipper)(IDirectDrawSurfaceImpl*, LPDIRECTDRAWCLIPPER FAR*);
    HRESULT (__stdcall *GetColorKey)(IDirectDrawSurfaceImpl*, DWORD, LPDDCOLORKEY);
    HRESULT (__stdcall *GetDC)(IDirectDrawSurfaceImpl*, HDC FAR *);
    HRESULT (__stdcall *GetFlipStatus)(IDirectDrawSurfaceImpl*, DWORD);
    HRESULT (__stdcall *GetOverlayPosition)(IDirectDrawSurfaceImpl*, LPLONG, LPLONG );
    HRESULT (__stdcall *GetPalette)(IDirectDrawSurfaceImpl*, LPDIRECTDRAWPALETTE FAR*);
    HRESULT (__stdcall *GetPixelFormat)(IDirectDrawSurfaceImpl*, LPDDPIXELFORMAT);
    HRESULT (__stdcall *GetSurfaceDesc)(IDirectDrawSurfaceImpl*, LPDDSURFACEDESC);
    HRESULT (__stdcall *Initialize)(IDirectDrawSurfaceImpl*, LPDIRECTDRAW, LPDDSURFACEDESC);
    HRESULT (__stdcall *IsLost)(IDirectDrawSurfaceImpl*);
    HRESULT (__stdcall *Lock)(IDirectDrawSurfaceImpl*, LPRECT,LPDDSURFACEDESC,DWORD,HANDLE);
    HRESULT (__stdcall *ReleaseDC)(IDirectDrawSurfaceImpl*, HDC);
    HRESULT (__stdcall *Restore)(IDirectDrawSurfaceImpl*);
    HRESULT (__stdcall *SetClipper)(IDirectDrawSurfaceImpl*, LPDIRECTDRAWCLIPPER);
    HRESULT (__stdcall *SetColorKey)(IDirectDrawSurfaceImpl*, DWORD, LPDDCOLORKEY);
    HRESULT (__stdcall *SetOverlayPosition)(IDirectDrawSurfaceImpl*, LONG, LONG );
    HRESULT (__stdcall *SetPalette)(IDirectDrawSurfaceImpl*, LPDIRECTDRAWPALETTE);
    HRESULT (__stdcall *Unlock)(IDirectDrawSurfaceImpl*, LPVOID);
    HRESULT (__stdcall *UpdateOverlay)(IDirectDrawSurfaceImpl*, LPRECT, LPDIRECTDRAWSURFACE,LPRECT,DWORD, LPDDOVERLAYFX);
    HRESULT (__stdcall *UpdateOverlayDisplay)(IDirectDrawSurfaceImpl*, DWORD);
    HRESULT (__stdcall *UpdateOverlayZOrder)(IDirectDrawSurfaceImpl*, DWORD, LPDIRECTDRAWSURFACE);
};

IDirectDrawSurfaceImpl *IDirectDrawSurfaceImpl_construct(IDirectDrawImpl*, LPDDSURFACEDESC);
