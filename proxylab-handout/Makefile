CC = gcc
CFLAGS = -Wall -g 
LDFLAGS = -lpthread

OBJS = proxy.o csapp.o

all: proxy proxy.c

proxy: $(OBJS)
	gcc $(CFLAGS) proxy.o csapp.o -o proxy $(LDFLAGS)
	
clean:
	rm -f proxy proxy.log *.o *~


