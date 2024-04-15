#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define MAX_EVENTS 50
#define PORT 8080

typedef void* (*handler_t)(int fd);

typedef struct {
    int fd;
    handler_t handler;
    pthread_mutex_t fd_lock; // Mutex to synchronize access to fd
} event_source_t;

typedef struct {
    struct pollfd fds[MAX_EVENTS];
    event_source_t sources[MAX_EVENTS];
    int count;
    pthread_mutex_t lock;
} proactor_t;

proactor_t* proactor_init() {
    proactor_t* proactor = malloc(sizeof(proactor_t));
    if (!proactor) return NULL;
    memset(proactor, 0, sizeof(proactor_t));
    pthread_mutex_init(&proactor->lock, NULL);
    return proactor;
}

void* event_handler_wrapper(void* arg) {
    event_source_t* source = (event_source_t*)arg;
    pthread_mutex_lock(&source->fd_lock);
    if (source->fd != -1) {
        source->handler(source->fd);
        close(source->fd); // Close fd and mark as closed
        source->fd = -1;
    }
    pthread_mutex_unlock(&source->fd_lock);
    return NULL;
}

void proactor_add_fd(proactor_t* proactor, int fd, handler_t handler) {
    pthread_mutex_lock(&proactor->lock);
    if (proactor->count < MAX_EVENTS) {
        proactor->fds[proactor->count].fd = fd;
        proactor->fds[proactor->count].events = POLLIN;
        proactor->sources[proactor->count].fd = fd;
        proactor->sources[proactor->count].handler = handler;
        pthread_mutex_init(&proactor->sources[proactor->count].fd_lock, NULL);
        proactor->count++;
    }
    pthread_mutex_unlock(&proactor->lock);
}

void proactor_run(proactor_t* proactor) {
    while (1) {
        int ret = poll(proactor->fds, proactor->count, -1);
        if (ret > 0) {
            for (int i = 0; i < proactor->count; i++) {
                if (proactor->fds[i].revents & POLLIN) {
                    if (proactor->fds[i].fd == proactor->sources[0].fd) {  // Listening socket
                        struct sockaddr_in client_addr;
                        socklen_t client_len = sizeof(client_addr);
                        int client_fd = accept(proactor->fds[i].fd, (struct sockaddr *)&client_addr, &client_len);
                        if (client_fd > 0) {
                            proactor_add_fd(proactor, client_fd, proactor->sources[0].handler);
                        }
                    } else {
                        pthread_t thread;
                        pthread_create(&thread, NULL, event_handler_wrapper, &proactor->sources[i]);
                        pthread_detach(thread);
                    }
                }
            }
        }
    }
}

void* simple_handler(int fd) {
    char buffer[1024];
    int read_bytes = read(fd, buffer, sizeof(buffer) - 1);
    if (read_bytes > 0) {
        buffer[read_bytes] = '\0';
        printf("Received data: %s\n", buffer);
    } else if (read_bytes == 0) {
        printf("Client disconnected.\n");
    } else {
        perror("Read failed");
    }
    return NULL;
}

int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, 5);

    proactor_t* proactor = proactor_init();
    proactor_add_fd(proactor, listen_fd, simple_handler);
    proactor_run(proactor);

    return 0;
}
