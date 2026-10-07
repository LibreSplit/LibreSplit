CC = gcc
CFLAGS = -Wall -fPIC -I../../include $(shell pkg-config --cflags gtk4)
LIBS = $(shell pkg-config --libs gtk4)

all:
	$(CC) -shared $(CFLAGS) -o test_plugin.so test_plugin.c $(LIBS)
