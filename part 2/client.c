#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define PORT 8080

int main() {
    int client_socket;
    struct sockaddr_in server_address;

    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        return 1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        perror("invalid address");
        return 1;
    }

    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("connection failed");
        return 1;
    }

    printf("Connected to server.\n");

    srand(time(NULL)); // Setting the seed based on current time
    long long number = rand(); // Generating a random number

    printf("Number sent for verification: %lld\n", number);
    write(client_socket, &number, sizeof(number));

    // Read prime numbers list from server
    long long primes[1000];
    if (read(client_socket, primes, sizeof(primes)) < 0) {
        perror("Read failed");
        close(client_socket);
        return 1;
    }


    close(client_socket);
    return 0;
}
