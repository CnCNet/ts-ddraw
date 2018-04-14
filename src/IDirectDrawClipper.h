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
#include "ddraw.h"
#include "main.h"

typedef struct IDirectDrawClipperImplVtbl IDirectDrawClipperImplVtbl;
typedef struct IDirectDrawClipperImpl IDirectDrawClipperImpl;

struct IDirectDrawClipperImplVtbl
{
    /* IUnknown */
    HRESULT (__stdcall *QueryInterface)(IDirectDrawClipperImpl *, REFIID, void **);
    ULONG (__stdcall *AddRef)(IDirectDrawClipperImpl *);
    ULONG (__stdcall *Release)(IDirectDrawClipperImpl *);

    /* IDirectDrawClipper */
    HRESULT (__stdcall *GetClipList)(IDirectDrawClipperImpl *, LPRECT, LPRGNDATA, LPDWORD);
    HRESULT (__stdcall *GetHWnd)(IDirectDrawClipperImpl *, HWND FAR *);
    HRESULT (__stdcall *Initialize)(IDirectDrawClipperImpl *, LPDIRECTDRAW, DWORD);
    HRESULT (__stdcall *IsClipListChanged)(IDirectDrawClipperImpl *, BOOL FAR *);
    HRESULT (__stdcall *SetClipList)(IDirectDrawClipperImpl *, LPRGNDATA,DWORD);
    HRESULT (__stdcall *SetHWnd)(IDirectDrawClipperImpl *, DWORD, HWND );
};

struct IDirectDrawClipperImpl
{
    struct IDirectDrawClipperImplVtbl *lpVtbl;

    IDirectDrawClipper *real;

    int ref;
    HWND hWnd;
};

IDirectDrawClipperImpl *IDirectDrawClipperImpl_construct();
