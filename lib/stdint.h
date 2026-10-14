#ifndef CREATEOS_STDINT_H
#define CREATEOS_STDINT_H

#include <stddef.h>

#ifndef __int8_t_defined
#define __int8_t_defined
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
#ifdef __LP64__
typedef signed long int64_t;
typedef unsigned long uint64_t;
#else
typedef signed long long int64_t;
typedef unsigned long long uint64_t;
#endif
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
#endif

#ifndef __intptr_t_defined
#define __intptr_t_defined
typedef __PTRDIFF_TYPE__ intptr_t;
typedef __SIZE_TYPE__ uintptr_t;
#endif

typedef int8_t int_least8_t;
typedef int16_t int_least16_t;
typedef int32_t int_least32_t;
typedef int64_t int_least64_t;
typedef uint8_t uint_least8_t;
typedef uint16_t uint_least16_t;
typedef uint32_t uint_least32_t;
typedef uint64_t uint_least64_t;

typedef int32_t int_fast8_t;
typedef int64_t int_fast16_t;
typedef int64_t int_fast32_t;
typedef int64_t int_fast64_t;
typedef uint32_t uint_fast8_t;
typedef uint64_t uint_fast16_t;
typedef uint64_t uint_fast32_t;
typedef uint64_t uint_fast64_t;

typedef int64_t intmax_t;
typedef uint64_t uintmax_t;

#define INT8_MIN (-128)
#define INT16_MIN (-32767 - 1)
#define INT32_MIN (-2147483647 - 1)
#define INT64_MIN (-9223372036854775807L - 1)
#define INT8_MAX 127
#define INT16_MAX 32767
#define INT32_MAX 2147483647
#define INT64_MAX 9223372036854775807L
#define UINT8_MAX 255U
#define UINT16_MAX 65535U
#define UINT32_MAX 4294967295U
#define UINT64_MAX 18446744073709551615UL
#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

#define INT8_C(value) value
#define INT16_C(value) value
#define INT32_C(value) value
#define INT64_C(value) value##L
#define UINT8_C(value) value##U
#define UINT16_C(value) value##U
#define UINT32_C(value) value##U
#define UINT64_C(value) value##UL
#define INTMAX_C(value) value##L
#define UINTMAX_C(value) value##UL

#endif
