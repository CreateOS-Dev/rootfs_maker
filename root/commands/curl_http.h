#ifndef CREATEOS_CURL_HTTP_H
#define CREATEOS_CURL_HTTP_H

#include <stddef.h>
#include <stdint.h>

#define CURL_HTTP_RESPONSE_MAX 4096
#define CURL_HTTP_SAVE_PATH_MAX 128

int curl_http_parse_response(const uint8_t* response, size_t response_length,
                             int connection_closed, size_t* body_offset,
                             size_t* body_length, int* status_code);
int curl_http_valid_relative_path(const char* path);

#endif
