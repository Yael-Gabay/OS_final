#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <sys/shm.h>
#include <errno.h>
#include <pthread.h>

#define PORT 8080
#define MAX_CLIENTS 200
#define BUFFER_SIZE 1024

// Shared memory structure
struct shared_memory {
    long long highest_prime;
    int request_counter;
    pthread_mutex_t lock;
};

// Function to check primality
int is_prime(long long num) {
    if (num <= 1) return 0;
    if (num % 2 == 0 && num > 2) return 0;
    for (long long i = 3; i * i <= num; i += 2) {
        if (num % i == 0) return 0;
    }
    return 1;
}

void client_handler(int client_fd, struct shared_memory *shared_memory_ptr) {
    char buffer[BUFFER_SIZE];
    int valread = read(client_fd, buffer, BUFFER_SIZE - 1);
    if (valread > 0) {
        buffer[valread] = '\0';
        long long num = atoll(buffer);
        int prime = is_prime(num);

        pthread_mutex_lock(&shared_memory_ptr->lock);
        shared_memory_ptr->request_counter++;
        if (prime) {
            if (num > shared_memory_ptr->highest_prime) 
                shared_memory_ptr->highest_prime = num;
            printf("Request #%d: %lld is prime. Highest prime: %lld\n", shared_memory_ptr->request_counter, num, shared_memory_ptr->highest_prime);
        } else {
            printf("Request #%d: %lld is not prime.\n", shared_memory_ptr->request_counter, num);
        }
        fflush(stdout);

        sprintf(buffer, "Request #%d: %lld is %sprime. Highest prime so far: %lld\n", shared_memory_ptr->request_counter, num, prime ? "" : "not ", shared_memory_ptr->highest_prime);
        send(client_fd, buffer, strlen(buffer), 0);
        pthread_mutex_unlock(&shared_memory_ptr->lock);

        // Close the client socket
        close(client_fd);
    } 
printf("\n");

    if (valread <= 0) {
        printf("Client disconnected: socket fd %d\n", client_fd);
        close(client_fd);
    }
}

int main() {
    int server_fd, new_socket, addrlen;
    struct sockaddr_in address;
    struct pollfd fds[MAX_CLIENTS];

    // Creating shared memory segment
    int shmid = shmget(IPC_PRIVATE, sizeof(struct shared_memory), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    struct shared_memory *shared_memory_ptr = (struct shared_memory *)shmat(shmid, NULL, 0);
    if (shared_memory_ptr == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    shared_memory_ptr->highest_prime = 0;
    shared_memory_ptr->request_counter = 0;
    pthread_mutex_init(&shared_memory_ptr->lock, NULL);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
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
    memset(fds + 1, 0 , sizeof(struct pollfd) * (MAX_CLIENTS - 1));

    printf("The server is waiting for connections...\n");

    while (1) {
        int activity = poll(fds, MAX_CLIENTS, -1);
        if (activity < 0) {
            perror("poll");
            continue;
        }

        // Accept new connections
        if (fds[0].revents & POLLIN) {
            addrlen = sizeof(address);
            new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            if (new_socket < 0) {
                perror("accept");
                continue;
            }
            printf("Client connected: socket fd is %d, ip is: %s, port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            // Fork a new process to handle the client
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
            } else if (pid == 0) {
                // Child process
                close(server_fd); // Close the listening socket in the child process
                client_handler(new_socket, shared_memory_ptr);
                exit(EXIT_SUCCESS); // Exit the child process after handling the client
            } else {
                // Parent process
                close(new_socket); // Close the new socket in the parent process
            }
        }
    }

    // Detach shared memory
    shmdt(shared_memory_ptr);
    // Remove shared memory segment
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}
