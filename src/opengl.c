#include <windows.h>
#include <stdio.h>
#include "opengl.h"
#include "main.h"

// Program
PFNGLCREATEPROGRAMPROC glCreateProgram = NULL;
PFNGLDELETEPROGRAMPROC glDeleteProgram = NULL;
PFNGLUSEPROGRAMPROC glUseProgram = NULL;
PFNGLATTACHSHADERPROC glAttachShader = NULL;
PFNGLDETACHSHADERPROC glDetachShader = NULL;
PFNGLLINKPROGRAMPROC glLinkProgram = NULL;
PFNGLGETPROGRAMIVPROC glGetProgramiv = NULL;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = NULL;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = NULL;
PFNGLUNIFORM1IPROC glUniform1i = NULL;
PFNGLUNIFORM1IVPROC glUniform1iv = NULL;
PFNGLUNIFORM2IVPROC glUniform2iv = NULL;
PFNGLUNIFORM3IVPROC glUniform3iv = NULL;
PFNGLUNIFORM4IVPROC glUniform4iv = NULL;
PFNGLUNIFORM1FPROC glUniform1f = NULL;
PFNGLUNIFORM1FVPROC glUniform1fv = NULL;
PFNGLUNIFORM2FVPROC glUniform2fv = NULL;
PFNGLUNIFORM3FVPROC glUniform3fv = NULL;
PFNGLUNIFORM4FVPROC glUniform4fv = NULL;
PFNGLUNIFORM4FPROC glUniform4f = NULL;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = NULL;
PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation = NULL;
PFNGLVERTEXATTRIB1FPROC glVertexAttrib1f = NULL;
PFNGLVERTEXATTRIB1FVPROC glVertexAttrib1fv = NULL;
PFNGLVERTEXATTRIB2FVPROC glVertexAttrib2fv = NULL;
PFNGLVERTEXATTRIB3FVPROC glVertexAttrib3fv = NULL;
PFNGLVERTEXATTRIB4FVPROC glVertexAttrib4fv = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = NULL;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = NULL;
PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation = NULL;
PFNGLGETACTIVEUNIFORMPROC glGetActiveUniform = NULL;

// Shader
PFNGLCREATESHADERPROC glCreateShader = NULL;
PFNGLDELETESHADERPROC glDeleteShader = NULL;
PFNGLSHADERSOURCEPROC glShaderSource = NULL;
PFNGLCOMPILESHADERPROC glCompileShader = NULL;
PFNGLGETSHADERIVPROC glGetShaderiv = NULL;

// VBO
PFNGLGENBUFFERSPROC glGenBuffers = NULL;
PFNGLBINDBUFFERPROC	glBindBuffer = NULL;
PFNGLBUFFERDATAPROC	glBufferData = NULL;
PFNGLBUFFERSUBDATAPROC  glBufferSubData = NULL;
PFNGLMAPBUFFERPROC      glMapBuffer = NULL;
PFNGLUNMAPBUFFERPROC      glUnmapBuffer = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = NULL;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = NULL;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = NULL;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = NULL;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = NULL;

PFNGLACTIVETEXTUREPROC glActiveTexture = NULL;

PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = NULL;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = NULL;
PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer = NULL;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = NULL;
PFNGLDRAWBUFFERSPROC glDrawBuffers = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = NULL;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = NULL;

PFNWGLSWAPINTERVALEXT wglSwapIntervalEXT = NULL;
PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = NULL;

