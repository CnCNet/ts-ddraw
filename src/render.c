#include "main.h"
#include "IDirectDrawClipper.h"
#include "IDirectDrawSurface.h"
#include <stdint.h>
#include <stdio.h>
#include "counter.h"

#include "opengl.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include "glext.h"

const GLchar *PassthroughVertShaderSrc =
    "#version 130\n"
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
    "#version 130\n"
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


DWORD WINAPI render(IDirectDrawSurfaceImpl *this)
{
    GdiSetBatchLimit(1);

    // Begin OpenGL Setup
    bool failToGDI = false;
    BOOL gotOpenglV3;
    GLuint convProgram = 0;
    GLenum texFormat = GL_RGB, texType = GL_RGB;
    GLuint vao, vaoBuffers[3];
    GLint vertexCoordAttrLoc, texCoordAttrLoc;

    if (InterlockedExchangeAdd(&Renderer, 0) == RENDERER_OPENGL)
    {
        this->dd->glInfo.glSupported = true;

        GLenum gle = GL_NO_ERROR;

        this->dd->glInfo.hRC_main = wglCreateContext(this->dd->hDC);
        this->dd->glInfo.hRC_render = wglCreateContext(this->dd->hDC);
        wglShareLists(this->dd->glInfo.hRC_render, this->dd->glInfo.hRC_main);

        wglMakeCurrent(this->dd->hDC, this->dd->glInfo.hRC_render);
        OpenGL_Init();

        this->pboCount = InterlockedExchangeAdd(&PrimarySurfacePBO, 0);
        this->pbo = calloc(this->pboCount, sizeof(GLuint));
        this->pboIndex = 0;

        this->dd->glInfo.initialized = true;

        if (wglSwapIntervalEXT)
        {
            wglSwapIntervalEXT(SwapInterval);
            failToGDI = failToGDI || ((gle = glGetError()) != GL_NO_ERROR);
        }
        if (gle != GL_NO_ERROR)
            dprintf("wglSwapIntervalEXT, %x\n", gle);


        gotOpenglV3 = glGenFramebuffers && glBindFramebuffer && glFramebufferTexture2D && glDrawBuffers &&
            glCheckFramebufferStatus && glUniform4f && glActiveTexture && glUniform1i &&
            glGetAttribLocation && glGenBuffers && glBindBuffer && glBufferData && glVertexAttribPointer &&
            glEnableVertexAttribArray && glUniform2fv && glUniformMatrix4fv && glGenVertexArrays && glBindVertexArray &&
            glGetUniformLocation;

        if (gotOpenglV3)
            convProgram = OpenGL_BuildProgram(PassthroughVertShaderSrc, ConvFragShaderSrc);

        glGenTextures(2, &this->textures[0]);

        failToGDI = failToGDI || ((gle = glGetError()) != GL_NO_ERROR);
        if (gle != GL_NO_ERROR)
            dprintf("glGenTextures, %x\n", gle);

        for (int i = 0; i < 2; i++)
        {
            glBindTexture(GL_TEXTURE_2D, this->textures[i]);

            failToGDI = failToGDI || ((gle = glGetError()) != GL_NO_ERROR);
            if (gle != GL_NO_ERROR)
                dprintf("glBindTexture, %x\n", gle);

            if (convProgram)
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, this->width, this->height, 0, texFormat = GL_RG, texType = GL_UNSIGNED_BYTE, NULL);
                if (glGetError() != GL_NO_ERROR)
                {
                    convProgram = 0;
                    goto no_shader;
                }
            }
            else
            {
            no_shader:
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, this->width, this->height, 0, texFormat = GL_RGB, texType = GL_UNSIGNED_SHORT_5_6_5, NULL);
                if ((gle = glGetError()) != GL_NO_ERROR)
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5, this->width, this->height, 0, texFormat = GL_RGB, texType = GL_UNSIGNED_SHORT_5_6_5, NULL);
            }

            failToGDI = failToGDI || ((gle = glGetError()) != GL_NO_ERROR);
            if (gle != GL_NO_ERROR)
                dprintf("glTexImage2D i = %d, %x\n", i, gle);

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

        if (convProgram)
        {
            glUseProgram(convProgram);

            vertexCoordAttrLoc = glGetAttribLocation(convProgram, "VertexCoord");
            texCoordAttrLoc = glGetAttribLocation(convProgram, "TexCoord");

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
            static const GLushort indices[] = {
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

        if (glBindBuffer && gle == GL_NO_ERROR  && this->pboCount)
        {

            for (int i = 0; i < this->pboCount; ++i)
            {
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->pbo[i]);
                gle = glGetError();
                if (gle != GL_NO_ERROR)
                {
                    dprintf("glBindBuffer %i, %x\n", i, gle);
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
            this->surface = this->systemSurface;
        }
    }

    if (failToGDI)
    {
        InterlockedExchange(&Renderer, RENDERER_GDI);

        this->surface = this->systemSurface;
        this->dd->glInfo.glSupported = false;
        if (this->usingPBO)
        {
            this->usingPBO = false;
            this->pboSurface = NULL;
            SelectObject(this->hDC, this->bitmap);
        }
        wglMakeCurrent(NULL, NULL);
    }

    SetEvent(this->pSurfaceReady);
    // End OpenGL Setup




    Sleep(500);

    if (failToGDI)
    {
        SendMessage(this->dd->hWnd, WM_ACTIVATE, WA_INACTIVE, 0);
        Sleep(50);
        ShowWindow(this->dd->hWnd, SW_RESTORE);
        SendMessage(this->dd->hWnd, WM_ACTIVATE, WA_ACTIVE, 0);
    }

    if (InterlockedExchangeAdd(&Renderer, 0) == RENDERER_OPENGL && SwapInterval > 0)
    {
        DEVMODE lpDevMode;
        memset(&lpDevMode, 0, sizeof(DEVMODE));
        lpDevMode.dmSize = sizeof(DEVMODE);
        lpDevMode.dmDriverExtra = 0;

        if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &lpDevMode))
        {
            TargetFPS = (double)lpDevMode.dmDisplayFrequency;
        }
    }

    double tick_time = 0.0;
    TargetFrameLen = 1000.0 / TargetFPS;
    double startTargetFPS = TargetFPS;

    double avg_len = TargetFrameLen;
    double recent_frames[FRAME_SAMPLES] = { -1.0 };
    double render_time = 0.0;
    double best_time = 0.0;
    int rIndex = 0;

    RECT textRect = (RECT){0,0,0,0};
    char fpsOglString[256] = "OpenGL\nFPS: NA\nTGT: NA\n";
    char fpsGDIString[256] = "GDI\nFPS: NA\nTGT: NA\n";
    double avg_fps = 0;

    // Vsync calculator variables
    double floor = 0;
    double ceiling = TargetFrameLen + 1;
    bool descending = true;
    double sleep = TargetFrameLen;
    double previous_best = TargetFrameLen * 2;
    double best_sleep = 0;

    CounterStart();

    while (this->thread)
    {
        static int texIndex = 0;
        if (PrimarySurface2Tex)
            texIndex = (texIndex + 1) % 2;

        {
            switch (InterlockedExchangeAdd(&Renderer, 0))
            {
            case RENDERER_GDI:
                EnterCriticalSection(&this->lock);
                if (DrawFPS)
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
                if (DrawFPS)
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

                static int errorCheckCount = 0;
                if (AutoRenderer && errorCheckCount < 3)
                {
                    errorCheckCount++;
                    glFinish();

                    if (glGetError() != GL_NO_ERROR || errorCheckCount == 3)
                        SendMessage(this->dd->hWnd, WM_SWITCHRENDERER, 0, 0);
                }

                wglMakeCurrent(NULL, NULL);

                break;

            default:
                break;
            }


            EnumChildWindows(this->dd->hWnd, EnumChildProc, (LPARAM)this);
        }

        tick_time = CounterGet();

        recent_frames[rIndex++] = tick_time;

        if (rIndex >= FRAME_SAMPLES)
            rIndex = 0;

        render_time = 0.0;
        best_time = 0.0;
        int bCount = 0;
        for (int i = 0; i < FRAME_SAMPLES; ++i)
        {
            render_time += recent_frames[i] < TargetFrameLen ? TargetFrameLen : recent_frames[i];
            if (recent_frames[i] > 0)
            {
                best_time += recent_frames[i];
                bCount++;
            }
        }

        avg_fps = 1000.0 / (render_time / FRAME_SAMPLES);

        avg_len = best_time / bCount;

        if (DrawFPS)
        {
            _snprintf(fpsOglString, 254, "OpenGL%d\nFPS: %3.0f\nTGT: %3.0f\nRender Time: %2.3f ms", convProgram?3:1, avg_fps, TargetFPS, avg_len);
            _snprintf(fpsGDIString, 254, "GDI\nFPS: %3.0f\nTGT: %3.0f\nRender Time: %2.3f ms", avg_fps, TargetFPS, avg_len);
        }

        if (startTargetFPS != TargetFPS)
        {
            // TargetFPS was changed externally
            TargetFrameLen = 1000.0 / TargetFPS;
            startTargetFPS = TargetFPS;
        }

        tick_time = CounterGet();
        if (SwapInterval < 1)
        {
            if (tick_time < TargetFrameLen)
            {
                int sleep = (int)(TargetFrameLen - tick_time);
                Sleep(sleep);

                // Finish sub-millisecond sleep
                while (CounterGet() < TargetFrameLen);
            }
        }
        else if (InterlockedExchangeAdd(&Renderer, 0) == RENDERER_OPENGL)
        {
            // Calculate optimal sleep time for vsync mode.
            if (bCount == FRAME_SAMPLES  && rIndex == FRAME_SAMPLES - 1)
            {
                if (avg_len < previous_best)
                {
                    previous_best = avg_len;
                    best_sleep = sleep;
                }
                else
                {
                    descending = sleep > best_sleep;
                    if (descending)
                        ceiling = (ceiling + sleep) / 2;
                    else
                        floor = (floor - 0.5 + sleep) / 2;
                }
                if (descending)
                    sleep = (sleep + floor)/2;
                else
                    sleep = ((ceiling - sleep)/2) + sleep;
            }

            if (sleep > TargetFrameLen || sleep < 1) sleep = TargetFrameLen;

            CounterStart();
            Sleep((int)sleep);
            while (CounterGet() < sleep);
        }

        if (InterlockedCompareExchange(&this->dd->focusGained, false, true))
        {
            switch (InterlockedExchangeAdd(&Renderer, 0))
            {
            case RENDERER_OPENGL:
                break;
            case RENDERER_GDI:
                if (this->usingPBO)
                {
                    this->surface = this->systemSurface;
                    SelectObject(this->hDC, this->bitmap);
                }
                break;

            default: break;
            }
        }
        CounterStart();
    }

    return 0;
}
