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

#include "opengl.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include "glext.h"

const GLchar *PassthroughVertShaderSrc =
    "#version 330\n"
    "in vec4 VertexCoord;\n"
    "in vec4 COLOR;\n"
    "in vec4 TexCoord;\n"
    "out vec4 COL0;\n"
    "out vec4 TEX0;\n"
    "uniform mat4 MVPMatrix;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    gl_Position = MVPMatrix * VertexCoord;\n"
    "    COL0 = COLOR;\n"
    "    TEX0.xy = TexCoord.xy;\n"
    "}\n";

const GLchar *ConvFragShaderSrc =
    "#version 330\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D SurfaceTex;\n"
    "in vec4 TEX0;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec4 texel = texture(SurfaceTex, TEX0.xy);\n"
    "    int bytes = int(texel.r * 255.0 + 0.5) | int(texel.g * 255.0 + 0.5) << 8;\n"
    "    vec4 colors;\n"
    "    colors.r = float(bytes >> 11) / 31.0;\n"
    "    colors.g = float((bytes >> 5) & 63) / 63.0;\n"
    "    colors.b = float(bytes & 31) / 31.0;\n"
    "    colors.a = 0;\n"
    "    FragColor = colors;\n"
    "}\n";

static IDirectDrawSurfaceImplVtbl Vtbl;

BOOL ShouldStretch(IDirectDrawSurfaceImpl *this)
{
    if (!this->dd->render.stretched)
        return FALSE;

    CURSORINFO pci;
    pci.cbSize = sizeof(CURSORINFO);
    if (GetCursorInfo(&pci))
        return pci.flags == 0;

    return TRUE;
}

