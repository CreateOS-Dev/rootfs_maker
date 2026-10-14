#include "user_syscall.h"
#include <errno.h>
#include <locale.h>
#include <time.h>
#include "lua.h"
#include "lauxlib.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h>
#include <limits.h>
#include <math.h>

#include "lua_compat.h"

#define CREATEOS_LUA_FILE_SLOTS 16

struct createos_lua_FILE {
    int descriptor;
    unsigned int eof;
    unsigned int error;
    unsigned int standard;
    int pushback;
    unsigned int has_pushback;
    char temporary_path[128];
};

static struct createos_lua_FILE lua_files[CREATEOS_LUA_FILE_SLOTS] = {
    {-1, 0, 0, 0, 0, 0, {0}},
    {-1, 0, 0, 0, 0, 0, {0}},
    {-1, 0, 0, 0, 0, 0, {0}},
    {-1, 0, 0, 0, 0, 0, {0}}
};
static struct createos_lua_FILE lua_stdin = {-1, 0, 0, 1, 0, 0, {0}};
static struct createos_lua_FILE lua_stdout = {-1, 0, 0, 2, 0, 0, {0}};
static struct createos_lua_FILE lua_stderr = {-1, 0, 0, 2, 0, 0, {0}};
FILE* stdin = &lua_stdin;
FILE* stdout = &lua_stdout;
FILE* stderr = &lua_stderr;

void createos_lua_write(const char* bytes, size_t length)
{
    size_t written = 0;
    while (written < length) {
        ssize_t result = sys_write(1, bytes + written, length - written);
        if (result <= 0) return;
        written += (size_t)result;
    }
}

void createos_lua_write_error(const char* format, const char* message)
{
    const char* marker = strstr(format, "%s");
    if (!marker) {
        createos_lua_write(format, strlen(format));
        return;
    }
    createos_lua_write(format, (size_t)(marker - format));
    createos_lua_write(message, strlen(message));
    createos_lua_write(marker + 2, strlen(marker + 2));
}

char* createos_lua_strpbrk(const char* string, const char* accept)
{
    for (; *string; string++)
        if (strchr(accept, *string)) return (char*)string;
    return 0;
}

size_t createos_lua_strspn(const char* string, const char* accept)
{
    const char* cursor = string;
    while (*cursor && strchr(accept, *cursor)) cursor++;
    return (size_t)(cursor - string);
}

int createos_lua_abs(int value)
{
    return value < 0 ? -value : value;
}

int createos_lua_strcoll(const char* left, const char* right)
{
    return strcmp(left, right);
}

static size_t lua_append(char* output, size_t capacity, size_t offset,
                         const char* text, size_t length)
{
    if (output && capacity && offset < capacity - 1) {
        size_t available = capacity - 1 - offset;
        size_t copied = length < available ? length : available;
        memcpy(output + offset, text, copied);
    }
    return offset + length;
}

static size_t lua_unsigned_text(char* output, unsigned long long value,
                               unsigned int base, int uppercase)
{
    char reverse[32];
    size_t length = 0;
    do {
        unsigned int digit = (unsigned int)(value % base);
        reverse[length++] = digit < 10 ? (char)('0' + digit) :
            (char)((uppercase ? 'A' : 'a') + digit - 10);
        value /= base;
    } while (value);
    for (size_t i = 0; i < length; i++)
        output[i] = reverse[length - i - 1];
    return length;
}

