CC=i686-w64-mingw32-gcc
WINDRES=i686-w64-mingw32-windres
CFLAGS=--std=c99 -Wall -Wl,--enable-stdcall-fixup -D_DEBUG
LIBS=-lgdi32

all:
	$(CC) $(CFLAGS) -shared -o ddraw.dll src/main.c src/IDirectDraw.c src/IDirectDrawClipper.c ddraw.def $(LIBS)

clean:
	rm -f ddraw.dll
