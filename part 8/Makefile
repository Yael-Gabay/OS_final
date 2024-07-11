CC = gcc
CFLAGS = -Wall

all: server8 client8

server8: server_level8.c
	$(CC) $(CFLAGS) -o server8 server_level8.c -lpthread

client8: client_level8.c
	$(CC) $(CFLAGS) -o client8 client_level8.c

clean:
	rm -f server8 client8