static size_t lua_format_float(char* output, double value, char specifier,
                               int precision)
{
    char digits[32];
    size_t length = 0;
    int negative = value < 0.0;
    int exponent = 0;
    int scientific = specifier == 'e' || specifier == 'E';
    int general = specifier == 'g' || specifier == 'G';
    int uppercase = specifier == 'E' || specifier == 'G' || specifier == 'F';
    double magnitude = negative ? -value : value;
    double normalized = magnitude;
    unsigned long long integer;

    if (value != value) {
        memcpy(output, uppercase ? "NAN" : "nan", 3);
        return 3;
    }
    if (magnitude > 1.0e300) {
        size_t at = 0;
        if (negative) output[at++] = '-';
        memcpy(output + at, uppercase ? "INF" : "inf", 3);
        return at + 3;
    }
    if (precision < 0) precision = general ? 6 : 6;
    if (precision > 15) precision = 15;
    if (general && precision == 0) precision = 1;
    if (scientific || general) {
        if (normalized != 0.0) {
            while (normalized >= 10.0 && exponent < 300) {
                normalized *= 0.1;
                exponent++;
            }
            while (normalized < 1.0 && exponent > -300) {
                normalized *= 10.0;
                exponent--;
            }
        }
        if (general)
            scientific = exponent < -4 || exponent >= precision;
    }
    if (negative) output[length++] = '-';
    if (general && !scientific) {
        int fractional = precision - exponent - 1;
        if (fractional < 0) fractional = 0;
        if (fractional > 15) fractional = 15;
        double multiplier = 1.0;
        for (int i = 0; i < fractional; i++) multiplier *= 10.0;
        if (magnitude > 18446744073709549568.0) {
            memcpy(output + length, uppercase ? "INF" : "inf", 3);
            return length + 3;
        }
        integer = (unsigned long long)(magnitude * multiplier + 0.5);
        unsigned long long divisor = 1;
        for (int i = 0; i < fractional; i++) divisor *= 10;
        size_t whole_length =
            lua_unsigned_text(digits, integer / divisor, 10, 0);
        memcpy(output + length, digits, whole_length);
        length += whole_length;
        unsigned long long remainder = integer % divisor;
        if (fractional && remainder) {
            output[length++] = '.';
            for (int i = fractional - 1; i >= 0; i--) {
                unsigned long long place = 1;
                for (int j = 0; j < i; j++) place *= 10;
                output[length++] = (char)('0' + (remainder / place) % 10);
            }
            while (output[length - 1] == '0') length--;
            if (output[length - 1] == '.') length--;
        }
        return length;
    }
    int fractional = scientific ? precision : (general ? 6 : precision);
    if (general && scientific) fractional = precision - 1;
    if (magnitude > 18446744073709549568.0) {
        memcpy(output + length, uppercase ? "INF" : "inf", 3);
        return length + 3;
    }
    double multiplier = 1.0;
    for (int i = 0; i < fractional; i++) multiplier *= 10.0;
    if (scientific) {
        integer = (unsigned long long)(normalized * multiplier + 0.5);
    } else {
        integer = (unsigned long long)(magnitude * multiplier + 0.5);
    }
    unsigned long long divisor = 1;
    for (int i = 0; i < fractional; i++) divisor *= 10;
    size_t whole_length =
        lua_unsigned_text(digits, integer / divisor, 10, 0);
    memcpy(output + length, digits, whole_length);
    length += whole_length;
    unsigned long long remainder = integer % divisor;
    if (fractional) {
        output[length++] = '.';
        for (int i = fractional - 1; i >= 0; i--) {
            unsigned long long place = 1;
            for (int j = 0; j < i; j++) place *= 10;
            output[length++] = (char)('0' + (remainder / place) % 10);
        }
    }
    if (general && fractional) {
        while (output[length - 1] == '0') length--;
        if (output[length - 1] == '.') length--;
    }
    if (scientific) {
        output[length++] = uppercase ? 'E' : 'e';
        output[length++] = exponent < 0 ? '-' : '+';
        if (exponent < 0) exponent = -exponent;
        if (exponent < 10) output[length++] = '0';
        length += lua_unsigned_text(output + length,
                                    (unsigned long long)exponent, 10, 0);
    }
    return length;
}

int createos_lua_integer_to_string(char* output, size_t capacity,
                                   long long value)
{
    char converted[32];
    size_t length = 0;
    unsigned long long magnitude;

    if (value < 0) {
        converted[length++] = '-';
        magnitude = (unsigned long long)(-(value + 1)) + 1;
    } else {
        magnitude = (unsigned long long)value;
    }
    length += lua_unsigned_text(converted + length, magnitude, 10, 0);
    if (output && capacity) {
        size_t copied = length < capacity - 1 ? length : capacity - 1;
        memcpy(output, converted, copied);
        output[copied] = 0;
    }
    return (int)length;
}

int createos_lua_number_to_string(char* output, size_t capacity,
                                  double value)
{
    char converted[64];
    size_t length = lua_format_float(converted, value, 'g', 14);
    if (output && capacity) {
        size_t copied = length < capacity - 1 ? length : capacity - 1;
        memcpy(output, converted, copied);
        output[copied] = 0;
    }
    return length > 2147483647u ? -1 : (int)length;
}

