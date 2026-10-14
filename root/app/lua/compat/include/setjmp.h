#ifndef CREATEOS_LUA_SETJMP_H
#define CREATEOS_LUA_SETJMP_H

typedef void* jmp_buf[5];

#define setjmp(environment) __builtin_setjmp(environment)
#define longjmp(environment, value) \
    ((void)(value), __builtin_longjmp((environment), 1))

#endif
