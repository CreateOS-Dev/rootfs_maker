#ifndef CREATEOS_TCC_SETJMP_H
#define CREATEOS_TCC_SETJMP_H

typedef void* jmp_buf[5];

#define setjmp(environment) __builtin_setjmp(environment)
#define longjmp(environment, value) \
    (__builtin_longjmp((environment), 1), (void)(value))

#endif
