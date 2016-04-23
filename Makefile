#**************************************************
# Makefile for Threaded List lab
#
# Author: Philip Howard
# Email:  phil.howard@oit.edu
# Date:   Feb. 20, 2016
#
COPTS = -g -O0 -Wall -pthread -std=gnu99

OBJS = list.o \
       sequence.o \
       main.o \

all: threads

clean:
	rm -f threads
	rm -f *.o

.c.o:
	gcc $(COPTS) -c $? -o $@

threads: $(OBJS)
	gcc $(COPTS) $(OBJS) -o threads

