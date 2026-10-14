#include "curl_http.h"

#include <string.h>

static int ascii_lower(int value)
{
    if (value >= 'A' && value <= 'Z')
        return value + ('a' - 'A');
    return value;
}

static int header_name_is(const uint8_t* name, size_t length,
                          const char* expected)
{
    size_t expected_length = strlen(expected);
    if (length != expected_length)
        return 0;
    for (size_t i = 0; i < length; i++)
        if (ascii_lower(name[i]) != ascii_lower((unsigned char)expected[i]))
            return 0;
    return 1;
}

static int header_name_character(uint8_t ch)
{
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9'))
        return 1;
    return ch == '!' || ch == '#' || ch == '$' || ch == '%' || ch == '&' ||
           ch == '\'' || ch == '*' || ch == '+' || ch == '-' || ch == '.' ||
           ch == '^' || ch == '_' || ch == '`' || ch == '|' || ch == '~';
}

int curl_http_parse_response(const uint8_t* response, size_t response_length,
                             int connection_closed, size_t* body_offset,
                             size_t* body_length, int* status_code)
{
    size_t header_end = 0;
    size_t status_end = 0;
    size_t cursor;
    size_t content_length = 0;
    int has_content_length = 0;

    if (!response || !body_offset || !body_length || !status_code)
        return 0;
    for (size_t i = 0; i + 3 < response_length; i++) {
        if (response[i] == '\r' && response[i + 1] == '\n' &&
            response[i + 2] == '\r' && response[i + 3] == '\n') {
            header_end = i + 2;
            break;
        }
    }
    if (!header_end || response_length < 13 ||
        (memcmp(response, "HTTP/1.0 ", 9) != 0 &&
         memcmp(response, "HTTP/1.1 ", 9) != 0))
        return 0;

    *status_code = 0;
    for (size_t i = 9; i < 12; i++) {
        if (response[i] < '0' || response[i] > '9')
            return 0;
        *status_code = *status_code * 10 + (response[i] - '0');
    }
    if (*status_code != 200 ||
        (response[12] != ' ' && response[12] != '\r'))
        return 0;
    for (size_t i = 0; i + 1 < header_end; i++) {
        if (response[i] == '\r' && response[i + 1] == '\n') {
            status_end = i;
            break;
        }
        if (response[i] < 0x20 || response[i] > 0x7e)
            return 0;
    }
    if (!status_end)
        return 0;

    cursor = status_end + 2;
    while (cursor < header_end) {
        size_t line_end = cursor;
        size_t colon = cursor;
        size_t value_start;
        size_t value_end;

        while (line_end + 1 < header_end &&
               !(response[line_end] == '\r' &&
                 response[line_end + 1] == '\n'))
            line_end++;
        if (line_end + 1 >= header_end)
            return 0;
        while (colon < line_end && response[colon] != ':') {
            if (!header_name_character(response[colon]))
                return 0;
            colon++;
        }
        if (colon == cursor || colon == line_end)
            return 0;
        value_start = colon + 1;
        while (value_start < line_end &&
               (response[value_start] == ' ' || response[value_start] == '\t'))
            value_start++;
        value_end = line_end;
        while (value_end > value_start &&
               (response[value_end - 1] == ' ' ||
                response[value_end - 1] == '\t'))
            value_end--;
        for (size_t i = value_start; i < value_end; i++)
            if ((response[i] < 0x20 && response[i] != '\t') ||
                response[i] > 0x7e)
                return 0;

        if (header_name_is(response + cursor, colon - cursor,
                           "content-length")) {
            size_t parsed = 0;
            if (has_content_length || value_start == value_end)
                return 0;
            for (size_t i = value_start; i < value_end; i++) {
                size_t digit;
                if (response[i] < '0' || response[i] > '9')
                    return 0;
                digit = (size_t)(response[i] - '0');
                if (parsed > (CURL_HTTP_RESPONSE_MAX - digit) / 10)
                    return 0;
                parsed = parsed * 10 + digit;
            }
            content_length = parsed;
            has_content_length = 1;
        } else if (header_name_is(response + cursor, colon - cursor,
                                  "transfer-encoding")) {
            return 0;
        }
        cursor = line_end + 2;
    }

    *body_offset = header_end + 2;
    *body_length = response_length - *body_offset;
    if (has_content_length)
        return *body_length == content_length;
    return connection_closed;
}

int curl_http_valid_relative_path(const char* path)
{
    size_t length;
    size_t component_start = 0;

    if (!path || !path[0] || path[0] == '/')
        return 0;
    length = strlen(path);
    if (length >= CURL_HTTP_SAVE_PATH_MAX || path[length - 1] == '/')
        return 0;
    for (size_t i = 0; i <= length; i++) {
        unsigned char ch = (unsigned char)path[i];
        if (ch == '/' || ch == '\0') {
            size_t component_length = i - component_start;
            if (!component_length ||
                (component_length == 1 && path[component_start] == '.') ||
                (component_length == 2 && path[component_start] == '.' &&
                 path[component_start + 1] == '.'))
                return 0;
            component_start = i + 1;
        } else if (ch < 0x20 || ch == 0x7f || ch == '\\' || ch == ':') {
            return 0;
        }
    }
    return 1;
}
