#ifndef CREATEOS_CURL_TLS_H
#define CREATEOS_CURL_TLS_H

#include <stddef.h>

typedef struct curl_tls_client curl_tls_client_t;

typedef enum {
    CURL_TLS_OK = 0,
    CURL_TLS_ERROR_MEMORY = -1,
    CURL_TLS_ERROR_TIME = -2,
    CURL_TLS_ERROR_ENTROPY = -3,
    CURL_TLS_ERROR_TRUST_STORE = -4,
    CURL_TLS_ERROR_HANDSHAKE = -5
} curl_tls_result_t;

curl_tls_result_t curl_tls_open(curl_tls_client_t **client, int tcp_handle,
                                const char *hostname);
int curl_tls_write(curl_tls_client_t *client, const void *buffer,
                   size_t length);
int curl_tls_read(curl_tls_client_t *client, void *buffer, size_t capacity);
const char *curl_tls_version(const curl_tls_client_t *client);
void curl_tls_close(curl_tls_client_t *client);

#endif