void OpenGL_Init()
{
    // Program
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
    glDeleteProgram = (PFNGLDELETEPROGRAMPROC)wglGetProcAddress("glDeleteProgram");
    glUseProgram = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
    glDetachShader = (PFNGLDETACHSHADERPROC)wglGetProcAddress("glDetachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)wglGetProcAddress("glGetProgramiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
    glUniform1i = (PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i");
    glUniform1iv = (PFNGLUNIFORM1IVPROC)wglGetProcAddress("glUniform1iv");
    glUniform2iv = (PFNGLUNIFORM2IVPROC)wglGetProcAddress("glUniform2iv");
    glUniform3iv = (PFNGLUNIFORM3IVPROC)wglGetProcAddress("glUniform3iv");
    glUniform4iv = (PFNGLUNIFORM4IVPROC)wglGetProcAddress("glUniform4iv");
    glUniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");
    glUniform1fv = (PFNGLUNIFORM1FVPROC)wglGetProcAddress("glUniform1fv");
    glUniform2fv = (PFNGLUNIFORM2FVPROC)wglGetProcAddress("glUniform2fv");
    glUniform3fv = (PFNGLUNIFORM3FVPROC)wglGetProcAddress("glUniform3fv");
    glUniform4fv = (PFNGLUNIFORM4FVPROC)wglGetProcAddress("glUniform4fv");
    glUniform4f = (PFNGLUNIFORM4FPROC)wglGetProcAddress("glUniform4f");
    glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)wglGetProcAddress("glUniformMatrix4fv");
    glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)wglGetProcAddress("glGetAttribLocation");
    glVertexAttrib1f = (PFNGLVERTEXATTRIB1FPROC)wglGetProcAddress("glVertexAttrib1f");
    glVertexAttrib1fv = (PFNGLVERTEXATTRIB1FVPROC)wglGetProcAddress("glVertexAttrib1fv");
    glVertexAttrib2fv = (PFNGLVERTEXATTRIB2FVPROC)wglGetProcAddress("glVertexAttrib2fv");
    glVertexAttrib3fv = (PFNGLVERTEXATTRIB3FVPROC)wglGetProcAddress("glVertexAttrib3fv");
    glVertexAttrib4fv = (PFNGLVERTEXATTRIB4FVPROC)wglGetProcAddress("glVertexAttrib4fv");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)wglGetProcAddress("glEnableVertexAttribArray");
    glBindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC)wglGetProcAddress("glBindAttribLocation");

    // Shader
    glCreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
    glDeleteShader = (PFNGLDELETESHADERPROC)wglGetProcAddress("glDeleteShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv");

    // VBO
    glGenBuffers = (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)wglGetProcAddress("glBufferData");
    glBufferSubData = (PFNGLBUFFERSUBDATAPROC)wglGetProcAddress("glBufferSubData");
    glMapBuffer = (PFNGLMAPBUFFERPROC)wglGetProcAddress("glMapBuffer");
    glUnmapBuffer = (PFNGLUNMAPBUFFERPROC)wglGetProcAddress("glUnmapBuffer");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)wglGetProcAddress("glVertexAttribPointer");
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)wglGetProcAddress("glDeleteBuffers");
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)wglGetProcAddress("glGenVertexArrays");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)wglGetProcAddress("glBindVertexArray");
    glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)wglGetProcAddress("glDeleteVertexArrays");

    glActiveTexture = (PFNGLACTIVETEXTUREPROC)wglGetProcAddress("glActiveTexture");

    glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)wglGetProcAddress("glGenFramebuffers");
    glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)wglGetProcAddress("glBindFramebuffer");
    glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC)wglGetProcAddress("glBlitFramebuffer");
    glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)wglGetProcAddress("glFramebufferTexture2D");
    glDrawBuffers = (PFNGLDRAWBUFFERSPROC)wglGetProcAddress("glDrawBuffers");
    glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)wglGetProcAddress("glCheckFramebufferStatus");
    glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)wglGetProcAddress("glDeleteFramebuffers");

    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXT)wglGetProcAddress("wglSwapIntervalEXT");
    wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");
}

BOOL OpenGL_ExtExists(char *ext, HDC hdc)
{
    char *glext = (char *)glGetString(GL_EXTENSIONS);
    if (glext)
    {
        if (strstr(glext, ext))
            return TRUE;
    }

    if (wglGetExtensionsStringARB)
    {
        char *wglext = (char *)wglGetExtensionsStringARB(hdc);
        if (wglext)
        {
            if (strstr(wglext, ext))
                return TRUE;
        }
}
    return FALSE;
}

