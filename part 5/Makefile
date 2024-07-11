CC = gcc
CFLAGS = -Wall

all: server5 client5

server5: server_level5.c
	$(CC) $(CFLAGS) -o server5 server_level5.c -lpthread

client5: client_level5.c
	$(CC) $(CFLAGS) -o client5 client_level5.c

clean:
	rm -f server5 client5
