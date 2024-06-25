CC = gcc
CFLAGS = -Wall

all: server3 client3

server3: server_level3.c
	$(CC) $(CFLAGS) -o server3 server_level3.c

client3: client_level3.c
	$(CC) $(CFLAGS) -o client3 client_level3.c

clean:
	rm -f server3 client3
