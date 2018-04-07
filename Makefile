CC=i686-w64-mingw32-gcc
WINDRES=i686-w64-mingw32-windres
STRIP=i686-w64-mingw32-strip
CFLAGS=--std=c99 -Wall -Wl,--enable-stdcall-fixup -O6 -s
LIBS=-lgdi32 -lwinmm
REV=$(shell sh -c 'git rev-parse --short @{0}')

all: debug

debug:
	$(CC) $(CFLAGS) -g -D_DEBUG -shared -o ddraw.dll src/main.c src/IDirectDraw.c src/IDirectDrawClipper.c src/IDirectDrawSurface.c ddraw.def $(LIBS)

release:
	$(CC) $(CFLAGS) -nostdlib -shared -o ddraw.dll src/main.c src/IDirectDraw.c src/IDirectDrawClipper.c src/IDirectDrawSurface.c ddraw.def $(LIBS) -lkernel32 -luser32 -lmsvcrt

clean:
	rm -f ddraw.dll ddraw.rc ddraw.rc.o
