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
#include <stdbool.h>
#include "ddraw.h"

#ifndef _MAIN_
#define _MAIN_

#ifdef _DEBUG

    #include <stdio.h>

    int dprintf(const char *, ...);

    extern int PROXY;
    extern int VERBOSE;
    extern int FPS;

#else
    #define dprintf(...)

    #define PROXY 0
    #define VERBOSE 0
    #define SYNC 0

#endif

// TODO: remove these
#define ENTER
#define LEAVE

void dump_ddcaps(LPDDCAPS);
void dump_ddsurfacedesc(LPDDSURFACEDESC);
void dump_ddpixelformat(LPDDPIXELFORMAT);
void dump_ddbltfx(LPDDBLTFX);
void DebugPrint(const char *format, ...);

bool GameHandlesClose;
bool DrawFPS;
DWORD TargetFPS;
DWORD TargetFrameLen;
DWORD FrameDropMode;
#define FRAMEDROP_NONE 0
#define FRAMEDROP_MEDIUM 1
#define FRAMEDROP_AGGRESSIVE 2
#define FRAMEDROP_ULTRA 3

#define debug_(format, ...) DebugPrint("xDBG " format, ##__VA_ARGS__)

#endif
