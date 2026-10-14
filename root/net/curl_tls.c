#include "curl_tls.h"

#include "user_syscall.h"

#include <mbedtls/platform.h>
#include <mbedtls/platform_time.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TLS_CA_BUNDLE_PATH "/etc/ssl/cert.pem"
#define TLS_CA_BUNDLE_MAX_SIZE (512U * 1024U)
#define TLS_IO_TIMEOUT_MS 15000U

struct curl_tls_client {
    int tcp_handle;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config config;
    mbedtls_x509_crt ca_chain;
};

static int is_leap_year(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static int month_days(int year, int month)
{
    static const uint8_t days[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if (month < 1 || month > 12) return 0;
    if (month == 2 && is_leap_year(year)) return 29;
    return days[month - 1];
}

static int rtc_to_epoch(const rtc_time_t *rtc, mbedtls_time_t *epoch)
{
    int year;
    int days = 0;

    if (!rtc || !epoch || rtc->year > 99 || rtc->month < 1 ||
        rtc->month > 12 || rtc->day < 1 || rtc->hour > 23 ||
        rtc->minute > 59 || rtc->second > 59)
        return 0;

    year = 2000 + rtc->year;
    if (year < 2020 || rtc->day > month_days(year, rtc->month))
        return 0;

    for (int current_year = 1970; current_year < year; current_year++)
        days += is_leap_year(current_year) ? 366 : 365;
    for (int month = 1; month < rtc->month; month++)
        days += month_days(year, month);
    days += rtc->day - 1;

    *epoch = (mbedtls_time_t)days * 86400 +
             (mbedtls_time_t)rtc->hour * 3600 +
             (mbedtls_time_t)rtc->minute * 60 + rtc->second;
    return 1;
}

static int read_epoch(mbedtls_time_t *epoch)
{
    rtc_time_t rtc;
    return sys_time(&rtc) == 0 && rtc_to_epoch(&rtc, epoch);
}

static mbedtls_time_t createos_time(mbedtls_time_t *result)
{
    mbedtls_time_t now;
    if (!read_epoch(&now)) return (mbedtls_time_t)-1;
    if (result) *result = now;
    return now;
}

static int epoch_to_tm(mbedtls_time_t epoch, struct tm *result)
{
    static const int month_offsets[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    int64_t days;
    int year = 1970;
    int day_of_year;
    int month;

    if (!result || epoch < 0) return 0;
    days = epoch / 86400;
    if (days > 47481) return 0;

    while (days >= (is_leap_year(year) ? 366 : 365)) {
        days -= is_leap_year(year) ? 366 : 365;
        year++;
    }
    if (year > 2099) return 0;

    day_of_year = (int)days;
    for (month = 1; month <= 12; month++) {
        int length = month_days(year, month);
        if (day_of_year < length) break;
        day_of_year -= length;
    }
    if (month > 12) return 0;

    result->tm_sec = (int)(epoch % 60);
    result->tm_min = (int)((epoch / 60) % 60);
    result->tm_hour = (int)((epoch / 3600) % 24);
    result->tm_mday = day_of_year + 1;
    result->tm_mon = month - 1;
    result->tm_year = year - 1900;
    result->tm_wday = (int)((epoch / 86400 + 4) % 7);
    result->tm_yday = month_offsets[month - 1] + day_of_year;
    if (month > 2 && is_leap_year(year)) result->tm_yday++;
    result->tm_isdst = 0;
    return 1;
}

time_t time(time_t *result)
{
    return (time_t)createos_time((mbedtls_time_t *)result);
}

struct tm *gmtime_r(const time_t *value, struct tm *result)
{
    if (!value || !epoch_to_tm((mbedtls_time_t)*value, result)) return NULL;
    return result;
}

struct tm *mbedtls_platform_gmtime_r(const mbedtls_time_t *value,
                                     struct tm *result)
{
    if (!value || !epoch_to_tm(*value, result)) return NULL;
    return result;
}

mbedtls_ms_time_t mbedtls_ms_time(void)
{
    unsigned long ticks = sys_ticks();
    if (ticks > (unsigned long)(INT64_MAX / 10)) return 0;
    return (mbedtls_ms_time_t)ticks * 10;
}

int mbedtls_platform_get_entropy(psa_driver_get_entropy_flags_t flags,
                                 size_t *estimate_bits,
                                 unsigned char *output, size_t output_size)
{
    if (flags != PSA_DRIVER_GET_ENTROPY_FLAGS_NONE)
        return PSA_ERROR_NOT_SUPPORTED;
    if (!estimate_bits || !output || output_size == 0 ||
        output_size > (size_t)-1 / 8)
        return PSA_ERROR_INSUFFICIENT_ENTROPY;

    for (size_t offset = 0; offset < output_size;) {
        size_t chunk = output_size - offset;
        int result;
        if (chunk > 256) chunk = 256;
        result = sys_random(output + offset, chunk);
        if (result != (int)chunk) {
            memset(output, 0, output_size);
            *estimate_bits = 0;
            return PSA_ERROR_INSUFFICIENT_ENTROPY;
        }
        offset += chunk;
    }
    *estimate_bits = output_size * 8;
    return PSA_SUCCESS;
}

static int tcp_send(void *context, const unsigned char *buffer, size_t length)
{
    curl_tls_client_t *client = (curl_tls_client_t *)context;
    size_t sent = 0;

    while (sent < length) {
        size_t chunk = length - sent;
        int result;
        if (chunk > NET_TCP_STREAM_IO_MAX) chunk = NET_TCP_STREAM_IO_MAX;
        result = sys_net_tcp_stream_write(client->tcp_handle, buffer + sent,
                                          (uint32_t)chunk,
                                          TLS_IO_TIMEOUT_MS);
        if (result <= 0) return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        sent += (size_t)result;
    }
    return (int)sent;
}

static int tcp_receive(void *context, unsigned char *buffer, size_t capacity)
{
    curl_tls_client_t *client = (curl_tls_client_t *)context;
    size_t chunk = capacity;
    int result;

    if (chunk > NET_TCP_STREAM_IO_MAX) chunk = NET_TCP_STREAM_IO_MAX;
    result = sys_net_tcp_stream_read(client->tcp_handle, buffer,
                                     (uint32_t)chunk, TLS_IO_TIMEOUT_MS);
    return result < 0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : result;
}

static int load_trust_store(mbedtls_x509_crt *chain)
{
    fs_stat_t status;
    char *contents;
    size_t offset = 0;
    int fd;
    int result = 0;

    if (sys_stat(TLS_CA_BUNDLE_PATH, &status) < 0 || status.size == 0 ||
        status.size > TLS_CA_BUNDLE_MAX_SIZE)
        return 0;

    contents = (char *)malloc((size_t)status.size + 1);
    if (!contents) return 0;

    fd = sys_open(TLS_CA_BUNDLE_PATH, O_RDONLY, 0);
    if (fd < 0) {
        free(contents);
        return 0;
    }
    while (offset < status.size) {
        ssize_t count = sys_read(fd, contents + offset, status.size - offset);
        if (count <= 0) goto cleanup;
        offset += (size_t)count;
    }
    contents[offset] = '\0';
    result = mbedtls_x509_crt_parse(chain,
                                    (const unsigned char *)contents,
                                    offset + 1) == 0;

cleanup:
    if (sys_close(fd) < 0) result = 0;
    free(contents);
    return result;
}

void curl_tls_close(curl_tls_client_t *client)
{
    if (!client) return;
    mbedtls_ssl_free(&client->ssl);
    mbedtls_ssl_config_free(&client->config);
    mbedtls_x509_crt_free(&client->ca_chain);
    free(client);
}

curl_tls_result_t curl_tls_open(curl_tls_client_t **client_out,
                                int tcp_handle, const char *hostname)
{
    curl_tls_client_t *client;
    mbedtls_time_t now;
    psa_status_t psa_result;
    int result;

    if (!client_out || !hostname) return CURL_TLS_ERROR_HANDSHAKE;
    *client_out = NULL;
    if (!read_epoch(&now)) return CURL_TLS_ERROR_TIME;

    client = (curl_tls_client_t *)calloc(1, sizeof(*client));
    if (!client) return CURL_TLS_ERROR_MEMORY;
    client->tcp_handle = tcp_handle;
    mbedtls_ssl_init(&client->ssl);
    mbedtls_ssl_config_init(&client->config);
    mbedtls_x509_crt_init(&client->ca_chain);

    if (mbedtls_platform_set_time(createos_time) != 0) {
        curl_tls_close(client);
        return CURL_TLS_ERROR_TIME;
    }
    psa_result = psa_crypto_init();
    if (psa_result != PSA_SUCCESS) {
        curl_tls_close(client);
        return CURL_TLS_ERROR_ENTROPY;
    }
    if (!load_trust_store(&client->ca_chain)) {
        curl_tls_close(client);
        return CURL_TLS_ERROR_TRUST_STORE;
    }

    result = mbedtls_ssl_config_defaults(
        &client->config, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT);
    if (result != 0) {
        curl_tls_close(client);
        return CURL_TLS_ERROR_HANDSHAKE;
    }
    mbedtls_ssl_conf_min_tls_version(&client->config,
                                     MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&client->config,
                                     MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_authmode(&client->config, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&client->config, &client->ca_chain, NULL);

    if (mbedtls_ssl_setup(&client->ssl, &client->config) != 0 ||
        mbedtls_ssl_set_hostname(&client->ssl, hostname) != 0) {
        curl_tls_close(client);
        return CURL_TLS_ERROR_HANDSHAKE;
    }
    mbedtls_ssl_set_bio(&client->ssl, client, tcp_send, tcp_receive, NULL);

    do {
        result = mbedtls_ssl_handshake(&client->ssl);
    } while (result == MBEDTLS_ERR_SSL_WANT_READ ||
             result == MBEDTLS_ERR_SSL_WANT_WRITE);
    if (result != 0 || mbedtls_ssl_get_verify_result(&client->ssl) != 0) {
        curl_tls_close(client);
        return CURL_TLS_ERROR_HANDSHAKE;
    }

    *client_out = client;
    return CURL_TLS_OK;
}

int curl_tls_write(curl_tls_client_t *client, const void *buffer,
                   size_t length)
{
    const unsigned char *bytes = (const unsigned char *)buffer;
    size_t written = 0;

    while (written < length) {
        size_t chunk = length - written;
        int result;
        if (chunk > NET_TCP_STREAM_IO_MAX) chunk = NET_TCP_STREAM_IO_MAX;
        result = mbedtls_ssl_write(&client->ssl, bytes + written, chunk);
        if (result == MBEDTLS_ERR_SSL_WANT_READ ||
            result == MBEDTLS_ERR_SSL_WANT_WRITE)
            continue;
        if (result <= 0) return -1;
        written += (size_t)result;
    }
    return (int)written;
}

int curl_tls_read(curl_tls_client_t *client, void *buffer, size_t capacity)
{
    int result;
    do {
        result = mbedtls_ssl_read(&client->ssl, (unsigned char *)buffer,
                                  capacity);
    } while (result == MBEDTLS_ERR_SSL_WANT_READ ||
             result == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) return 0;
    if (result <= 0) return -1;
    return result;
}

const char *curl_tls_version(const curl_tls_client_t *client)
{
    return mbedtls_ssl_get_version(&client->ssl);
}
