/******************************************************************************
 * Minimal math.h compatibility header
 * Only defines what we actually need, avoiding conflicts with standard library
 ******************************************************************************/

#ifndef MATH_COMPAT_H
#define MATH_COMPAT_H

#include <float.h>  // For DBL_EPSILON

// Only define the math functions we actually use
// fabs - absolute value of double
static inline double math_fabs(double x) {
    return x < 0.0 ? -x : x;
}
#define fabs math_fabs

// pow - x raised to the power of y
// Used only for pow(10.0, exponent) in JSON parsing
static inline double math_pow(double x, double y) {
    // For pow(10.0, y) where y is typically a small integer from JSON parsing
    if (y == 0.0) return 1.0;
    if (x == 10.0) {
        // Optimized path for 10.0^y
        int exp = (int)y;
        if (exp == y && exp >= -308 && exp <= 308) {
            double result = 1.0;
            if (exp > 0) {
                while (exp > 0) { result *= 10.0; exp--; }
            } else {
                while (exp < 0) { result /= 10.0; exp++; }
            }
            return result;
        }
    }
    // Fallback: simple approximation for small integer exponents
    int exp = (int)y;
    if (exp == y && exp >= -20 && exp <= 20) {
        double result = 1.0;
        if (exp > 0) {
            while (exp > 0) { result *= x; exp--; }
        } else {
            while (exp < 0) { result /= x; exp++; }
        }
        return result;
    }
    // For non-integer or large exponents, use a simple iterative approach
    // This is a basic implementation - for production, consider a proper pow
    double result = 1.0;
    double abs_y = (y < 0.0) ? -y : y;
    int i;
    for (i = 0; i < (int)abs_y && i < 1000; i++) {
        result *= x;
    }
    if (y < 0.0) result = 1.0 / result;
    return result;
}
#define pow math_pow

// floor - largest integer not greater than x
static inline double math_floor(double x) {
    if (x >= 0.0) {
        return (double)((long long)x);
    } else {
        long long n = (long long)x;
        return (x == (double)n) ? (double)n : (double)(n - 1);
    }
}
#define floor math_floor

#endif // MATH_COMPAT_H

