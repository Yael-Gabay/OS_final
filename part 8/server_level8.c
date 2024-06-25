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
#include <semaphore.h>
#include <fcntl.h>

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

// נתינת המספר הראשון הגבוה ביותר
ll getHighestPrime(ll *shared_memory) {
    return shared_memory[0];
}

// עדכון המספר הראשוני הגבוה ביותר
void updateHighestPrime(ll *shared_memory, ll num) {
    shared_memory[0] = num;
}

// עדכון המונה
void updateCounter(ll *shared_memory, int count) {
    shared_memory[1] = count;
}

// נתינת המונה
int getCounter(ll *shared_memory) {
    return shared_memory[1];
}

// פונקציה לטיפול בלקוח חדש בתהליך ילד
void handle_client(int client_socket, ll *shared_memory, sem_t *sem) {
    char buffer[BUFFER_SIZE];
    int valread;
    int requestCounter = 0;

    while (1) {
        valread = read(client_socket, buffer, BUFFER_SIZE - 1);
        if (valread > 0) {
            buffer[valread] = '\0';
            ll num = atoll(buffer);

            // בדיקת ראשוניות ועדכון המספר הראשוני הגבוה ביותר
            srand(time(0));
            int prime = isPrime(num, 5);
            
            // נעילה לגישה בטוחה לזיכרון המשותף
            sem_wait(sem);

            requestCounter = getCounter(shared_memory) + 1;
            updateCounter(shared_memory, requestCounter);
            
            ll highest_prime = getHighestPrime(shared_memory);
            if (prime) {
                if (num > highest_prime) {
                    highest_prime = num;
                    updateHighestPrime(shared_memory, num);
                }
                printf("Request #%d: %lld is prime. Highest prime: %lld\n", requestCounter, num, highest_prime);
            } else {
                printf("Request #%d: %lld is not prime.\n", requestCounter, num);
            }

            // שחרור הנעילה
            sem_post(sem);

            sprintf(buffer, "Request #%d: %lld is %sprime. Highest prime so far: %lld\n", requestCounter, num, prime ? "" : "not ", highest_prime);
            send(client_socket, buffer, strlen(buffer), 0);
        } 
        if (valread <= 0) {
            printf("Client disconnected: socket fd %d\n", client_socket);
            close(client_socket);
            break;
        }
    }
}

// פונקציה לתהליך המעקב
void monitor_process(ll *shared_memory, sem_t *sem) {
    while (1) {
        sleep(1); // המתנה לשנייה

        // נעילה לגישה בטוחה לזיכרון המשותף
        sem_wait(sem);

        int count = getCounter(shared_memory);
        if (count % 10 == 0 && count != 0) {
            ll highest_prime = getHighestPrime(shared_memory);
            printf("Monitor: Checked %d numbers. Highest prime so far: %lld\n", count, highest_prime);
        }

        // שחרור הנעילה
        sem_post(sem);
    }
}

int main() {
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int shmid;
    ll *shared_memory;
    sem_t *sem;

    // יצירת זיכרון משותף
    shmid = shmget(IPC_PRIVATE, 2 * sizeof(ll), IPC_CREAT | 0666); // 2 מקומות: אחד למספר ואחד למונה
    if (shmid < 0) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    shared_memory = (ll *)shmat(shmid, NULL, 0);
    if (shared_memory == (ll *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    shared_memory[0] = 0; // אתחול המספר הראשוני הגבוה ביותר ל-0
    shared_memory[1] = 0; // אתחול המונה ל-0

    // יצירת semaphore
    sem = sem_open("/sem_high_prime", O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    // יצירת כתובת וחיבור לשרת
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

    printf("The server is waiting for connections on port %d...\n", PORT);

    // יצירת תהליך המעקב
    pid_t monitor_pid = fork();
    if (monitor_pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (monitor_pid == 0) {
        // תהליך המעקב
        monitor_process(shared_memory, sem);
        exit(EXIT_SUCCESS);
    }

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        printf("New connection: socket fd is %d, ip is: %s, port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        // יצירת תהליך ילד עבור לקוח חדש
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(new_socket);
            continue;
        }

        if (pid == 0) { // תהליך ילד
            close(server_fd); // סגירת קצה השרת האם, לטובת התהליך הילד
            handle_client(new_socket, shared_memory, sem); // טיפול בלקוח המחובר
            exit(0); // יציאה מהתהליך הילד
        } else { // תהליך הורה
            close(new_socket); // סגירת קצה הלקוח החדש, לטובת התהליך הורה
        }
    }

    // הסרת זיכרון משותף
    shmdt(shared_memory);

    // הסרת semaphore
    sem_close(sem);
    sem_unlink("/sem_high_prime");

    return 0;
}
