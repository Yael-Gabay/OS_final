#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

#define PORT 8080
#define MAX_EVENTS 50
#define BUFFER_SIZE 1024
#define ll long long

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

ll highest_prime = 0;
int requestCounter = 0;
FILE *logFile;
pthread_mutex_t prime_mutex;
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t count_cond = PTHREAD_COND_INITIALIZER;

void proactor_mark_inactive(proactor_t* proactor, int index);

// פונקציה לחישוב (base^exp) % mod
ll powerMod(ll base, ll exp, ll mod) {
    ll result = 1;
    base = base % mod;
    while (exp > 0) {
        if (exp % 2 == 1) // אם exp אי-זוגי
            result = (result * base) % mod;
        exp = exp >> 1; // חלוקה ב-2
        base = (base * base) % mod;
    }
    return result;
}

// פונקציה שעושה את הבדיקה העיקרית של מילר-רבין
bool millerTest(ll d, ll n) {
    ll a = 2 + rand() % (n - 4); // משתנה a: בחירת מספר אקראי בתחום [2, n-2]
    ll x = powerMod(a, d, n);    // משתנה x: חישוב (a^d) % n

    if (x == 1 || x == n - 1)
        return true;

    // חישוב חוזר של x^2 % n
    while (d != n - 1) { // משתנה d מחושב תחילה כ- n - 1 ומחולק ב-2 עד שהוא אי-זוגי
        x = (x * x) % n;
        d *= 2; // s (מספר הצעדים) 

        if (x == 1)
            return false;
        if (x == n - 1)
            return true;
    }
    return false;
}

// פונקציה לבדוק ראשוניות בשיטת מילר-רבין
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
                        proactor_mark_inactive(proactor, i);
                        pthread_t thread;
                        pthread_create(&thread, NULL, event_handler_wrapper, &proactor->sources[i]);
                        pthread_detach(thread);
                    }
                }
            }
        }
    }
}
void proactor_mark_inactive(proactor_t* proactor, int index) {
    pthread_mutex_lock(&proactor->lock);
    
    // Mark the file descriptor as inactive
    proactor->fds[index].fd = -1;

    // Move the last active file descriptor to this position
    int last_active_index = proactor->count - 1;
    proactor->fds[index] = proactor->fds[last_active_index];
    proactor->sources[index] = proactor->sources[last_active_index];
    proactor->count--;

    pthread_mutex_unlock(&proactor->lock);
}



void* client_handler(int fd) {
    char buffer[BUFFER_SIZE];
    int valread;
    while ((valread = read(fd, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[valread] = '\0';
        long long num = atoll(buffer);
        srand(time(0)); // אתחול מחולל המספרים האקראיים
        int prime = isPrime(num, 5);
        pthread_mutex_lock(&prime_mutex);
        requestCounter++;
        if (prime) {
            if (num > highest_prime) highest_prime = num;
            fprintf(logFile, "Request #%d: %lld is prime. Highest prime: %lld\n", requestCounter, num, highest_prime);
        } else {
            fprintf(logFile, "Request #%d: %lld is not prime.\n", requestCounter, num);
        }
        fflush(logFile);

        sprintf(buffer, "Request #%d: %lld is %sprime. Highest prime so far: %lld\n", requestCounter, num, prime ? "" : "not ", highest_prime);
        send(fd, buffer, strlen(buffer), 0);

        if (requestCounter % 100 == 0) {
            pthread_cond_signal(&count_cond);
        }
        pthread_mutex_unlock(&prime_mutex);
    }

    if (valread <= 0) {
        printf("Client disconnected: socket fd %d\n", fd);
        close(fd);
    }
    return NULL;
}

void* reporter_thread(void* arg) {
    while (1) {
        pthread_mutex_lock(&count_mutex);
        pthread_cond_wait(&count_cond, &count_mutex);
        pthread_mutex_lock(&prime_mutex);
        printf("Reporter: The highest prime number found so far is %lld after %d requests\n", highest_prime, requestCounter);
        fprintf(logFile, "Reporter: The highest prime number found so far is %lld after %d requests\n", highest_prime, requestCounter);
        fflush(logFile);
        pthread_mutex_unlock(&prime_mutex);
        pthread_mutex_unlock(&count_mutex);
    }
    return NULL;
}

int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    logFile = fopen("server_log.txt", "w");
    if (!logFile) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }

    // אתחול המנעול
    pthread_mutex_init(&prime_mutex, NULL);

    pthread_t reporter;
    pthread_create(&reporter, NULL, reporter_thread, NULL);

    proactor_t* proactor = proactor_init();
    proactor_add_fd(proactor, listen_fd, client_handler);
    proactor_run(proactor);

    fclose(logFile);
    pthread_mutex_destroy(&prime_mutex); // השמדת המנעול
    pthread_cancel(reporter);
    pthread_join(reporter, NULL);
    return 0;
}