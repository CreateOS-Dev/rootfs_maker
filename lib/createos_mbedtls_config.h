#ifndef CREATEOS_MBEDTLS_CONFIG_H
#define CREATEOS_MBEDTLS_CONFIG_H

#include <stdint.h>
#include "mbedtls/mbedtls_config.h"

#define MBEDTLS_HAVE_TIME
#undef MBEDTLS_FS_IO
#undef MBEDTLS_NET_C
#undef MBEDTLS_TIMING_C

#endif
