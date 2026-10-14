#include <math.h>

double fabs(double value)
{
    return value < 0.0 ? -value : value;
}

double floor(double value)
{
    long long integer;
    if (value != value) return value;
    if (value >= 9223372036854775807.0 ||
        value <= -9223372036854775807.0)
        return value;
    integer = (long long)value;
    if ((double)integer > value) integer--;
    return (double)integer;
}

double ceil(double value)
{
    long long integer;
    if (value != value) return value;
    if (value >= 9223372036854775807.0 ||
        value <= -9223372036854775807.0)
        return value;
    integer = (long long)value;
    if ((double)integer < value) integer++;
    return (double)integer;
}

double fmod(double value, double divisor)
{
    double remainder;
    double scaled_divisor;
    double divisor_magnitude;
    if (divisor == 0.0 || value != value || divisor != divisor ||
        fabs(value) == __builtin_huge_val())
        return __builtin_nan("");
    if (fabs(divisor) == __builtin_huge_val()) return value;
    remainder = fabs(value);
    divisor_magnitude = fabs(divisor);
    scaled_divisor = divisor_magnitude;
    while (scaled_divisor <= remainder * 0.5)
        scaled_divisor *= 2.0;
    while (scaled_divisor >= divisor_magnitude) {
        if (remainder >= scaled_divisor)
            remainder -= scaled_divisor;
        scaled_divisor *= 0.5;
    }
    return value < 0.0 ? -remainder : remainder;
}

double frexp(double value, int* exponent)
{
    int power = 0;
    if (value == 0.0 || value != value ||
        fabs(value) == __builtin_huge_val()) {
        *exponent = 0;
        return value;
    }
    while (fabs(value) >= 1.0 && power < 2048) {
        value *= 0.5;
        power++;
    }
    while (fabs(value) < 0.5 && power > -2048) {
        value *= 2.0;
        power--;
    }
    *exponent = power;
    return value;
}

double ldexp(double value, int exponent)
{
    if (exponent > 2048) exponent = 2048;
    if (exponent < -2048) exponent = -2048;
    while (exponent > 0) {
        value *= 2.0;
        exponent--;
    }
    while (exponent < 0) {
        value *= 0.5;
        exponent++;
    }
    return value;
}

double pow(double base, double exponent)
{
    double result = 1.0;
    double factor = base;
    double integral;
    if (base != base || exponent != exponent)
        return __builtin_nan("");
    if (exponent == 0.0) return 1.0;
    if (base == 1.0) return 1.0;
    if (base == -1.0 && fabs(exponent) == __builtin_huge_val()) return 1.0;
    double fraction = modf(exponent, &integral);
    unsigned long long power;
    int reciprocal = exponent < 0.0;

    if (base == 0.0 && exponent < 0.0) return __builtin_huge_val();
    if (base < 0.0 && fraction != 0.0) return __builtin_nan("");
    if (exponent > 9.0e18 || exponent < -9.0e18)
        return exponent < 0.0 ? (fabs(base) < 1.0 ? __builtin_huge_val() :
                                 (fabs(base) > 1.0 ? 0.0 : 1.0)) :
                               (fabs(base) < 1.0 ? 0.0 :
                                (fabs(base) > 1.0 ? __builtin_huge_val() : 1.0));
    if (reciprocal) {
        if (base == 0.0) return 0.0;
        factor = 1.0 / factor;
    }
    power = (unsigned long long)(integral < 0.0 ? -integral : integral);
    while (power) {
        if (power & 1) result *= factor;
        factor *= factor;
        power >>= 1;
    }
    if (fraction != 0.0) {
        double fractional_result = exp(fraction * log(fabs(base)));
        result *= reciprocal ? 1.0 / fractional_result : fractional_result;
    }
    return result;
}

double sqrt(double value)
{
    double estimate;
    if (value < 0.0) return __builtin_nan("");
    if (value == 0.0 || value != value) return value;
    estimate = value >= 1.0 ? value : 1.0;
    for (int i = 0; i < 96; i++) {
        double next = 0.5 * (estimate + value / estimate);
        if (fabs(next - estimate) <= fabs(next) * 1.0e-15) {
            estimate = next;
            break;
        }
        estimate = next;
    }
    return estimate;
}

