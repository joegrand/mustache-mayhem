CC=g++
CFLAGS=`pkg-config --static --libs opencv cairo freetype2 ncurses` `sdl-config --cflags --libs` -lSDL_mixer -lsoc -I/usr/include/freetype2 -O3

all: stache

stache: main.cpp
	$(CC) $(CFLAGS) -o stache main.cpp

clean:
	rm -f stache

install: stache
	cp stache.desktop $(HOME)/Desktop/
