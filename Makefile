#**************************************************
# Makefile for threaded chat server lab
# Erik Andersen
#
GIT_VERSION := $(shell git describe --abbrev=16 --dirty="-uncommitted" --always --tags)
CFLAGS=-Wall -Wshadow -Wunreachable-code -Wredundant-decls -DGIT_VERSION=\"$(GIT_VERSION)\" -std=gnu99 -g -O0
CC = gcc

OBJS = list.o \

all: client server

clean:
	rm -f server
	rm -f client
	rm -f *.o

.c.o:
	$(CC) $(COPTS) -c $? -o $@

server: $(OBJS)
	$(CC) $(COPTS) $(OBJS) -o server

client: $(OBJS)
	$(CC) $(COPTS) $(OBJS) -o client

