#ifndef CREATEOS_TCC_SYS_TIME_H
#define CREATEOS_TCC_SYS_TIME_H

struct timeval {
    long tv_sec;
    long tv_usec;
};

struct timezone;
int gettimeofday(struct timeval* value, struct timezone* zone);

#endif
