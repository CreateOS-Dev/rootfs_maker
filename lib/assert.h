#ifndef CREATEOS_ASSERT_H
#define CREATEOS_ASSERT_H

#define assert(expression) \
    ((expression) ? (void)0 : __builtin_trap())

#endif