double exp(double value)
{
    const double ln2 = 0.69314718055994530942;
    int exponent;
    double reduced;
    double term = 1.0;
    double result = 1.0;

    if (value != value) return value;
    if (value > 709.782712893384) return __builtin_huge_val();
    if (value < -745.133219101941) return 0.0;
    exponent = (int)floor(value / ln2 + 0.5);
    reduced = value - exponent * ln2;
    for (int i = 1; i <= 40; i++) {
        term *= reduced / i;
        result += term;
    }
    return ldexp(result, exponent);
}

double log(double value)
{
    const double ln2 = 0.69314718055994530942;
    double normalized;
    double z;
    double z_squared;
    double term;
    double sum = 0.0;
    int exponent;

    if (value < 0.0) return __builtin_nan("");
    if (value == 0.0) return -__builtin_huge_val();
    if (value == __builtin_huge_val()) return value;
    normalized = frexp(value, &exponent) * 2.0;
    exponent--;
    z = (normalized - 1.0) / (normalized + 1.0);
    z_squared = z * z;
    term = z;
    for (int divisor = 1; divisor < 200; divisor += 2) {
        sum += term / divisor;
        term *= z_squared;
        if (fabs(term) < 1.0e-18) break;
    }
    return 2.0 * sum + exponent * ln2;
}

double log2(double value)
{
    return log(value) / 0.69314718055994530942;
}

double log10(double value)
{
    return log(value) / 2.30258509299404568402;
}

static double reduce_angle(double value)
{
    const double two_pi = 6.28318530717958647693;
    if (fabs(value) > 1.0e15) return __builtin_nan("");
    value = fmod(value, two_pi);
    if (value > 3.14159265358979323846)
        value -= two_pi;
    else if (value < -3.14159265358979323846)
        value += two_pi;
    return value;
}

double sin(double value)
{
    double x = reduce_angle(value);
    double term;
    double result;
    if (x != x) return x;
    if (x > 1.57079632679489661923)
        x = 3.14159265358979323846 - x;
    else if (x < -1.57079632679489661923)
        x = -3.14159265358979323846 - x;
    term = result = x;
    for (int i = 1; i <= 12; i++) {
        term *= -x * x / ((2 * i) * (2 * i + 1));
        result += term;
    }
    return result;
}

double cos(double value)
{
    return sin(value + 1.57079632679489661923);
}

double tan(double value)
{
    double cosine = cos(value);
    if (cosine == 0.0) return sin(value) < 0.0 ?
        -__builtin_huge_val() : __builtin_huge_val();
    return sin(value) / cosine;
}

double atan(double value)
{
    const double pi_over_4 = 0.78539816339744830962;
    double sign = value < 0.0 ? -1.0 : 1.0;
    double x = fabs(value);
    double result;

    if (x > 1.0)
        return sign * (1.57079632679489661923 - atan(1.0 / x));
    if (x > 0.41421356237309504880)
        return sign * (pi_over_4 +
            atan((x - 1.0) / (x + 1.0)));
    result = x;
    {
        double term = x;
        double square = x * x;
        for (int i = 1; i < 64; i++) {
            term *= -square;
            result += term / (2 * i + 1);
            if (fabs(term) < 1.0e-18) break;
        }
    }
    return sign * result;
}

double atan2(double y, double x)
{
    if (x > 0.0) return atan(y / x);
    if (x < 0.0)
        return y >= 0.0 ? atan(y / x) + 3.14159265358979323846 :
                          atan(y / x) - 3.14159265358979323846;
    if (y > 0.0) return 1.57079632679489661923;
    if (y < 0.0) return -1.57079632679489661923;
    return 0.0;
}

double asin(double value)
{
    if (value < -1.0 || value > 1.0) return __builtin_nan("");
    return atan2(value, sqrt(1.0 - value * value));
}

double acos(double value)
{
    if (value < -1.0 || value > 1.0) return __builtin_nan("");
    return atan2(sqrt(1.0 - value * value), value);
}

double sinh(double value)
{
    return (exp(value) - exp(-value)) * 0.5;
}

double cosh(double value)
{
    return (exp(value) + exp(-value)) * 0.5;
}

double tanh(double value)
{
    double positive = exp(2.0 * fabs(value));
    double result = (positive - 1.0) / (positive + 1.0);
    return value < 0.0 ? -result : result;
}

double modf(double value, double* integer_part)
{
    if (value != value || fabs(value) == __builtin_huge_val()) {
        if (integer_part) *integer_part = value;
        return value != value ? value : 0.0;
    }
    double integer = value < 0.0 ? ceil(value) : floor(value);
    if (integer_part) *integer_part = integer;
    return value - integer;
}
