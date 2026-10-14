#include "user_syscall.h"
#include "curl_tls.h"
#include "curl_http.h"
#include "errno.h"

#define CURL_DESTINATION_MAX 256

static int parse_ipv4(const char* text, uint8_t address[4])
{
    const char* cursor = text;
    for (int part = 0; part < 4; part++) {
        unsigned int value = 0;
        unsigned int digits = 0;
        while (*cursor >= '0' && *cursor <= '9') {
            unsigned int digit = (unsigned int)(*cursor++ - '0');
            if (value > (255U - digit) / 10U) return 0;
            value = value * 10U + digit;
            digits++;
        }
        if (!digits) return 0;
        address[part] = (uint8_t)value;
        if (part < 3) {
            if (*cursor++ != '.') return 0;
        } else if (*cursor) {
            return 0;
        }
    }
    return 1;
}

static int find_http_body(const uint8_t* response, size_t length,
                          size_t* body_offset)
{
    size_t separator = 0;

    if (length >= 8 &&
        memcmp(response, "HTTP/1.", 7) != 0)
        return -1;
    if (length >= 8 && response[7] != '0' && response[7] != '1')
        return -1;
    for (size_t i = 0; i + 3 < length; i++) {
        if (response[i] == '\r' && response[i + 1] == '\n' &&
            response[i + 2] == '\r' && response[i + 3] == '\n') {
            separator = i;
            break;
        }
    }
    if (!separator) {
        if (length >= CURL_HTTP_RESPONSE_MAX) return -1;
        return 0;
    }
    if (separator < 12 || response[8] != ' ' ||
        response[9] < '1' || response[9] > '5' ||
        response[10] < '0' || response[10] > '9' ||
        response[11] < '0' || response[11] > '9')
        return -1;
    *body_offset = separator + 4;
    return 1;
}

static int select_interface(void)
{
    int count = sys_net_count();
    if (count < 0) return -2;
    for (int i = 0; i < count; i++) {
        netdev_info_t info;
        if (sys_net_info(i, &info) == 0 && info.link_up &&
            info.dhcp_state == NETDEV_DHCP_BOUND)
            return i;
    }
    return -1;
}

static int valid_hostname(const char* host, size_t length)
{
    size_t label_length = 0;

    if (!length || length > NET_DNS_MAX_NAME_LENGTH) return 0;
    if (host[0] == '.' || host[0] == '-' || host[length - 1] == '.' ||
        host[length - 1] == '-')
        return 0;
    for (size_t i = 0; i < length; i++) {
        char ch = host[i];
        if (ch == '.') {
            if (label_length == 0 || label_length > 63 ||
                host[i - 1] == '-')
                return 0;
            label_length = 0;
            continue;
        }
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '-'))
            return 0;
        if (label_length == 0 && ch == '-') return 0;
        label_length++;
    }
    return label_length > 0 && label_length <= 63 && host[length - 1] != '-';
}

static int confirm_action(const char* prompt)
{
    int accepted;
    sys_puts(prompt, 0x0E);
    char choice = sys_getc();
    accepted = choice == 'y' || choice == 'Y';
    sys_putc(choice, 0x0F);
    sys_puts("\n", 0x0F);
    while (choice != '\n' && choice != '\r')
        choice = sys_getc();
    return accepted;
}