GLuint OpenGL_BuildProgram(const GLchar *vertSource, const GLchar *fragSource)
{
    if (!glCreateShader || !glShaderSource || !glCompileShader || !glCreateProgram ||
        !glAttachShader || !glLinkProgram || !glUseProgram || !glDetachShader)
        return 0;

    GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);

    if (!vertShader || !fragShader)
        return 0;

    glShaderSource(vertShader, 1, &vertSource, NULL);
    glShaderSource(fragShader, 1, &fragSource, NULL);

    GLint isCompiled = 0;

    glCompileShader(vertShader);
    if (glGetShaderiv)
    {
        glGetShaderiv(vertShader, GL_COMPILE_STATUS, &isCompiled);
        if (isCompiled == GL_FALSE)
        {
            if (glDeleteShader)
                glDeleteShader(vertShader);

            return 0;
        }
    }

    glCompileShader(fragShader);
    if (glGetShaderiv)
    {
        glGetShaderiv(fragShader, GL_COMPILE_STATUS, &isCompiled);
        if (isCompiled == GL_FALSE)
        {
            if (glDeleteShader)
            {
                glDeleteShader(fragShader);
                glDeleteShader(vertShader);
            }

            return 0;
        }
    }

    GLuint program = glCreateProgram();
    if (program)
    {
        glAttachShader(program, vertShader);
        glAttachShader(program, fragShader);

        glLinkProgram(program);

        glDetachShader(program, vertShader);
        glDetachShader(program, fragShader);
        glDeleteShader(vertShader);
        glDeleteShader(fragShader);

        if (glGetProgramiv)
        {
            GLint isLinked = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
            if (isLinked == GL_FALSE)
            {
                if (glDeleteProgram)
                    glDeleteProgram(program);

                return 0;
            }
        }
    }

    return program;
}

GLuint OpenGL_BuildProgramFromFile(const char *filePath)
{
    GLuint program = 0;

    FILE *file = fopen(filePath, "rb");
    if (file)
    {
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);

        char *source = calloc(fileSize + 1, 1);
        if (source)
        {
            fread(source, fileSize, 1, file);
            fclose(file);

            char *vertSource = calloc(fileSize + 50, 1);
            char *fragSource = calloc(fileSize + 50, 1);

            if (fragSource && vertSource)
            {
                char *versionStart = strstr(source, "#version");
                if (versionStart)
                {
                    const char deli[2] = "\n";
                    char *version = strtok(versionStart, deli);

                    strcpy(vertSource, source);
                    strcpy(fragSource, source);
                    strcat(vertSource, "\n#define VERTEX\n");
                    strcat(fragSource, "\n#define FRAGMENT\n");
                    strcat(vertSource, version + strlen(version) + 1);
                    strcat(fragSource, version + strlen(version) + 1);

                    program = OpenGL_BuildProgram(vertSource, fragSource);
                }
                else
                {
                    strcpy(vertSource, "#define VERTEX\n");
                    strcpy(fragSource, "#define FRAGMENT\n");
                    strcat(vertSource, source);
                    strcat(fragSource, source);

                    program = OpenGL_BuildProgram(vertSource, fragSource);
                }

                free(vertSource);
                free(fragSource);
            }

            free(source);
        }
    }

    return program;
}

BOOL TextureUploadTest(int width, int height, GLint internalFormat, GLenum format, GLenum type)
{
    BOOL result = TRUE;
    GLenum gle = GL_NO_ERROR;

    static char testData[] = { 0,1,2,0,0,2,3,0,0,4,5,0,0,6,7,0,0,8,9,0 };
    void *textureBuffer = calloc(1, 2 * width * height);

    GLuint texID;

    memcpy(textureBuffer, testData, sizeof(testData));

    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, textureBuffer);
    if ((gle = glGetError()) != GL_NO_ERROR)
    {
        result = FALSE;
        goto finish;
    }

    glFinish();

    memset(textureBuffer, 0, sizeof(testData));

    glGetTexImage(GL_TEXTURE_2D, 0, format, type, textureBuffer);
    if ((gle = glGetError()) != GL_NO_ERROR)
    {
        result = FALSE;
        goto finish;
    }

    glFinish();

    if (memcmp(textureBuffer, testData, sizeof(testData)) != 0)
        result = FALSE;

 finish:
    glDeleteTextures(1, &texID);
    free(textureBuffer);
    return result;
}

#define U565_RED 11
#define U565_GREEN 5
#define U565_BLUE 0
#define RGBA_RED 0
#define RGBA_BLUE 16
#define RGBA_GREEN 8
#define RGBA_ALPHA 24

