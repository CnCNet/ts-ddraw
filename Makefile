-include config.mk

CC ?= i686-w64-mingw32-gcc
WINDRES ?= windres

CFLAGS=--std=c99 -Wall -Wl,--enable-stdcall-fixup -O6 -s
LIBS=-lgdi32 -lwinmm

all: debug

debug:
	$(WINDRES) -J rc ddraw.rc ddraw.rc.o
	$(CC) $(CFLAGS) -g -D_DEBUG -shared -o ddraw.dll src/main.c src/IDirectDraw.c src/IDirectDrawClipper.c src/IDirectDrawSurface.c ddraw.rc.o ddraw.def $(LIBS)

release:
	$(WINDRES) -J rc ddraw.rc ddraw.rc.o
	$(CC) $(CFLAGS) -nostdlib -shared -o ddraw.dll src/main.c src/IDirectDraw.c src/IDirectDrawClipper.c src/IDirectDrawSurface.c ddraw.rc.o ddraw.def $(LIBS) -lkernel32 -luser32 -lmsvcrt

clean:
	rm -f ddraw.dll ddraw.rc.o