double createos_lua_strtod(const char* string, char** end)
{
    const char* cursor = string;
    double value = 0.0;
    double scale = 1.0;
    int negative = 0;
    int exponent = 0;
    int exponent_negative = 0;
    int digits = 0;

    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' ||
           *cursor == '\r') cursor++;
    if (*cursor == '-') {
        negative = 1;
        cursor++;
    } else if (*cursor == '+') {
        cursor++;
    }
    while (*cursor >= '0' && *cursor <= '9') {
        value = value * 10.0 + (*cursor++ - '0');
        digits++;
    }
    if (*cursor == '.') {
        cursor++;
        while (*cursor >= '0' && *cursor <= '9') {
            scale *= 0.1;
            value += (*cursor++ - '0') * scale;
            digits++;
        }
    }
    if (!digits) {
        if (end) *end = (char*)string;
        return 0.0;
    }
    if (*cursor == 'e' || *cursor == 'E') {
        const char* exponent_start = cursor++;
        if (*cursor == '-') {
            exponent_negative = 1;
            cursor++;
        } else if (*cursor == '+') {
            cursor++;
        }
        if (*cursor < '0' || *cursor > '9') {
            if (end) *end = (char*)exponent_start;
            return negative ? -value : value;
        }
        while (*cursor >= '0' && *cursor <= '9' && exponent < 400)
            exponent = exponent * 10 + (*cursor++ - '0');
        while (exponent-- > 0)
            value = exponent_negative ? value * 0.1 : value * 10.0;
    }
    if (end) *end = (char*)cursor;
    return negative ? -value : value;
}

void* memchr(const void* memory, int value, size_t length)
{
    const unsigned char* bytes = memory;
    for (size_t i = 0; i < length; i++)
        if (bytes[i] == (unsigned char)value) return (void*)(bytes + i);
    return 0;
}

static int64_t days_from_civil(int64_t year, unsigned int month,
                               unsigned int day)
{
    int64_t era;
    unsigned int year_of_era;
    unsigned int day_of_year;
    unsigned int day_of_era;

    year -= month <= 2;
    era = (year >= 0 ? year : year - 399) / 400;
    year_of_era = (unsigned int)(year - era * 400);
    day_of_year = (153 * (month + (month > 2 ? (unsigned int)-3 : 9)) + 2) /
                  5 + day - 1;
    day_of_era = year_of_era * 365 + year_of_era / 4 -
                 year_of_era / 100 + day_of_year;
    return era * 146097 + (int64_t)day_of_era - 719468;
}

