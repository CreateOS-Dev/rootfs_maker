#ifndef CREATEOS_LUA_MATH_H
#define CREATEOS_LUA_MATH_H

#define HUGE_VAL (__builtin_huge_val())

double ceil(double value);
double fabs(double value);
double floor(double value);
double fmod(double value, double divisor);
double frexp(double value, int* exponent);
double ldexp(double value, int exponent);
double pow(double base, double exponent);
double sqrt(double value);
double exp(double value);
double log(double value);
double log2(double value);
double log10(double value);
double sin(double value);
double cos(double value);
double tan(double value);
double asin(double value);
double acos(double value);
double atan(double value);
double atan2(double y, double x);
double sinh(double value);
double cosh(double value);
double tanh(double value);
double modf(double value, double* integer_part);

#endif
