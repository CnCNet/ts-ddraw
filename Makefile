-include config.mk

CC ?= i686-w64-mingw32-gcc
WINDRES ?= windres
STRIP ?= strip
COPY ?= copy

CFLAGS=--std=c99 -Iinc -Wall -Wl,--enable-stdcall-fixup -O6 -g
LIBS=-lgdi32 -lwinmm -lopengl32

FILES = src/main.c \
        src/IDirectDraw.c \
        src/IDirectDrawClipper.c \
        src/IDirectDrawSurface.c \
        src/hook.c \
        src/render.c \
        src/Settings.c \
        src/opengl.c \
        src/counter.c

all: debug

debug:
	$(WINDRES) -J rc ddraw.rc ddraw.rc.o
	$(CC) $(CFLAGS) -D_DEBUG -shared -o ddraw.dll $(FILES) ddraw.rc.o ddraw.def $(LIBS)

release:
	$(WINDRES) -J rc ddraw.rc ddraw.rc.o
	$(CC) $(CFLAGS) -nostdlib -shared -o ddraw.dll $(FILES) ddraw.rc.o ddraw.def $(LIBS) -lkernel32 -luser32 -lmsvcrt
	$(COPY) ddraw.dll ddraw.debug.dll
	$(STRIP) -s ddraw.dll

clean:
	rm -f ddraw.dll ddraw.debug.dll ddraw.rc.o