static int save_download(uint8_t* response, size_t response_length,
                         int connection_closed, const char* relative_path)
{
    size_t body_offset;
    size_t body_length;
    int status_code;
    char destination[CURL_DESTINATION_MAX];
    user_t user;
    fs_stat_t existing;
    int stat_result;
    size_t home_length;
    size_t path_length;
    int result = 1;

    if (!curl_http_parse_response(response, response_length, connection_closed,
                                  &body_offset, &body_length, &status_code)) {
        sys_puts("curl: invalid, incomplete, oversized, or non-success HTTP response; file discarded\n",
                 0x0C);
        goto done;
    }
    if (!curl_http_valid_relative_path(relative_path)) {
        sys_puts("curl: save path must be relative to your home and cannot contain . or .. components\n",
                 0x0C);
        goto done;
    }
    if (sys_user_current(&user) != 0 || !user.home[0]) {
        sys_puts("curl: unable to determine the current user's home directory\n",
                 0x0C);
        goto done;
    }
    home_length = 0;
    while (home_length < sizeof(user.home) && user.home[home_length])
        home_length++;
    if (home_length == sizeof(user.home)) {
        sys_puts("curl: current user's home directory is invalid\n", 0x0C);
        goto done;
    }
    path_length = strlen(relative_path);
    if (home_length + 1 + path_length >= sizeof(destination)) {
        sys_puts("curl: save path is too long\n", 0x0C);
        goto done;
    }
    memcpy(destination, user.home, home_length);
    destination[home_length] = '/';
    memcpy(destination + home_length + 1, relative_path, path_length + 1);

    stat_result = sys_stat(destination, &existing);
    if (stat_result == 0) {
        if (existing.type != FS_TYPE_FILE) {
            sys_puts("curl: destination exists and is not a regular file\n",
                     0x0C);
            goto done;
        }
        if (!confirm_action("Destination exists. Replace it? [y/N] ")) {
            sys_puts("curl: existing file kept unchanged\n", 0x0E);
            goto done;
        }
    } else if (stat_result != -ENOENT) {
        sys_puts("curl: cannot safely inspect the destination\n", 0x0C);
        goto done;
    }

    sys_puts("Untrusted download held in memory; it has not been written or executed.\n",
             0x0E);
    if (!confirm_action("Save this response into your home directory? [y/N] ")) {
        sys_puts("curl: download discarded\n", 0x0E);
        goto done;
    }
    if (sys_file_replace(destination, response + body_offset, body_length) != 0) {
        sys_puts("curl: could not save the download; no success is reported\n",
                 0x0C);
        goto done;
    }
    sys_puts("curl: saved to ", 0x0A);
    sys_puts(destination, 0x0A);
    sys_puts("\n", 0x0A);
    result = 0;

done:
    memset(response, 0, CURL_HTTP_RESPONSE_MAX);
    return result;
}

static int parse_url(const char* url, uint16_t* port, const char** path,
                     char host[NET_DNS_MAX_NAME_LENGTH + 1], int* use_tls)
{
    const char* host_start;
    const char* host_end;
    size_t host_length;
    unsigned int parsed_port;

    if (strncmp(url, "http://", 7) == 0) {
        *use_tls = 0;
        parsed_port = 80;
        host_start = url + 7;
    } else if (strncmp(url, "https://", 8) == 0) {
        *use_tls = 1;
        parsed_port = 443;
        host_start = url + 8;
    } else {
        return 0;
    }
    host_end = host_start;
    while (*host_end && *host_end != '/' && *host_end != ':') host_end++;
    host_length = (size_t)(host_end - host_start);
    if (!valid_hostname(host_start, host_length)) return 0;
    memcpy(host, host_start, host_length);
    host[host_length] = 0;

    if (*host_end == ':') {
        const char* digit = host_end + 1;
        if (*digit < '0' || *digit > '9') return 0;
        parsed_port = 0;
        while (*digit >= '0' && *digit <= '9') {
            unsigned int value = (unsigned int)(*digit++ - '0');
            if (parsed_port > (65535U - value) / 10U) return 0;
            parsed_port = parsed_port * 10U + value;
        }
        if (!parsed_port || (*digit && *digit != '/')) return 0;
        host_end = digit;
    }
    if (*host_end && *host_end != '/') return 0;
    *path = *host_end ? host_end : "/";
    for (const char* p = *path; *p; p++)
        if ((unsigned char)*p < 0x21 || *p == '\r' || *p == '\n')
            return 0;
    *port = (uint16_t)parsed_port;
    return 1;
}