BOOL ShaderTest(GLuint convProgram, int width, int height, GLint internalFormat, GLenum format, GLenum type)
{
    dprintf("--> ShaderTest(%d, %d, %d, %d, %d, %d)\n", convProgram, width, height, internalFormat, format, type);
    BOOL result = TRUE;
    GLenum gle = GL_NO_ERROR;
    BOOL setupFailed = FALSE;

    uint16_t inTestData[] = { 16 << U565_RED, 32 << U565_GREEN, 16 << U565_BLUE,
        (16 << U565_RED) | (32 << U565_GREEN) | (16 << U565_BLUE) };


    uint32_t outTestData[] = { 128 << RGBA_RED, 128 << RGBA_GREEN, 128 << RGBA_BLUE,
        (128 << RGBA_RED) | (128 << RGBA_GREEN) | (128 << RGBA_BLUE) };

    uint16_t *inBuffer = (uint16_t*)calloc(1, 2 * width * height);
    uint32_t *outBuffer = (uint32_t*)calloc(1, 4 * width * height);

    GLuint texID, fboId, fboTexId;

    gle = glGetError();

    for (int y = 0; y < height; ++y)
    {
        uint16_t *line = inBuffer + y * width;
        for (int x = 0, i = 0; x < width; ++x)
        {
            if (i >= sizeof(inTestData)/sizeof(inTestData[0]))
                i = 0;
            line[x] = inTestData[i++];
        }
    }

    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, inBuffer);
    if ((gle = glGetError()) != GL_NO_ERROR)
    {
        dprintf("glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, inBuffer), %x\n", gle);
        setupFailed = TRUE;
    }

    glGenFramebuffers(1, &fboId);
    glBindFramebuffer(GL_FRAMEBUFFER, fboId);

    glGenTextures(1, &fboTexId);
    glBindTexture(GL_TEXTURE_2D, fboTexId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, outBuffer);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTexId, 0);

    GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBuffers);
    if ((gle = glGetError()) != GL_NO_ERROR)
        dprintf("glDrawBuffers, %x\n", gle);

    if (setupFailed)
    {
        result = FALSE;
        goto finish;
    }

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
    {
        glViewport(0, 0, width, height);

        glUseProgram(convProgram);
        glBindTexture(GL_TEXTURE_2D, texID);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

        glFinish();

        memset(outBuffer, 0, 4 * width * height);

        glBindTexture(GL_TEXTURE_2D, fboTexId);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, outBuffer);

        glFinish();

        int colors[] = { RGBA_RED, RGBA_BLUE, RGBA_GREEN, RGBA_ALPHA };
#ifdef DEBUG
        char *colorNames[] = { "RGBA_RED", "RGBA_BLUE", "RGBA_GREEN", "RGBA_ALPHA" };
#endif
        for (int i = 0; i < sizeof(outTestData)/sizeof(outTestData[0]); ++i)
        {
            bool failed = false;
            for (int c = 0; c < sizeof(colors)/sizeof(colors[0]); ++c)
            {
                if ((int)(outBuffer[i] >> colors[c] & 0xff) > (int)(outTestData[i] >> colors[c] & 0xff) + 5
                    || (int)(outBuffer[i] >> colors[c] & 0xff) < (int)(outTestData[i] >> colors[c] & 0xff) - 5)
                {
                    dprintf("Shader failed at %d with color %s, expected (%d - %d) got %d\n", i, colorNames[c],
                            (outTestData[i] >> colors[c] & 0xff) - 5,
                            (outTestData[i] >> colors[c] & 0xff) + 5,
                            (outBuffer[i] >> colors[c] & 0xff));
                    failed = true;
                }
            }
            if (failed)
            {
                result = FALSE;
                break;
            }
        }
    }
    else
    {
        dprintf("glCheckFramebufferStatus(GL_FRAMEBUFFER) failed\n");
        result = FALSE;
    }

finish:
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fboId);

    glDeleteTextures(1, &texID);
    glDeleteTextures(1, &fboTexId);

    free(inBuffer);
    free(outBuffer);

    dprintf("<-- ShaderTest(%d, %d, %d, %d, %d, %d) %d\n", convProgram, width, height, internalFormat, format, type, result);
    return result;
}
