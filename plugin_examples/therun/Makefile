CC = gcc
CFLAGS = -Wall -fPIC -I ../../include $(shell pkg-config --cflags gtk4) $(shell pkg-config --cflags luajit) $(shell pkg-config --cflags libcurl) $(shell pkg-config --libs libcurl)
LIBS = $(shell pkg-config --libs gtk4)

all:
	$(CC) -shared $(CFLAGS) -o therun_plugin.so therun.c $(LIBS)
