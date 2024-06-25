#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

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

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <number>\n", argv[0]);
        return 1;
    }

    ll number;
    char* endptr;
    number = strtoll(argv[1], &endptr, 10);
    
    if (endptr == argv[1] || *endptr != '\0') {
        printf("Error: Invalid input. Please provide a valid number.\n");
        return 1;
    }

    if (number <= 0) {
        printf("Error: The number must be a positive integer.\n");
        return 1;
    }

    srand(time(0)); // אתחול מחולל המספרים האקראיים

    int iterations = 5; // מספר החזרות לבדיקה בשיטת מילר-רבין

    if (isPrime(number, iterations))
        printf("%lld is probably prime.\n", number);
    else
        printf("%lld is not prime.\n", number);

    return 0;
}