#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 8080
#define MAX_CLIENTS 200
#define BUFFER_SIZE 1024
#define ll long long

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

int main() {
    int server_fd, new_socket, valread, addrlen;
    struct sockaddr_in address;
    struct pollfd fds[MAX_CLIENTS];
    char buffer[BUFFER_SIZE];
    FILE *logFile;
    long long highest_prime = 0;
    int requestCounter = 0, clientCounter = 0;

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

    logFile = fopen("server_log.txt", "w");
    if (!logFile) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }

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
            clientCounter++;
            printf("Client number %d connected: socket fd is %d, ip is: %s, port: %d\n", clientCounter, new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            for (int i = 1; i < MAX_CLIENTS; i++) {
                if (fds[i].fd == 0) {
                    fds[i].fd = new_socket;
                    fds[i].events = POLLIN;
                    break;
                }
            }
        }

        // Handle data from clients
        for (int i = 1; i < MAX_CLIENTS; i++) {
            if (fds[i].fd > 0 && (fds[i].revents & POLLIN)) {
                valread = read(fds[i].fd, buffer, BUFFER_SIZE - 1);
                if (valread > 0) {
                    buffer[valread] = '\0';
                    long long num = atoll(buffer);
                    srand(time(0)); // אתחול מחולל המספרים האקראיים
                    int prime = isPrime(num, 5);
                    requestCounter++;
                    if (prime) {
                        if (num > highest_prime) highest_prime = num;
                        fprintf(logFile, "Request #%d: %lld is prime. Highest prime: %lld\n", requestCounter, num, highest_prime);
                    } else {
                        fprintf(logFile, "Request #%d: %lld is not prime.\n", requestCounter, num);
                    }
                    fflush(logFile);

                    sprintf(buffer, "Request #%d: %lld is %sprime. Highest prime so far: %lld\n", requestCounter, num, prime ? "" : "not ", highest_prime);
                    send(fds[i].fd, buffer, strlen(buffer), 0);
                } 
                if (valread <= 0) {
                    printf("Client number %d disconnected: socket fd %d\n", clientCounter, fds[i].fd);
                    close(fds[i].fd);
                    fds[i].fd = 0; // Mark as available
                }
            }
        }
    }

    fclose(logFile);
    return 0;
}
