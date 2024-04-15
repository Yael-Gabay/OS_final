#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080

// Rabin-Miller primality test function
bool isPrime(long long n, int iterations) {
    if (n <= 1 || n == 4)
        return false;
    if (n <= 3)
        return true;

    while (iterations > 0) {
        long long a = 2 + (long long)rand() % (n - 4);
        long long x = a % n;
        long long y = n - 1;
        long long result = 1;
        while (y > 0) {
            if (y & 1)
                result = (result * x) % n;
            y >>= 1;
            x = (x * x) % n;
        }
        if (result != 1)
            return false;
        iterations--;
    }
    return true;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Initialize list of prime numbers
    long long primes[1000];
    int primes_count = 0;

    printf("Server listening on port %d\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        printf("New client connected.\n");

        long long number;
        read(new_socket, &number, sizeof(number));

        bool is_prime = isPrime(number, 5);

        if (is_prime) {
            printf("%lld is probably prime.\n", number);
            primes[primes_count++] = number;
            write(new_socket, &number, sizeof(number));
        } else {
            printf("%lld is composite.\n", number);
        }

        // Send list of prime numbers to client
        if (send(new_socket, primes, sizeof(primes), 0) < 0) {
            perror("Send failed");
            close(new_socket);
            continue;
        }

        // Print list of prime numbers
        printf("Prime numbers found so far: ");
        for (int i = 0; i < primes_count; i++) {
            printf("%lld ", primes[i]);
        }
        printf("\n\n");
    }

    close(server_fd);
    return 0;
}
