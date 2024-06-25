#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <pthread.h>

#define PORT 8080
#define MAX_CLIENTS 200
#define BUFFER_SIZE 1024
#define ll long long

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

ll getHighestPrime(ll *shared_memory) {
    return *shared_memory;
}

void updateHighestPrime(ll *shared_memory, ll num) {
    *shared_memory = num;
}

void handle_client(int client_socket, ll *shared_memory, int *request_count) {
    char buffer[BUFFER_SIZE];
    int valread;
    ll highest_prime = getHighestPrime(shared_memory);

    while (1) {
        valread = read(client_socket, buffer, BUFFER_SIZE - 1);
        if (valread > 0) {
            buffer[valread] = '\0';
            ll num = atoll(buffer);

            srand(time(0));
            int prime = isPrime(num, 5);
            (*request_count)++;

            if (prime) {
                if (num > highest_prime) {
                    highest_prime = num;
                    updateHighestPrime(shared_memory, num);
                }
                printf("Request #%d: %lld is prime.\n", *request_count, num);
            } else {
                printf("Request #%d: %lld is not prime.\n", *request_count, num);
            }

            sprintf(buffer, "Request #%d: %lld is %sprime.\n", *request_count, num, prime ? "" : "not ");
            send(client_socket, buffer, strlen(buffer), 0);

            // Check if we've processed 100 requests or a multiple of 100
            if (*request_count % 100 == 0 || *request_count == 100) {
                printf("Report: Process detected %d numbers checked. The highest prime number so far is: %lld\n", *request_count, highest_prime);
            }
        }
        if (valread <= 0) {
            printf("Client disconnected: socket fd %d\n", client_socket);
            close(client_socket);
            break;
        }
    }
}

int main() {
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int shmid;
    ll *shared_memory;
    int request_count = 0;

    shmid = shmget(IPC_PRIVATE, sizeof(ll), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    shared_memory = (ll *)shmat(shmid, NULL, 0);
    if (shared_memory == (ll *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    *shared_memory = 0;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        printf("New connection: socket fd is %d, ip is: %s, port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(new_socket);
            continue;
        }

        if (pid == 0) {
            close(server_fd);
            handle_client(new_socket, shared_memory, &request_count);
            exit(0);
        } else {
            close(new_socket);
        }
    }

    shmdt(shared_memory);

    return 0;
}