/* the TS hack itself */
BOOL CALLBACK EnumChildProc(HWND hWnd, LPARAM lParam)
{
    IDirectDrawSurfaceImpl *this = (IDirectDrawSurfaceImpl *)lParam;

    HDC hDC = GetDC(hWnd);

    RECT size;
    GetClientRect(hWnd, &size);

    RECT pos;
    GetWindowRect(hWnd, &pos);

    if (this->usingPBO && InterlockedExchangeAdd(&Renderer, 0) == RENDERER_OPENGL)
    {
        //the GDI struggle is real
        // Copy the scanlines of menu windows from pboSurface to the gdi surface
        SetDIBits(NULL, this->bitmap, pos.top, pos.bottom - pos.top, this->surface, this->bmi, DIB_RGB_COLORS);
    }

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
    char fpsOglString[256] = "OpenGL\nFPS: NA\nTGT: NA\nDropped: NA";
    char fpsGDIString[256] = "GDI\nFPS: NA\nTGT: NA\nDropped: NA";
    DWORD avg_fps = 0;

#ifdef _DEBUG
    double frame_time = 0, real_time = timeGetTime();
    int frames = 0;
#endif

    bool failToGDI = false;
    GLenum gle;

    wglMakeCurrent(this->dd->hDC, this->dd->glInfo.hRC_render);

    failToGDI = failToGDI || ((gle = glGetError()) != GL_NO_ERROR);
    if (gle != GL_NO_ERROR)
        dprintf("wglMakeCurrent, %x\n", gle);

    this->dd->glInfo.initialized = true;

    if (wglSwapIntervalEXT)
    {
        wglSwapIntervalEXT(SwapInterval);
        failToGDI = failToGDI || ((gle = glGetError()) != GL_NO_ERROR);
    }
    if (gle != GL_NO_ERROR)
        dprintf("wglSwapIntervalEXT, %x\n", gle);


    BOOL gotOpenglV3 = glGenFramebuffers && glBindFramebuffer && glFramebufferTexture2D && glDrawBuffers &&
        glCheckFramebufferStatus && glUniform4f && glActiveTexture && glUniform1i &&
        glGetAttribLocation && glGenBuffers && glBindBuffer && glBufferData && glVertexAttribPointer &&
        glEnableVertexAttribArray && glUniform2fv && glUniformMatrix4fv && glGenVertexArrays && glBindVertexArray &&
        glGetUniformLocation;

    GLuint convProgram = 0;
    if (gotOpenglV3)
        convProgram = OpenGL_BuildProgram(PassthroughVertShaderSrc, ConvFragShaderSrc);

    GLenum texFormat, texType;

    glGenTextures(2, this->textures);

    failToGDI = failToGDI || ((gle = glGetError()) != GL_NO_ERROR);
    if (gle != GL_NO_ERROR)
        dprintf("glGenTextures, %x\n", gle);

    for (int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, this->textures[i]);

        if (convProgram)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, this->width, this->height, 0, texFormat = GL_RG, texType = GL_UNSIGNED_BYTE, NULL);
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, this->width, this->height, 0, texFormat = GL_RGB, texType = GL_UNSIGNED_SHORT_5_6_5, NULL);
            if ((gle = glGetError()) != GL_NO_ERROR)
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5, this->width, this->height, 0, texFormat = GL_RGB, texType = GL_UNSIGNED_SHORT_5_6_5, NULL);
        }

        failToGDI = failToGDI || ((gle = glGetError()) != GL_NO_ERROR);
        if (gle != GL_NO_ERROR)
            dprintf("glTexImage2D, %x\n", gle);


        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        failToGDI = failToGDI || ((gle = glGetError()) != GL_NO_ERROR);
        if (gle != GL_NO_ERROR)
            dprintf("glTexParameteri MIN, %x\n", gle);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        failToGDI = failToGDI || ((gle = glGetError()) != GL_NO_ERROR);
        if (gle != GL_NO_ERROR)
            dprintf("glTexParameteri MAG, %x\n", gle);


        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

        if (gle != GL_NO_ERROR)
            dprintf("glTexParameteri MAX, %x\n", gle);
    }

    GLuint vao, vaoBuffers[3];

    if (convProgram)
    {
        glUseProgram(convProgram);

        GLint vertexCoordAttrLoc = glGetAttribLocation(convProgram, "VertexCoord");
        GLint texCoordAttrLoc = glGetAttribLocation(convProgram, "TexCoord");

        glGenBuffers(3, vaoBuffers);

        glBindBuffer(GL_ARRAY_BUFFER, vaoBuffers[0]);
        static const GLfloat vertexCoord[] = {
            -1.0f, 1.0f,
            1.0f, 1.0f,
            1.0f,-1.0f,
            -1.0f,-1.0f,
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoord), vertexCoord, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ARRAY_BUFFER, vaoBuffers[1]);
        static const GLfloat texCoord[] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f,
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(texCoord), texCoord, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vaoBuffers[0]);
        glVertexAttribPointer(vertexCoordAttrLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray(vertexCoordAttrLoc);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ARRAY_BUFFER, vaoBuffers[1]);
        glVertexAttribPointer(texCoordAttrLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray(texCoordAttrLoc);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vaoBuffers[2]);
        static const GLushort indices[] =
        {
            0, 1, 2,
            0, 2, 3,
        };
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glBindVertexArray(0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        static const float mvpMatrix[16] = {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0,
            0,0,0,1,
        };
        glUniformMatrix4fv(glGetUniformLocation(convProgram, "MVPMatrix"), 1, GL_FALSE, mvpMatrix);
    }
    else
        glEnable(GL_TEXTURE_2D);

    failToGDI = failToGDI || ((gle = glGetError()) != GL_NO_ERROR);
    if (gle != GL_NO_ERROR)
        dprintf("glEnable, %x\n", gle);

    tick_start = timeGetTime();

    if (failToGDI)
    {
        InterlockedExchange(&Renderer, RENDERER_GDI);
        this->dd->glInfo.glSupported = false;
    }
    DWORD wfso;

    while (this->thread && ( wfso = WaitForSingleObject(this->syncEvent, TargetFrameLen) ) != WAIT_FAILED)
    {
        static int texIndex = 0;
        texIndex = (texIndex + 1) % 2;

        if (dropFrames > 0)
            dropFrames--;
        else
        {
            switch (InterlockedExchangeAdd(&Renderer, 0))
            {
            case RENDERER_GDI:
                EnterCriticalSection(&this->lock);
                if (DrawFPS && (showFPS > tick_start || DrawFPS == 1))
                {
                    textRect.left = this->dd->winRect.left;
                    textRect.top = this->dd->winRect.top;
                    DrawText(this->hDC, fpsGDIString, -1, &textRect, DT_NOCLIP);
                }

                if (ShouldStretch(this))
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
                            this->hDC, this->dd->winRect.left, this->dd->winRect.top, this->dd->width, this->dd->height, SRCCOPY);
                    }
                }
                else
                {
                    if (this->dd->render.stretched)
                        this->dd->render.invalidate = TRUE;

                    BitBlt(this->dd->hDC, 0, 0, this->width, this->height, this->hDC,
                        this->dd->winRect.left, this->dd->winRect.top, SRCCOPY);
                }
                LeaveCriticalSection(&this->lock);
                break;

            case RENDERER_OPENGL:

                wglMakeCurrent(this->dd->hDC, this->dd->glInfo.hRC_render);
                EnterCriticalSection(&this->lock);
                if (DrawFPS && (showFPS > tick_start || DrawFPS == 1))
                {
                    textRect.left = this->dd->winRect.left;
                    textRect.top = this->dd->winRect.top;

                    if (this->usingPBO)
                    {
                        // Copy the scanlines that will be behind the FPS counter to the GDI surface
                        memcpy((uint8_t*)this->systemSurface + (textRect.top * this->lPitch),
                            (uint8_t*)this->surface + (textRect.top * this->lPitch),
                            textRect.bottom * this->lPitch);
                        SelectObject(this->hDC, this->bitmap);
                    }

                    textRect.bottom = DrawText(this->hDC, fpsOglString, -1, &textRect, DT_NOCLIP);

                    if (this->usingPBO)
                    {
                        // Copy the scanlines from the gdi surface back to pboSurface
                        memcpy((uint8_t*)this->surface + (textRect.top * this->lPitch),
                            (uint8_t*)this->systemSurface + (textRect.top * this->lPitch),
                            textRect.bottom * this->lPitch);
                        SelectObject(this->hDC, this->defaultBM);
                    }
                }

                glBindTexture(GL_TEXTURE_2D, this->textures[texIndex]);
                if (this->usingPBO)
                {
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->pbo[this->pboIndex]);

                    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, this->width, this->height, texFormat, texType, 0);

                    this->pboIndex++;
                    if (this->pboIndex >= this->pboCount)
                        this->pboIndex = 0;

                    if (this->pboCount > 1)
                    {
                        glBindBuffer(GL_PIXEL_PACK_BUFFER, this->pbo[this->pboIndex]);
                        glGetTexImage(GL_TEXTURE_2D, 0, texFormat, texType, 0);
                    }
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->pbo[this->pboIndex]);
                    this->surface = (void*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_WRITE);

                }
                else
                {
                    glPixelStorei(GL_UNPACK_ROW_LENGTH, this->width);
                    glPixelStorei(GL_UNPACK_SKIP_PIXELS, this->dd->winRect.left);
                    glPixelStorei(GL_UNPACK_SKIP_ROWS, this->dd->winRect.top);

                    glTexSubImage2D(GL_TEXTURE_2D, 0, this->dd->winRect.left, this->dd->winRect.top, this->dd->width, this->dd->height, texFormat, texType, this->surface);

                    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
                    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
                }

                LeaveCriticalSection(&this->lock);

                if (ShouldStretch(this))
                    glViewport(-this->dd->winRect.left, this->dd->winRect.bottom - this->dd->render.viewport.height,
                        this->dd->render.viewport.width, this->dd->render.viewport.height);
                else
                    glViewport(-this->dd->winRect.left, this->dd->winRect.bottom - this->height, this->width, this->height);

                if (convProgram)
                {
                    glBindVertexArray(vao);
                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
                    glBindVertexArray(0);
                }
                else
                {
                    glBegin(GL_TRIANGLE_FAN);
                    glTexCoord2f(0, 0); glVertex2f(-1, 1);
                    glTexCoord2f(1, 0); glVertex2f(1, 1);
                    glTexCoord2f(1, 1); glVertex2f(1, -1);
                    glTexCoord2f(0, 1); glVertex2f(-1, -1);
                    glEnd();
                }

                SwapBuffers(this->dd->hDC);

                wglMakeCurrent(NULL, NULL);

                break;

            default:
                break;
            }


            EnumChildWindows(this->dd->hWnd, EnumChildProc, (LPARAM)this);

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

        if (dropFrames == -1 && DrawFPS)
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

            _snprintf(fpsOglString, 254, "OpenGL%d\nFPS: %li\nTGT: %li\nDropped: %li", convProgram?3:1, avg_fps, TargetFPS, totalDroppedFrames);
            _snprintf(fpsGDIString, 254, "GDI\nFPS: %li\nTGT: %li\nDropped: %li", avg_fps, TargetFPS, totalDroppedFrames);
        }

        if (startTargetFPS != TargetFPS)
        {
            // TargetFPS was changed externally
            TargetFrameLen = 1000 / TargetFPS;
            startTargetFPS = TargetFPS;
        }

        if (tick_len < TargetFrameLen)
        {
            Sleep(TargetFrameLen - tick_len);
        }
        else if (tick_len > TargetFrameLen)
        {/*
            dropFrames = (tick_len * 2) / TargetFrameLen;

            if (dropFrames > TargetFPS / 30)
            {
                showFPS = tick_end + tick_len + 2000;
            }
            totalDroppedFrames += dropFrames;*/
        }
        if (InterlockedCompareExchange(&this->dd->focusGained, false, true))
        {
            switch (InterlockedExchangeAdd(&Renderer, 0))
            {
            case RENDERER_OPENGL:
                break;
            case RENDERER_GDI:
                if (InterlockedExchangeAdd(&PrimarySurfacePBO, 0))
                {
                    this->surface = this->systemSurface;
                    SelectObject(this->hDC, this->bitmap);
                }
                break;

            default: break;
            }
        }
        tick_start = timeGetTime();
        ResetEvent(this->syncEvent);
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
        this->dd->glInfo.glSupported = true;

        GLenum gle;
        int i = 0;

        this->dd->glInfo.hRC_render = wglCreateContext(this->dd->hDC);
        this->dd->glInfo.hRC_main = wglCreateContext(this->dd->hDC);
        wglShareLists(this->dd->glInfo.hRC_render, this->dd->glInfo.hRC_main);

        wglMakeCurrent(this->dd->hDC, this->dd->glInfo.hRC_main);
        OpenGL_Init();

        this->pboCount = InterlockedExchangeAdd(&PrimarySurfacePBO, 0);
        this->pbo = calloc(this->pboCount, sizeof(GLuint));
        this->pboIndex = 0;
        if (glGenBuffers)
        {
            glGenBuffers(this->pboCount, this->pbo);

            gle = glGetError();
            if (gle != GL_NO_ERROR)
            {
                dprintf("glGenBuffers, %x\n", gle);
                goto no_pbo;
            }
        }

        if (glBindBuffer && gle == GL_NO_ERROR)
        {
            for (i = 0; i < this->pboCount; ++i)
            {
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->pbo[i]);
                gle = glGetError();
                if (gle != GL_NO_ERROR)
                {
                    dprintf("glBindBuffer, %x\n", gle);
                    goto no_pbo;
                }

                glBufferData(GL_PIXEL_UNPACK_BUFFER, this->height * this->lPitch, 0, GL_STREAM_DRAW);
                gle = glGetError();
                if (gle != GL_NO_ERROR)
                {
                    dprintf("glBufferData, %x\n", gle);
                    goto no_pbo;
                }
            }

            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->pbo[0]);

            if (glMapBuffer)
            {
                this->pboSurface = (void*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_WRITE);

                gle = glGetError();
                if (gle != GL_NO_ERROR)
                {
                    dprintf("glMapBuffer, %x\n", gle);
                    goto no_pbo;
                }

                this->systemSurface = this->surface;

                if (InterlockedExchangeAdd(&PrimarySurfacePBO, 0) && InterlockedExchangeAdd(&Renderer, 0) == RENDERER_OPENGL)
                {
                    this->usingPBO = true;

                    this->surface = this->pboSurface;
                    SelectObject(this->hDC, this->defaultBM);
                }

            }
            else
            {
                goto no_pbo;
            }
        }
        else
        {
        no_pbo:
            this->dd->glInfo.pboSupported = false;
            this->usingPBO = false;
            this->pboSurface = NULL;
        }

        this->syncEvent = CreateEvent(NULL, true, false, NULL);

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
