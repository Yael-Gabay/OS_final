CC = gcc
CFLAGS = -Wall

all: server5 client5

server5: server_level5.o
	$(CC) $(CFLAGS) -o server5 server_level5.o

server_level5.o: server_level5.c
	$(CC) $(CFLAGS) -c server_level5.c

client5: client_level5.o
	$(CC) $(CFLAGS) -o client5 client_level5.o

client_level5.o: client_level5.c
	$(CC) $(CFLAGS) -c client_level5.c

clean:
	rm -f server5 client5 *.o
