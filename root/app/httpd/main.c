#include "user_syscall.h"
#include "createos/http.h"
#include "createos/network.h"

#include <string.h>

#define HTTPD_PORT 8080
#define HTTPD_REQUEST_CAPACITY \
    (CREATEOS_HTTP_HEADER_BYTES_MAX + CREATEOS_HTTP_REQUEST_LINE_MAX)
#define HTTPD_READ_CHUNK 1024
#define HTTPD_IO_TIMEOUT_MS 2000

static const unsigned char response_ok_header[] =
    "HTTP/1.1 200 OK\r\n"
    "Connection: close\r\n"
    "Content-Type: text/plain; charset=utf-8\r\n"
    "Content-Length: 33\r\n\r\n";
static const unsigned char response_ok_body[] =
    "CreateOS HTTP server is running.\n";
static const unsigned char response_not_found[] =
    "HTTP/1.1 404 Not Found\r\n"
    "Connection: close\r\n"
    "Content-Length: 0\r\n\r\n";
static const unsigned char response_bad_request[] =
    "HTTP/1.1 400 Bad Request\r\n"
    "Connection: close\r\n"
    "Content-Length: 0\r\n\r\n";
static const unsigned char response_method_not_allowed[] =
    "HTTP/1.1 405 Method Not Allowed\r\n"
    "Allow: GET\r\n"
    "Connection: close\r\n"
    "Content-Length: 0\r\n\r\n";
static const unsigned char response_timeout[] =
    "HTTP/1.1 408 Request Timeout\r\n"
    "Connection: close\r\n"
    "Content-Length: 0\r\n\r\n";

static int write_all(int stream, const unsigned char* data, size_t length)
{
    size_t written = 0;
    while (written < length) {
        int result = createos_net_tcp_write(
            stream, data + written, (uint32_t)(length - written),
            HTTPD_IO_TIMEOUT_MS);
        if (result <= 0 || (size_t)result > length - written)
            return 0;
        written += (size_t)result;
    }
    return 1;
}

static int interface_has_ipv4(const netdev_info_t* info)
{
    uint8_t address_bits = 0;
    for (size_t i = 0; i < sizeof(info->ipv4_address); i++)
        address_bits |= info->ipv4_address[i];
    return info->link_up && address_bits != 0;
}

static int find_ready_interface(netdev_info_t* selected)
{
    int count = sys_net_count();
    if (count <= 0)
        return -1;
    for (int index = 0; index < count; index++) {
        netdev_info_t info;
        if (sys_net_info(index, &info) == 0 && interface_has_ipv4(&info)) {
            *selected = info;
            return index;
        }
    }
    return -1;
}

static int respond_to_request(int stream, const unsigned char* data,
                              size_t length)
{
    createos_http_request_t request;
    int parsed = createos_http_parse_request(data, length, &request);
    if (parsed != 1)
        return 0;
    if (request.body_length || request.content_length)
        return write_all(stream, response_bad_request,
                         sizeof(response_bad_request) - 1);
    if (request.method_length != 3 ||
        memcmp(data + request.method_offset, "GET", 3) != 0)
        return write_all(stream, response_method_not_allowed,
                         sizeof(response_method_not_allowed) - 1);
    if (!((request.target_length == 1 &&
           data[request.target_offset] == '/') ||
          (request.target_length == 7 &&
           memcmp(data + request.target_offset, "/health", 7) == 0)))
        return write_all(stream, response_not_found,
                         sizeof(response_not_found) - 1);
    return write_all(stream, response_ok_header,
                     sizeof(response_ok_header) - 1) &&
           write_all(stream, response_ok_body,
                     sizeof(response_ok_body) - 1);
}

static int serve_one_request(int stream)
{
    unsigned char request_data[HTTPD_REQUEST_CAPACITY];
    size_t length = 0;

    for (;;) {
        createos_http_request_t request;
        int parsed;
        int received;

        parsed = createos_http_parse_request(request_data, length, &request);
        if (parsed < 0)
            return write_all(stream, response_bad_request,
                             sizeof(response_bad_request) - 1);
        if (parsed > 0)
            return respond_to_request(stream, request_data, length);
        if (length == sizeof(request_data))
            return write_all(stream, response_bad_request,
                             sizeof(response_bad_request) - 1);

        size_t capacity = sizeof(request_data) - length;
        if (capacity > HTTPD_READ_CHUNK)
            capacity = HTTPD_READ_CHUNK;
        received = createos_net_tcp_read(
            stream, request_data + length, (uint32_t)capacity,
            HTTPD_IO_TIMEOUT_MS);
        if (received <= 0)
            return write_all(stream, response_timeout,
                             sizeof(response_timeout) - 1);
        length += (size_t)received;
    }
}

int main(int argc, char** argv)
{
    netdev_info_t interface;
    int interface_index;
    int listener;
    int stream;
    int served;
    (void)argv;

    if (argc != 1) {
        sys_puts("Usage: open httpd.APPL\n", 0x0E);
        return 2;
    }
    interface_index = find_ready_interface(&interface);
    if (interface_index < 0) {
        sys_puts("httpd: no linked network interface has an IPv4 address\n",
                 0x0C);
        return 1;
    }
    listener = createos_net_tcp_listen(interface_index, HTTPD_PORT);
    if (listener < 0) {
        sys_puts("httpd: cannot listen on TCP port 8080; administrator access and a free port are required\n",
                 0x0C);
        return 1;
    }

    sys_puts("httpd: listening on TCP port 8080 for one request\n", 0x0A);
    stream = createos_net_tcp_accept(listener, 30000);
    if (stream < 0) {
        (void)createos_net_tcp_listener_close(listener);
        sys_puts("httpd: no connection accepted before timeout\n", 0x0C);
        return 1;
    }

    served = serve_one_request(stream);
    if (createos_net_tcp_close(stream) < 0)
        served = 0;
    if (createos_net_tcp_listener_close(listener) < 0)
        served = 0;
    sys_puts(served ? "httpd: request served; listener closed\n"
                    : "httpd: request failed; listener closed\n",
             served ? 0x0A : 0x0C);
    return served ? 0 : 1;
}
