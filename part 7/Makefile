CC = gcc
CFLAGS = -Wall

all: server7 client7

server7: server_level7.c
	$(CC) $(CFLAGS) -o server7 server_level7.c

client7: client_level7.c
	$(CC) $(CFLAGS) -o client7 client_level7.c

clean:
	rm -f server7 client7
