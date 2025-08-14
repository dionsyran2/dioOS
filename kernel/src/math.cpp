#include <math.h>
#include <memory.h>
#include "math.h"


double modd(double x, double y) {
    if (y == 0.0) return 0.0 / 0.0; // return NaN
    double div = x / y;
    int64_t quotient = static_cast<int64_t>(div); // truncate toward zero
    return x - static_cast<double>(quotient) * y;
}

int floor(float x) {
    return (x >= 0) ? (int)x : (int)x - 1;  // Handles negative numbers correctly
}

int ceil(float x) {
    return (x <= 0) ? (int)x : (int)x + 1;  // Handles negative numbers correctly
}

float fabs(float x) {
    return (x < 0) ? -x : x;
}

float sqrt(float x) {
    if (x < 0) return 0; // Return 0 for negative values (no complex support)
    float res = x, last;
    do {
        last = res;
        res = (res + x / res) * 0.5f;
    } while (fabs(res - last) > 1e-6f);  // Iterative approach for convergence
    return res;
}

float pow(float x, float y) {
    if (x == 0 && y == 0) return 1; // Special case: 0^0 is typically considered 1
    if (y == 0) return 1;  // Any number raised to the power of 0 is 1
    float result = 1.0f;
    for (int i = 0; i < (int)y; i++) {
        result *= x;
    }
    return result;
}

float fmod(float x, float y) {
    if (y == 0) return 0;  // Handle division by zero
    return x - y * (int)(x / y);
}

// Calculates cosine using Taylor series (radians)
// cos(x) = 1 - x^2/2! + x^4/4! - x^6/6! + ...
double cos(double x) {
    x = modd(x, M_2PI);
    char sign = 1;
    if (x > M_PI)
    {
        x -= M_PI;
        sign = -1;
    }
    double xx = x * x;

    return sign * (1 - ((xx) / (2)) + ((xx * xx) / (24)) - ((xx * xx * xx) / (720)) + ((xx * xx * xx * xx) / (40320)) - ((xx * xx * xx * xx * xx) / (3628800)) + ((xx * xx * xx * xx * xx * xx) / (479001600)) - ((xx * xx * xx * xx * xx * xx * xx) / (87178291200)) + ((xx * xx * xx * xx * xx * xx * xx * xx) / (20922789888000)) - ((xx * xx * xx * xx * xx * xx * xx * xx * xx) / (6402373705728000)) + ((xx * xx * xx * xx * xx * xx * xx * xx * xx * xx) / (2432902008176640000)));
}


float sin(float x) {
    return cos(x - M_PI / 2);
}


float acos(float x) {
    // Clamp input between -1 and 1 for safe computation
    if (x < -1.0f) x = -1.0f;
    if (x > 1.0f) x = 1.0f;

    float result = 0.0f;
    float term = x;
    float x2 = x * x;
    int n = 1;
    for (int i = 1; i < 10; i++) {
        result += term / n;
        term *= x2;
        n += 2;
    }
    return 3.14159f / 2 - result;
}

float roundf(float x) {
    if (x >= 0.0f) {
        // Round positive numbers
        return (float)((int)(x + 0.5f)); // Add 0.5, then cast to int
    } else {
        // Round negative numbers
        return (float)((int)(x - 0.5f)); // Subtract 0.5, then cast to int
    }
}

uint32_t abs(int number){
    return number > 0 ? number : -number;
}

int ipart(float x) {
    return (int)x;
}

float fpart(float x) {
    return x - floor(x);
}

float rfpart(float x){
    return 1 - fpart(x);
}

uint64_t min(uint64_t a, uint64_t b){
    return (a > b) ? b : a;
}