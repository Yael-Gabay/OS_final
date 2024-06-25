#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define PORT 8080
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

        srand(time(0)); // אתחול מחולל המספרים האקראיים

        bool is_prime = isPrime(number, 5);

        if (is_prime) {
            printf("%lld is probably prime.\n", number);
            primes[primes_count++] = number;
            write(new_socket, &number, sizeof(number));
        } else {
            printf("%lld is composite.\n", number);
        }

        // Send list of prime numbers to client
        if (send(new_socket, primes, primes_count * sizeof(long long), 0) < 0) {
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

        close(new_socket);
    }

    close(server_fd);
    return 0;
}
