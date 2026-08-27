CC = gcc
CFLAGS = -lws2_32 -Wall

all: exec/lamma.exe
	$(CC) src/lamma.c -o exec/lamma.exe $(CFLAGS)
