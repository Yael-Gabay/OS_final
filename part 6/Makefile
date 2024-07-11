CC = gcc
CFLAGS = -Wall

all: server6 client6

server6: server_level6.c
	$(CC) $(CFLAGS) -o server6 server_level6.c -lpthread

client6: client_level6.c
	$(CC) $(CFLAGS) -o client6 client_level6.c

clean:
	rm -f server6 client6