static void civil_from_days(int64_t days, int64_t* year,
                            unsigned int* month, unsigned int* day)
{
    int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    unsigned int day_of_era = (unsigned int)(days - era * 146097);
    unsigned int year_of_era = (day_of_era - day_of_era / 1460 +
        day_of_era / 36524 - day_of_era / 146096) / 365;
    int64_t y = (int64_t)year_of_era + era * 400;
    unsigned int day_of_year = day_of_era -
        (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    unsigned int month_prime = (5 * day_of_year + 2) / 153;

    *day = day_of_year - (153 * month_prime + 2) / 5 + 1;
    *month = month_prime + (month_prime < 10 ? 3 : (unsigned int)-9);
    y += *month <= 2;
    *year = y;
}

time_t time(time_t* result)
{
    rtc_time_t rtc;
    time_t now;
    if (sys_time(&rtc) < 0 || rtc.month < 1 || rtc.month > 12 ||
        rtc.day < 1 || rtc.day > 31 || rtc.hour > 23 ||
        rtc.minute > 59 || rtc.second > 60)
        return (time_t)-1;
    now = (time_t)days_from_civil(2000 + rtc.year, rtc.month, rtc.day) *
          86400 + rtc.hour * 3600 + rtc.minute * 60 + rtc.second;
    if (result) *result = now;
    return now;
}

struct tm* gmtime_r(const time_t* value, struct tm* result)
{
    static const unsigned short days_before_month[12] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    int64_t seconds;
    int64_t days;
    int64_t year;
    int64_t weekday;
    unsigned int month;
    unsigned int day;
    int leap;

    if (!value || !result) return 0;
    seconds = *value;
    days = seconds / 86400;
    seconds %= 86400;
    if (seconds < 0) {
        seconds += 86400;
        days--;
    }
    civil_from_days(days, &year, &month, &day);
    if (year - 1900 < INT_MIN || year - 1900 > INT_MAX)
        return 0;
    leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    result->tm_sec = (int)(seconds % 60);
    result->tm_min = (int)((seconds / 60) % 60);
    result->tm_hour = (int)(seconds / 3600);
    result->tm_mday = (int)day;
    result->tm_mon = (int)month - 1;
    result->tm_year = (int)(year - 1900);
    result->tm_yday = days_before_month[month - 1] + (int)day - 1;
    if (leap && month > 2) result->tm_yday++;
    weekday = (days + 4) % 7;
    if (weekday < 0) weekday += 7;
    result->tm_wday = (int)weekday;
    result->tm_isdst = 0;
    return result;
}

struct tm* localtime_r(const time_t* value, struct tm* result)
{
    return gmtime_r(value, result);
}

struct tm* gmtime(const time_t* value)
{
    static struct tm result;
    return gmtime_r(value, &result);
}

struct tm* localtime(const time_t* value)
{
    return gmtime(value);
}

time_t mktime(struct tm* value)
{
    int64_t month_index;
    int64_t year;
    int64_t month;
    int64_t days;
    time_t result;
    struct tm normalized;

    if (!value) return (time_t)-1;
    month_index = (int64_t)value->tm_year + 1900;
    month_index = month_index * 12 + value->tm_mon;
    year = month_index / 12;
    month = month_index % 12;
    if (month < 0) {
        month += 12;
        year--;
    }
    days = days_from_civil(year, (unsigned int)month + 1, 1) +
           (int64_t)value->tm_mday - 1;
    result = (time_t)(days * 86400 + (int64_t)value->tm_hour * 3600 +
                      (int64_t)value->tm_min * 60 + value->tm_sec);
    if (!gmtime_r(&result, &normalized)) return (time_t)-1;
    *value = normalized;
    return result;
}

double difftime(time_t end, time_t beginning)
{
    return (double)end - (double)beginning;
}

static size_t time_append(char* output, size_t capacity, size_t offset,
                          const char* text, size_t length)
{
    if (output && capacity && offset < capacity - 1) {
        size_t available = capacity - offset - 1;
        size_t copied = length < available ? length : available;
        memcpy(output + offset, text, copied);
    }
    return offset + length;
}

size_t strftime(char* output, size_t capacity, const char* format,
                const struct tm* value)
{
    static const char* const weekdays[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
        "Saturday"
    };
    static const char* const months[] = {
        "January", "February", "March", "April", "May", "June", "July",
        "August", "September", "October", "November", "December"
    };
    char number[32];
    size_t length = 0;

    if (!format || !value || (!output && capacity)) return 0;
    for (size_t i = 0; format[i]; i++) {
        const char* text = format + i;
        size_t text_length = 1;
        if (format[i] == '%') {
            char specifier = format[++i];
            if (!specifier) break;
            switch (specifier) {
            case 'a': text = weekdays[value->tm_wday % 7]; text_length = 3; break;
            case 'A': text = weekdays[value->tm_wday % 7]; text_length = 0; break;
            case 'b': text = months[value->tm_mon % 12]; text_length = 3; break;
            case 'B': text = months[value->tm_mon % 12]; text_length = 0; break;
            case 'c':
                text = number;
                snprintf(number, sizeof(number), "%s %s %02d %02d:%02d:%02d %04d",
                         weekdays[value->tm_wday % 7],
                         months[value->tm_mon % 12], value->tm_mday,
                         value->tm_hour, value->tm_min, value->tm_sec,
                         value->tm_year + 1900);
                text_length = strlen(text);
                break;
            case 'd': snprintf(number, sizeof(number), "%02d", value->tm_mday); goto append_number;
            case 'H': snprintf(number, sizeof(number), "%02d", value->tm_hour); goto append_number;
            case 'I': snprintf(number, sizeof(number), "%02d",
                               value->tm_hour % 12 ? value->tm_hour % 12 : 12);
                      goto append_number;
            case 'j': snprintf(number, sizeof(number), "%03d", value->tm_yday + 1); goto append_number;
            case 'm': snprintf(number, sizeof(number), "%02d", value->tm_mon + 1); goto append_number;
            case 'M': snprintf(number, sizeof(number), "%02d", value->tm_min); goto append_number;
            case 'p': text = value->tm_hour < 12 ? "AM" : "PM"; text_length = 2; break;
            case 'S': snprintf(number, sizeof(number), "%02d", value->tm_sec); goto append_number;
            case 'U': snprintf(number, sizeof(number), "%02d",
                               (value->tm_yday + 7 - value->tm_wday) / 7);
                      goto append_number;
            case 'w': snprintf(number, sizeof(number), "%d", value->tm_wday); goto append_number;
            case 'W': snprintf(number, sizeof(number), "%02d",
                               (value->tm_yday + 7 -
                                (value->tm_wday + 6) % 7) / 7);
                      goto append_number;
            case 'x': snprintf(number, sizeof(number), "%02d/%02d/%02d",
                               value->tm_mon + 1, value->tm_mday,
                               (value->tm_year + 1900) % 100);
                      goto append_number;
            case 'X': snprintf(number, sizeof(number), "%02d:%02d:%02d",
                               value->tm_hour, value->tm_min, value->tm_sec);
                      goto append_number;
            case 'y': snprintf(number, sizeof(number), "%02d",
                               (value->tm_year + 1900) % 100);
                      goto append_number;
            case 'Y': snprintf(number, sizeof(number), "%04d",
                               value->tm_year + 1900);
                      goto append_number;
            case 'Z': text = "UTC"; text_length = 3; break;
            case '%': text = "%"; text_length = 1; break;
            default: return 0;
append_number:
                text = number;
                text_length = strlen(number);
                break;
            }
            if (!text_length) text_length = strlen(text);
        }
        length = time_append(output, capacity, length, text, text_length);
    }
    if (capacity) {
        if (length >= capacity) {
            output[capacity - 1] = 0;
            return 0;
        }
        output[length] = 0;
    }
    return length;
}

clock_t clock(void)
{
    int64_t elapsed = sys_monotonic_ns();
    return elapsed < 0 ? (clock_t)-1 : (clock_t)elapsed;
}

char* setlocale(int category, const char* locale)
{
    static char current_locale[] = "C";
    (void)category;
    if (!locale || !locale[0]) return current_locale;
    if (strcmp(locale, "C") == 0 || strcmp(locale, "POSIX") == 0)
        return current_locale;
    return 0;
}

char* getenv(const char* name)
{
    static char home[sizeof(((user_t*)0)->home)];
    static char cwd[256];
    user_t user;
    if (!name) return 0;
    if (strcmp(name, "PATH") == 0) return "/bin:/usr/bin";
    if (strcmp(name, "HOME") == 0 &&
        sys_user_current(&user) == 0) {
        strcpy(home, user.home);
        return home;
    }
    if (strcmp(name, "PWD") == 0 && sys_getcwd(cwd, sizeof(cwd)) == 0)
        return cwd;
    return 0;
}

int system(const char* command)
{
    (void)command;
    errno = ENOSYS;
    return -1;
}

char* tmpnam(char* buffer)
{
    static unsigned int sequence;
    static char static_buffer[L_tmpnam];
    char* output = buffer ? buffer : static_buffer;
    for (unsigned int attempt = 0; attempt < 32; attempt++) {
        uint32_t random_value = 0;
        fs_stat_t status;
        if (sys_random(&random_value, sizeof(random_value)) !=
            (int)sizeof(random_value))
            random_value = (uint32_t)sys_ticks() ^ ++sequence;
        if (snprintf(output, L_tmpnam, "lua-tmp-%08x", random_value) < 0)
            return 0;
        if (sys_stat(output, &status) < 0)
            return output;
    }
    errno = EEXIST;
    return 0;
}

void exit(int status)
{
    sys_exit(status);
}

int createos_lua_snprintf(char* output, size_t capacity,
                          const char* format, ...)
{
    va_list arguments;
    const char* conversion = format;
    const char* cursor;
    char value[128];
    size_t value_length = 0;
    size_t output_length = 0;
    int precision = -1;
    int width = 0;
    int zero_pad = 0;
    int long_count = 0;
    char specifier;

    while (*conversion && *conversion != '%') conversion++;
    if (!*conversion || conversion[1] == '%') {
        va_start(arguments, format);
        int result = vsnprintf(output, capacity, format, arguments);
        va_end(arguments);
        return result;
    }
    cursor = conversion + 1;
    while (*cursor == '0' || *cursor == '+' || *cursor == '-' ||
           *cursor == ' ' || *cursor == '#') {
        if (*cursor == '0') zero_pad = 1;
        cursor++;
    }
    while (*cursor >= '0' && *cursor <= '9')
        width = width * 10 + (*cursor++ - '0');
    if (*cursor == '.') {
        cursor++;
        precision = 0;
        while (*cursor >= '0' && *cursor <= '9')
            precision = precision * 10 + (*cursor++ - '0');
    }
    while (*cursor == 'l' || *cursor == 'h' || *cursor == 'z' ||
           *cursor == 'L') {
        if (*cursor == 'l') long_count++;
        cursor++;
    }
    specifier = *cursor;
    va_start(arguments, format);
    switch (specifier) {
    case 'g': case 'G': case 'f': case 'F': case 'e': case 'E':
        value_length = lua_format_float(value, va_arg(arguments, double),
                                        specifier, precision);
        break;
    case 'd': case 'i': case 'u': case 'o': case 'x': case 'X': {
        unsigned long long number;
        int signed_conversion = specifier == 'd' || specifier == 'i';
        int negative = 0;
        if (signed_conversion) {
            long long signed_number = long_count > 1
                ? va_arg(arguments, long long) :
                  long_count ? va_arg(arguments, long) : va_arg(arguments, int);
            negative = signed_number < 0;
            number = negative ? (unsigned long long)(-(signed_number + 1)) + 1 :
                                (unsigned long long)signed_number;
        } else if (long_count > 1) {
            number = va_arg(arguments, unsigned long long);
        } else if (long_count) {
            number = va_arg(arguments, unsigned long);
        } else {
            number = va_arg(arguments, unsigned int);
        }
        if (negative) value[value_length++] = '-';
        value_length += lua_unsigned_text(value + value_length, number,
            specifier == 'o' ? 8u :
            (specifier == 'x' || specifier == 'X') ? 16u : 10u,
            specifier == 'X');
        break;
    }
    case 's': {
        const char* text = va_arg(arguments, const char*);
        if (!text) text = "(null)";
        value_length = strlen(text);
        if (precision >= 0 && value_length > (size_t)precision)
            value_length = (size_t)precision;
        if (value_length >= sizeof(value)) value_length = sizeof(value) - 1;
        memcpy(value, text, value_length);
        break;
    }
    case 'c':
        value[0] = (char)va_arg(arguments, int);
        value_length = 1;
        break;
    case 'p':
        value[0] = '0';
        value[1] = 'x';
        value_length = 2 + lua_unsigned_text(value + 2,
            (unsigned long long)(uintptr_t)va_arg(arguments, void*), 16, 0);
        break;
    default:
        va_end(arguments);
        return -1;
    }
    va_end(arguments);
    output_length = lua_append(output, capacity, output_length,
                               format, (size_t)(conversion - format));
    if (width > (int)value_length) {
        size_t padding = (size_t)width - value_length;
        char pad = zero_pad ? '0' : ' ';
        while (padding--)
            output_length = lua_append(output, capacity, output_length,
                                       &pad, 1);
    }
    output_length = lua_append(output, capacity, output_length, value,
                               value_length);
    output_length = lua_append(output, capacity, output_length,
                               cursor + 1, strlen(cursor + 1));
    if (output && capacity)
        output[output_length < capacity ? output_length : capacity - 1] = 0;
    return output_length > 2147483647u ? -1 : (int)output_length;
}

int luaL_loadfilex(lua_State* state, const char* path, const char* mode)
{
    fs_stat_t status;
    char* source;
    ssize_t count;
    int result;
    (void)mode;
    if (!path || sys_stat(path, &status) < 0 ||
        status.type != FS_TYPE_FILE || status.size > 1024u * 1024u) {
        lua_pushfstring(state, "cannot open %s", path ? path : "(null)");
        return LUA_ERRFILE;
    }
    source = malloc((size_t)status.size + 1);
    if (!source) {
        lua_pushliteral(state, "not enough memory");
        return LUA_ERRMEM;
    }
    count = sys_read_at(path, source, status.size, 0);
    if (count < 0 || (uint32_t)count != status.size) {
        free(source);
        lua_pushfstring(state, "cannot read %s", path);
        return LUA_ERRFILE;
    }
    source[status.size] = 0;
    result = luaL_loadbufferx(state, source, status.size, path, mode);
    free(source);
    return result;
}

FILE* fopen(const char* path, const char* mode)
{
    int flags;
    int descriptor;
    int plus;
    int binary = 0;
    const char* option;
    if (!path || !mode || !mode[0]) return 0;
    if (mode[0] != 'r' && mode[0] != 'w' && mode[0] != 'a')
        return 0;
    plus = 0;
    for (option = mode + 1; *option; option++) {
        if (*option == '+' && !plus)
            plus = 1;
        else if (*option == 'b' && !binary)
            binary = 1;
        else
            return 0;
    }
    if (mode[0] == 'r')
        flags = plus ? O_RDWR : O_RDONLY;
    else if (mode[0] == 'w')
        flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC;
    else
        flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
    descriptor = sys_open(path, flags, 0644);
    if (descriptor < 0) return 0;
    for (size_t i = 0; i < CREATEOS_LUA_FILE_SLOTS; i++) {
        if (lua_files[i].descriptor < 0) {
            lua_files[i].descriptor = descriptor;
            lua_files[i].eof = 0;
            lua_files[i].error = 0;
            lua_files[i].has_pushback = 0;
            lua_files[i].temporary_path[0] = 0;
            return &lua_files[i];
        }
    }
    (void)sys_close(descriptor);
    return 0;
}

int fclose(FILE* stream)
{
    int result;
    if (!stream || stream->standard || stream->descriptor < 0) return -1;
    result = sys_close(stream->descriptor);
    stream->descriptor = -1;
    stream->eof = 0;
    stream->error = result < 0;
    stream->has_pushback = 0;
    if (stream->temporary_path[0]) {
        if (sys_unlink(stream->temporary_path) < 0)
            result = -1;
        stream->temporary_path[0] = 0;
    }
    return result;
}

size_t fread(void* buffer, size_t size, size_t count, FILE* stream)
{
    size_t total;
    size_t read_count;
    if (!stream || !buffer || !size || count > (size_t)-1 / size)
        return 0;
    total = size * count;
    read_count = 0;
    if (stream->standard == 1) {
        while (read_count < total) {
            int character;
            if (stream->has_pushback) {
                character = stream->pushback;
                stream->has_pushback = 0;
            } else {
                character = (unsigned char)sys_getc();
                if (character == 4) {
                    stream->eof = 1;
                    break;
                }
            }
            ((unsigned char*)buffer)[read_count++] =
                (unsigned char)character;
        }
        return read_count / size;
    }
    if (stream->standard || stream->descriptor < 0) {
        stream->error = 1;
        return 0;
    }
    if (stream->has_pushback && total > 0) {
        ((unsigned char*)buffer)[0] = (unsigned char)stream->pushback;
        stream->has_pushback = 0;
        read_count = 1;
    } else {
        read_count = 0;
    }
    while (read_count < total) {
        ssize_t result = sys_read(stream->descriptor,
                                  (uint8_t*)buffer + read_count,
                                  total - read_count);
        if (result < 0) {
            stream->error = 1;
            break;
        }
        if (!result) {
            stream->eof = 1;
            break;
        }
        read_count += (size_t)result;
    }
    return read_count / size;
}

size_t fwrite(const void* buffer, size_t size, size_t count, FILE* stream)
{
    size_t total;
    size_t written = 0;
    if (!stream || !buffer || !size || count > (size_t)-1 / size)
        return 0;
    total = size * count;
    if (stream->standard) {
        while (written < total) {
            ssize_t result = sys_write(1, (const uint8_t*)buffer + written,
                                       total - written);
            if (result <= 0) {
                stream->error = 1;
                break;
            }
            written += (size_t)result;
        }
        return written / size;
    }
    if (stream->descriptor < 0) return 0;
    while (written < total) {
        ssize_t result = sys_write(stream->descriptor,
                                   (const uint8_t*)buffer + written,
                                   total - written);
        if (result <= 0) {
            stream->error = 1;
            break;
        }
        written += (size_t)result;
    }
    return written / size;
}

int ferror(FILE* stream)
{
    return stream ? (int)stream->error : 1;
}

int feof(FILE* stream)
{
    return stream ? (int)stream->eof : 1;
}

int fflush(FILE* stream)
{
    (void)stream;
    return 0;
}

int fgetc(FILE* stream)
{
    unsigned char character;
    if (!stream) return EOF;
    if (stream->has_pushback) {
        stream->has_pushback = 0;
        return stream->pushback;
    }
    if (stream->standard == 1) {
        int input = (unsigned char)sys_getc();
        if (input == 4) {
            stream->eof = 1;
            return EOF;
        }
        return input;
    }
    return fread(&character, 1, 1, stream) == 1 ? character : EOF;
}

int getc(FILE* stream)
{
    return fgetc(stream);
}

int ungetc(int character, FILE* stream)
{
    if (!stream || character == EOF || stream->has_pushback)
        return EOF;
    stream->pushback = (unsigned char)character;
    stream->has_pushback = 1;
    stream->eof = 0;
    return stream->pushback;
}

void clearerr(FILE* stream)
{
    if (!stream) return;
    stream->eof = 0;
    stream->error = 0;
}

int fseek(FILE* stream, long offset, int origin)
{
    if (!stream || stream->standard || stream->descriptor < 0 ||
        (origin != SEEK_SET && origin != SEEK_CUR && origin != SEEK_END))
        return -1;
    if (stream->has_pushback && origin == SEEK_CUR) {
        if (offset == LONG_MIN) return -1;
        offset--;
    }
    if (sys_seek(stream->descriptor, offset, origin) < 0) {
        stream->error = 1;
        return -1;
    }
    stream->eof = 0;
    stream->has_pushback = 0;
    return 0;
}

long ftell(FILE* stream)
{
    int64_t position;
    if (!stream || stream->standard || stream->descriptor < 0) return -1;
    position = sys_seek(stream->descriptor, 0, SEEK_CUR);
    if (position >= 0 && stream->has_pushback)
        position--;
    if (position < 0 || position > LONG_MAX) {
        stream->error = 1;
        return -1;
    }
    return (long)position;
}

int setvbuf(FILE* stream, char* buffer, int mode, size_t size)
{
    (void)buffer;
    (void)size;
    if (!stream || mode != _IONBF)
        return -1;
    return 0;
}

FILE* tmpfile(void)
{
    static unsigned int sequence;
    char path[128];
    int descriptor;
    for (unsigned int attempt = 0; attempt < 32; attempt++) {
        uint32_t random_value = 0;
        if (sys_random(&random_value, sizeof(random_value)) !=
            (int)sizeof(random_value))
            random_value = (uint32_t)sys_ticks() ^ ++sequence;
        int length = snprintf(path, sizeof(path), "lua-tmp-%08x-%u",
                              random_value, ++sequence);
        if (length <= 0 || (size_t)length >= sizeof(path)) return 0;
        descriptor = sys_open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (descriptor >= 0) break;
    }
    if (descriptor < 0) return 0;
    for (size_t i = 0; i < CREATEOS_LUA_FILE_SLOTS; i++) {
        if (lua_files[i].descriptor < 0) {
            lua_files[i].descriptor = descriptor;
            lua_files[i].eof = 0;
            lua_files[i].error = 0;
            lua_files[i].has_pushback = 0;
            strcpy(lua_files[i].temporary_path, path);
            return &lua_files[i];
        }
    }
    sys_close(descriptor);
    sys_unlink(path);
    return 0;
}

int fputc(int character, FILE* stream)
{
    unsigned char byte = (unsigned char)character;
    return fwrite(&byte, 1, 1, stream) == 1 ? byte : EOF;
}

int fputs(const char* string, FILE* stream)
{
    size_t length;
    if (!string) return EOF;
    length = strlen(string);
    return fwrite(string, 1, length, stream) == length ? 0 : EOF;
}

char* fgets(char* string, int size, FILE* stream)
{
    int count = 0;
    if (!string || size <= 0 || !stream) return 0;
    while (count + 1 < size) {
        int character = fgetc(stream);
        if (character == EOF) break;
        string[count++] = (char)character;
        if (character == '\n') break;
    }
    if (!count) return 0;
    string[count] = 0;
    return string;
}

int vfprintf(FILE* stream, const char* format, va_list arguments)
{
    va_list count_arguments;
    va_list format_arguments;
    char* buffer;
    int length;
    int result;
    __builtin_va_copy(count_arguments, arguments);
    length = vsnprintf(0, 0, format, count_arguments);
    __builtin_va_end(count_arguments);
    if (length < 0 || (size_t)length >= 1024u * 1024u) return -1;
    buffer = malloc((size_t)length + 1);
    if (!buffer) return -1;
    __builtin_va_copy(format_arguments, arguments);
    result = vsnprintf(buffer, (size_t)length + 1, format,
                       format_arguments);
    __builtin_va_end(format_arguments);
    if (result != length) {
        free(buffer);
        return -1;
    }
    result = fwrite(buffer, 1, (size_t)length, stream) == (size_t)length ?
             length : -1;
    free(buffer);
    return result;
}

int fprintf(FILE* stream, const char* format, ...)
{
    va_list arguments;
    int result;
    va_start(arguments, format);
    result = vfprintf(stream, format, arguments);
    va_end(arguments);
    return result;
}

FILE* freopen(const char* path, const char* mode, FILE* stream)
{
    if (!path || !mode || !stream || stream->standard) return 0;
    if (stream->descriptor >= 0)
        fclose(stream);
    return fopen(path, mode);
}

char* strerror(int error)
{
    static char message[24];
    snprintf(message, sizeof(message), "error %d", error);
    return message;
}

unsigned int createos_lua_randomize_pivot(void)
{
    unsigned int value = 0;
    if (sys_random(&value, sizeof(value)) != (int)sizeof(value))
        return (unsigned int)sys_ticks();
    return value;
}
