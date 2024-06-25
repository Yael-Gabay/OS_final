#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8080
#define MAX_CLIENTS 200
#define BUFFER_SIZE 1024
#define ll long long

// Function to calculate (base^exp) % mod
ll powerMod(ll base, ll exp, ll mod) {
    ll result = 1;
    base = base % mod;
    while (exp > 0) {
        if (exp % 2 == 1)
            result = (result * base) % mod;
        exp = exp >> 1;
        base = (base * base) % mod;
    }
    return result;
}

// Function to perform Miller-Rabin primality test
bool millerTest(ll d, ll n) {
    ll a = 2 + rand() % (n - 4);
    ll x = powerMod(a, d, n);

    if (x == 1 || x == n - 1)
        return true;

    while (d != n - 1) {
        x = (x * x) % n;
        d *= 2;

        if (x == 1)
            return false;
        if (x == n - 1)
            return true;
    }
    return false;
}

// Function to check primality using Miller-Rabin method
bool isPrime(ll n, int k) {
    if (n <= 1 || n == 4)
        return false;
    if (n <= 3)
        return true;

    ll d = n - 1;
    while (d % 2 == 0)
        d /= 2;

    for (int i = 0; i < k; i++)
        if (!millerTest(d, n))
            return false;

    return true;
}

// Structure for shared memory
typedef struct {
    ll highest_prime;
    int request_counter;
    pthread_mutex_t lock;
} shared_data_t;

int main() {
    int server_fd, new_socket, addrlen;
    struct sockaddr_in address;
    struct pollfd fds[MAX_CLIENTS];
    char buffer[BUFFER_SIZE];

    // Create shared memory
    int shm_fd;
    shared_data_t *shared_memory;
    const char *shm_name = "/prime_shared_memory";

    shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shm_fd, sizeof(shared_data_t)) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    shared_memory = (shared_data_t *) mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_memory == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // Initialize shared memory
    shared_memory->highest_prime = 0;
    shared_memory->request_counter = 0;
    pthread_mutex_init(&shared_memory->lock, NULL);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    memset(fds + 1, 0, sizeof(struct pollfd) * (MAX_CLIENTS - 1));

    printf("The server is waiting for connections on port %d...\n", PORT);

    while (1) {
        int activity = poll(fds, MAX_CLIENTS, -1);
        if (activity < 0) {
            perror("poll");
            continue;
        }

        // Accept new connections
        if (fds[0].revents & POLLIN) {
            addrlen = sizeof(address);
            new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
            if (new_socket < 0) {
                perror("accept");
                continue;
            }

            printf("New connection: socket fd is %d, ip is: %s, port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            // Fork a new process for each client
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                continue;
            } else if (pid == 0) {
                // Child process handles client request
                close(server_fd); // Child process doesn't need the listener socket

                while (1) {
                    int valread = read(new_socket, buffer, BUFFER_SIZE - 1);
                    if (valread > 0) {
                        buffer[valread] = '\0';
                        ll num = atoll(buffer);
                        int prime = isPrime(num, 5);
                        pthread_mutex_lock(&shared_memory->lock);
                        shared_memory->request_counter++;

                        if (prime) {
                            if (num > shared_memory->highest_prime)
                                shared_memory->highest_prime = num;
                            printf("Request #%d: %lld is prime. Highest prime: %lld\n", shared_memory->request_counter, num, shared_memory->highest_prime);
                        } else {
                            printf("Request #%d: %lld is not prime.\n", shared_memory->request_counter, num);
                        }

                        // Check if we've processed 10 requests
                        if (shared_memory->request_counter % 10 == 0) {
                            printf("Reporting highest prime number detected so far: %lld\n", shared_memory->highest_prime);
                        }

                        pthread_mutex_unlock(&shared_memory->lock);

                        sprintf(buffer, "Request #%d: %lld is %sprime. Highest prime so far: %lld\n", shared_memory->request_counter, num, prime ? "" : "not ", shared_memory->highest_prime);
                        send(new_socket, buffer, strlen(buffer), 0);
                    }

                    if (valread <= 0) {
                        printf("Client disconnected: socket fd %d\n", new_socket);
                        close(new_socket);
                        exit(EXIT_SUCCESS); // Exit child process upon client disconnection
                    }
                }
            } else {
                // Parent process continues to accept new connections
                close(new_socket); // Parent process doesn't need this client socket
            }
        }
    }

    close(server_fd);
    return 0;
}
