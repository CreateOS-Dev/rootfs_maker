#ifndef CREATEOS_TCC_COMPAT_H
#define CREATEOS_TCC_COMPAT_H

#include "include/errno.h"
#include "include/math.h"
#include "include/setjmp.h"
#include "include/stdio.h"
#include "include/stdlib.h"
#include "include/string.h"
#include "include/time.h"
#include "include/unistd.h"
#include "include/sys/time.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define printf createos_tcc_printf

#endif
