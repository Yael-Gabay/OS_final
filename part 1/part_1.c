#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define ll long long

// Rabin-Miller primality test function
bool isPrime(ll n, int iterations) {
    if (n <= 1 || n == 4)
        return false;
    if (n <= 3)
        return true;

    while (iterations > 0) {
        ll a = 2 + rand() % (n - 4);
        ll x = a % n;
        ll y = n - 1;
        ll result = 1;
        while (y > 0) {
            if (y & 1)
                result = (result * x) % n;
            y >>= 1;
            x = (x * x) % n;
        }
        if (result != 1)
            return false;
        iterations--;
    }
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

    int iterations = 5; // Number of iterations for Rabin-Miller test

    if (isPrime(number, iterations))
        printf("%lld is probably prime.\n", number);
    else
        printf("%lld is not prime.\n", number);

    return 0;
}
