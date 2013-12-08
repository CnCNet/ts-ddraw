CC=i686-w64-mingw32-gcc
WINDRES=i686-w64-mingw32-windres
STRIP=i686-w64-mingw32-strip
CFLAGS=--std=c99 -Wall -Wl,--enable-stdcall-fixup -O6
LIBS=-lgdi32 -lwinmm
REV=$(shell sh -c 'git rev-parse --short @{0}')

all: debug

debug:
	sed 's/__REV__/$(REV)+DEBUG/g' ddraw.rc.in > ddraw.rc
	$(WINDRES) -J rc ddraw.rc ddraw.rc.o
	$(CC) $(CFLAGS) -g -D_DEBUG -shared -o ddraw.dll src/main.c src/IDirectDraw.c src/IDirectDrawClipper.c src/IDirectDrawSurface.c ddraw.def ddraw.rc.o $(LIBS)

release:
	sed 's/__REV__/$(REV)/g' ddraw.rc.in > ddraw.rc
	$(WINDRES) -J rc ddraw.rc ddraw.rc.o
	$(CC) $(CFLAGS) -shared -o ddraw.dll src/main.c src/IDirectDraw.c src/IDirectDrawClipper.c src/IDirectDrawSurface.c ddraw.def ddraw.rc.o $(LIBS)
	$(STRIP) -s ddraw.dll

clean:
	rm -f ddraw.dll ddraw.rc ddraw.rc.o
