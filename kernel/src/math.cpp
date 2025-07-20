#include <math.h>

double pow(double base, int exp) {
    double result = 1.0;
    int negative = (exp < 0);
    
    if (negative) exp = -exp; // Handle negative exponent

    while (exp > 0) {
        if (exp & 1) result *= base; // Multiply when exp is odd
        base *= base;  // Square the base
        exp >>= 1;     // Divide exponent by 2
    }

    return negative ? (1.0 / result) : result; // If negative exponent, return reciprocal
}
