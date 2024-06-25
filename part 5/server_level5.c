#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define ll long long

// Function to calculate (base^exp) % mod
ll powerMod(ll base, ll exp, ll mod) {
    ll result = 1;
    base = base % mod;
    while (exp > 0) {
        if (exp % 2 == 1) // if exp is odd
            result = (result * base) % mod;
        exp = exp >> 1; // divide exp by 2
        base = (base * base) % mod;
    }
    return result;
}

// Function to perform the core Miller-Rabin test
bool millerTest(ll d, ll n) {
    ll a = 2 + rand() % (n - 4); // random number in [2, n-2]
    ll x = powerMod(a, d, n);    // Compute (a^d) % n

    if (x == 1 || x == n - 1)
        return true;

    // Repeat squaring x^2 % n
    while (d != n - 1) {
        x = (x * x) % n;
        d *= 2; // s (number of steps)

        if (x == 1)
            return false;
        if (x == n - 1)
            return true;
    }
    return false;
}

// Function to check primality using Miller-Rabin test
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

typedef struct {
    int fd;
    pthread_mutex_t lock;
    ll highest_prime;
} client_info_t;

pthread_mutex_t prime_mutex = PTHREAD_MUTEX_INITIALIZER;
int requestCounter = 0;
ll highest_prime = 0;

void* client_handler(void* arg) {
    client_info_t* client_info = (client_info_t*)arg;
    int fd = client_info->fd;
    ll* highest_prime_ptr = &(client_info->highest_prime);

    char buffer[BUFFER_SIZE];
    int valread;

    while (1) {
        valread = read(fd, buffer, BUFFER_SIZE - 1);
        if (valread > 0) {
            buffer[valread] = '\0';
            ll num = atoll(buffer);
            srand(time(0)); // Initialize random number generator
            int prime = isPrime(num, 5);
            
            pthread_mutex_lock(&prime_mutex);
            requestCounter++;
            if (prime) {
                if (num > highest_prime) {
                    highest_prime = num;
                    printf("Request #%d: %lld is prime. Highest prime: %lld\n", requestCounter, num, highest_prime);
                }
                *highest_prime_ptr = highest_prime;
            } else {
                printf("Request #%d: %lld is not prime.\n", requestCounter, num);
            }

            sprintf(buffer, "Request #%d: %lld is %sprime. Highest prime so far: %lld\n", requestCounter, num, prime ? "" : "not ", highest_prime);
            send(fd, buffer, strlen(buffer), 0);
            pthread_mutex_unlock(&prime_mutex);
        } 
        if (valread <= 0) {
            printf("Client disconnected: socket fd %d\n", fd);
            close(fd);
            break;
        }
    }

    pthread_mutex_lock(&client_info->lock);
    client_info->fd = -1;
    pthread_mutex_unlock(&client_info->lock);

    free(client_info);
    return NULL;
}

int main() {
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
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

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("The server is waiting for connections on port %d...\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        printf("New connection: socket fd is %d, ip is: %s, port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        // Handle the new client in a new thread
        pthread_t thread;
        client_info_t* client_info = (client_info_t*)malloc(sizeof(client_info_t));
        if (!client_info) {
            perror("Failed to allocate memory for client_info");
            close(new_socket);
            continue;
        }

        client_info->fd = new_socket;
        client_info->highest_prime = 0;
        pthread_mutex_init(&client_info->lock, NULL);

        if (pthread_create(&thread, NULL, client_handler, (void*)client_info) != 0) {
            perror("Failed to create thread for client");
            close(new_socket);
            free(client_info);
            continue;
        }

        pthread_detach(thread);
    }

    return 0;
}