int main(int argc, char* argv[])
{
    uint8_t address[4];
    uint8_t response[CURL_HTTP_RESPONSE_MAX];
    char host[NET_DNS_MAX_NAME_LENGTH + 1];
    char request[NET_TCP_MAX_REQUEST];
    const char* path;
    const char* url;
    const char* save_path = NULL;
    uint16_t port;
    int interface_index;
    int tcp_handle;
    int request_length;
    int written;
    int use_tls;
    int download;
    int response_complete = 0;
    uint64_t response_length = 0;
    curl_tls_client_t *tls = NULL;
    uint8_t response_chunk[2048];
    size_t display_header_length = 0;
    int display_headers_complete = 0;

    download = argc == 4 && strcmp(argv[1], "-o") == 0;
    if (download) {
        save_path = argv[2];
        url = argv[3];
    } else if (argc == 2) {
        url = argv[1];
    } else {
        sys_puts("Usage: curl URL | curl -o HOME_RELATIVE_PATH URL\n", 0x0E);
        sys_puts("HTTPS requires a valid system clock and trusted CA bundle.\n",
                 0x0E);
        return 2;
    }
    {
        if (!parse_url(url, &port, &path, host, &use_tls)) {
            sys_puts("curl: expected http[s]://host[:port]/path\n", 0x0C);
            return 2;
        }
        interface_index = select_interface();
        if (interface_index < 0) {
            sys_puts("curl: no configured, link-up network interface\n", 0x0C);
            return 1;
        }
        if (!parse_ipv4(host, address) &&
            sys_net_resolve(interface_index, host, address) != 1) {
            sys_puts("curl: DNS lookup failed for host\n", 0x0C);
            return 1;
        }
        request_length = snprintf(request, sizeof(request),
                                  "GET %s HTTP/1.0\r\nHost: %s\r\n"
                                  "Connection: close\r\n\r\n", path, host);
        if (request_length <= 0 ||
            (size_t)request_length >= sizeof(request)) {
            sys_puts("curl: request URL is too long\n", 0x0C);
            return 2;
        }

        tcp_handle = sys_net_tcp_stream_open(interface_index, address, port,
                                             15000);
        if (tcp_handle < 0) {
            sys_puts("curl: TCP connection failed\n", 0x0C);
            return 1;
        }

        if (use_tls) {
            curl_tls_result_t tls_result =
                curl_tls_open(&tls, tcp_handle, host);
            if (tls_result != CURL_TLS_OK) {
                sys_net_tcp_stream_close(tcp_handle);
                switch (tls_result) {
                case CURL_TLS_ERROR_MEMORY:
                    sys_puts("curl: not enough memory for TLS\n", 0x0C);
                    break;
                case CURL_TLS_ERROR_TIME:
                    sys_puts("curl: system clock is invalid; HTTPS stopped\n",
                             0x0C);
                    break;
                case CURL_TLS_ERROR_ENTROPY:
                    sys_puts("curl: hardware entropy unavailable; HTTPS stopped\n",
                             0x0C);
                    break;
                case CURL_TLS_ERROR_TRUST_STORE:
                    sys_puts("curl: trusted CA bundle missing or invalid\n",
                             0x0C);
                    break;
                default:
                    sys_puts("curl: TLS handshake or certificate validation failed\n",
                             0x0C);
                    break;
                }
                return 1;
            }
        }

        written = 0;
        while (written < request_length) {
            int sent;
            if (tls) {
                sent = curl_tls_write(tls, request + written,
                                      (size_t)(request_length - written));
            } else {
                sent = sys_net_tcp_stream_write(
                    tcp_handle, request + written,
                    (uint32_t)(request_length - written), 15000);
            }
            if (sent <= 0) {
                curl_tls_close(tls);
                sys_net_tcp_stream_close(tcp_handle);
                sys_puts("curl: failed to send HTTP request\n", 0x0C);
                return 1;
            }
            written += sent;
        }

            while (!download || response_length < sizeof(response)) {
            int received;
            if (tls) {
                received = curl_tls_read(tls,
                                         download ? response + response_length :
                                                    response_chunk,
                                         download ?
                                             sizeof(response) - response_length :
                                             sizeof(response_chunk));
            } else {
                received = sys_net_tcp_stream_read(
                    tcp_handle,
                    download ? response + response_length : response_chunk,
                    download ? sizeof(response) - response_length :
                               sizeof(response_chunk),
                    15000);
            }
            if (received < 0) {
                curl_tls_close(tls);
                sys_net_tcp_stream_close(tcp_handle);
                if (download)
                    memset(response, 0, sizeof(response));
                sys_puts(tls ? "curl: TLS response was interrupted or truncated\n" :
                               "curl: failed while receiving HTTP response\n",
                         0x0C);
                return 1;
            }
            if (received == 0) {
                response_complete = 1;
                break;
            }
            if (!download) {
                size_t chunk_offset = 0;
                if (!display_headers_complete) {
                    while (chunk_offset < (size_t)received &&
                           !display_headers_complete) {
                        size_t body_offset = 0;
                        int header_status;
                        if (display_header_length >= sizeof(response)) {
                            curl_tls_close(tls);
                            sys_net_tcp_stream_close(tcp_handle);
                            sys_puts("curl: HTTP response headers are too large\n",
                                     0x0C);
                            return 1;
                        }
                        response[display_header_length++] =
                            response_chunk[chunk_offset++];
                        header_status = find_http_body(response,
                            display_header_length, &body_offset);
                        if (header_status < 0) {
                            curl_tls_close(tls);
                            sys_net_tcp_stream_close(tcp_handle);
                            sys_puts("curl: invalid HTTP response headers\n",
                                     0x0C);
                            return 1;
                        }
                        if (header_status > 0) {
                            display_headers_complete = 1;
                            chunk_offset -= display_header_length - body_offset;
                        }
                    }
                }
                if (display_headers_complete &&
                    chunk_offset < (size_t)received &&
                    sys_write(1, response_chunk + chunk_offset,
                              (size_t)received - chunk_offset) !=
                        (ssize_t)((size_t)received - chunk_offset)) {
                    curl_tls_close(tls);
                    sys_net_tcp_stream_close(tcp_handle);
                    sys_puts("curl: failed to write response to console\n",
                             0x0C);
                    return 1;
                }
            }
            response_length += (uint64_t)received;
        }

        if (download && response_length == sizeof(response) &&
            !response_complete) {
            uint8_t overflow_byte;
            int overflow_read = tls ?
                curl_tls_read(tls, &overflow_byte, sizeof(overflow_byte)) :
                sys_net_tcp_stream_read(tcp_handle, &overflow_byte,
                                        sizeof(overflow_byte), 15000);
            if (overflow_read < 0) {
                curl_tls_close(tls);
                sys_net_tcp_stream_close(tcp_handle);
                memset(response, 0, sizeof(response));
                sys_puts("curl: response exceeded the safe download limit or was incomplete\n",
                         0x0C);
                return 1;
            }
            if (overflow_read > 0) {
                curl_tls_close(tls);
                sys_net_tcp_stream_close(tcp_handle);
                memset(response, 0, sizeof(response));
                sys_puts("curl: download exceeds the 4 KiB safe limit and was discarded\n",
                         0x0C);
                return 1;
            }
            response_complete = 1;
        }
        if (!download && !display_headers_complete) {
            curl_tls_close(tls);
            sys_net_tcp_stream_close(tcp_handle);
            sys_puts("curl: connection closed before HTTP headers completed\n",
                     0x0C);
            return 1;
        }
        if (tls)
            sys_puts("\n[curl: encrypted connection ", 0x0A);
        if (tls)
            sys_puts(curl_tls_version(tls), 0x0A);
        if (tls)
            sys_puts("; certificate and hostname verified]\n", 0x0A);
        curl_tls_close(tls);
    }
    if (sys_net_tcp_stream_close(tcp_handle) < 0) {
        sys_puts("curl: failed to close TCP connection\n", 0x0C);
        if (download)
            memset(response, 0, sizeof(response));
        return 1;
    }
    if (download)
        return save_download(response, response_length, response_complete,
                             save_path);
    if (download && response_length == sizeof(response))
        sys_puts("\ncurl: response truncated at the userland buffer limit\n",
                 0x0E);
    return 0;
}
