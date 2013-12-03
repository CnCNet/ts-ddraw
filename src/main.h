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

#ifndef _MAIN_
#define _MAIN_

#ifdef _DEBUG

    #include <stdio.h>
    int dprintf(const char *, ...);
    #define ENTER if (SYNC) EnterCriticalSection(&this->dd->cs)
    #define LEAVE if (SYNC) { LeaveCriticalSection(&this->dd->cs); } fflush(stdout)

    extern int PROXY;
    extern int VERBOSE;
    extern int SYNC;

#else

    #define dprintf(...)
    #define ENTER
    #define LEAVE 

    #define PROXY 0
    #define VERBOSE 0
    #define SYNC 0

#endif


void dump_ddcaps(LPDDCAPS);
void dump_ddsurfacedesc(LPDDSURFACEDESC);
void dump_ddpixelformat(LPDDPIXELFORMAT);
void dump_ddbltfx(LPDDBLTFX);

#endif
