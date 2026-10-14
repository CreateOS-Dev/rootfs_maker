#ifndef CREATEOS_TIME_H
#define CREATEOS_TIME_H

#include <stddef.h>
#include <stdint.h>

typedef int64_t time_t;
typedef int64_t clock_t;

#define CLOCKS_PER_SEC 1000000000LL

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

time_t time(time_t* result);
struct tm* gmtime_r(const time_t* value, struct tm* result);
struct tm* localtime_r(const time_t* value, struct tm* result);
struct tm* gmtime(const time_t* value);
struct tm* localtime(const time_t* value);
time_t mktime(struct tm* value);
double difftime(time_t end, time_t beginning);
size_t strftime(char* output, size_t capacity, const char* format,
                const struct tm* value);
clock_t clock(void);

#endif
