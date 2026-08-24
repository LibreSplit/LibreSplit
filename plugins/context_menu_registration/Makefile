CC = gcc
CFLAGS = -Wall -fPIC -I../../include $(shell pkg-config --cflags gtk+-3.0)
LIBS = $(shell pkg-config --libs gtk+-3.0)

all:
	$(CC) -shared $(CFLAGS) -o test_plugin4.so test_plugin.c $(LIBS)
