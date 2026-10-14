#include "tcc_compat.h"

#include "user_syscall.h"

#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>

#define TCC_FILE_COUNT 32
#define TCC_FORMAT_BUFFER 1024

static FILE tcc_streams[TCC_FILE_COUNT];
static unsigned char tcc_stream_used[TCC_FILE_COUNT];
static FILE tcc_stdin = { .descriptor = 0 };
static FILE tcc_stdout = { .descriptor = 1 };
static FILE tcc_stderr = { .descriptor = 2 };
FILE* stdin = &tcc_stdin;
FILE* stdout = &tcc_stdout;
FILE* stderr = &tcc_stderr;

extern void sys_exit(int status);

static int tcc_mode_flags(const char* mode)
{
    if (!mode || !mode[0])
        return -1;
    if (mode[0] == 'r')
        return mode[1] == '+' ? O_RDWR : O_RDONLY;
    if (mode[0] == 'w')
        return (mode[1] == '+' ? O_RDWR : O_WRONLY) |
               O_CREAT | O_TRUNC;
    if (mode[0] == 'a')
        return (mode[1] == '+' ? O_RDWR : O_WRONLY) |
               O_CREAT | O_APPEND;
    return -1;
}

static FILE* tcc_stream_allocate(int descriptor)
{
    for (size_t i = 0; i < TCC_FILE_COUNT; i++) {
        if (tcc_stream_used[i])
            continue;
        tcc_stream_used[i] = 1;
        memset(&tcc_streams[i], 0, sizeof(tcc_streams[i]));
        tcc_streams[i].descriptor = descriptor;
        return &tcc_streams[i];
    }
    return NULL;
}

FILE* fopen(const char* path, const char* mode)
{
    int flags = tcc_mode_flags(mode);
    int descriptor;
    FILE* stream;
    if (flags < 0) {
        errno = EINVAL;
        return NULL;
    }
    descriptor = open(path, flags, 0666);
    if (descriptor < 0)
        return NULL;
    stream = tcc_stream_allocate(descriptor);
    if (!stream) {
        close(descriptor);
        errno = EMFILE;
    }
    return stream;
}

FILE* fdopen(int descriptor, const char* mode)
{
    if (descriptor < 0 || tcc_mode_flags(mode) < 0) {
        errno = EINVAL;
        return NULL;
    }
    return tcc_stream_allocate(descriptor);
}

int fclose(FILE* stream)
{
    int result;
    if (!stream) {
        errno = EINVAL;
        return EOF;
    }
    if (stream == stdin || stream == stdout || stream == stderr)
        return fflush(stream);
    result = close(stream->descriptor);
    for (size_t i = 0; i < TCC_FILE_COUNT; i++) {
        if (stream == &tcc_streams[i]) {
            tcc_stream_used[i] = 0;
            memset(stream, 0, sizeof(*stream));
            return result < 0 ? EOF : 0;
        }
    }
    errno = EBADF;
    return EOF;
}

int fflush(FILE* stream)
{
    if (!stream) {
        errno = EINVAL;
        return EOF;
    }
    return stream->error ? EOF : 0;
}

size_t fread(void* buffer, size_t size, size_t count, FILE* stream)
{
    size_t total;
    size_t received = 0;
    if (!stream || (!buffer && size && count) ||
        (size && count > (size_t)-1 / size)) {
        errno = EINVAL;
        return 0;
    }
    total = size * count;
    if (!total)
        return 0;
    if (stream->has_ungot_character) {
        ((unsigned char*)buffer)[received++] =
            (unsigned char)stream->ungot_character;
        stream->has_ungot_character = 0;
    }
    while (received < total) {
        ssize_t result = read(stream->descriptor,
                              (unsigned char*)buffer + received,
                              total - received);
        if (result < 0) {
            stream->error = 1;
            break;
        }
        if (!result) {
            stream->eof = 1;
            break;
        }
        received += (size_t)result;
    }
    return received / size;
}

size_t fwrite(const void* buffer, size_t size, size_t count, FILE* stream)
{
    size_t total;
    size_t written = 0;
    if (!stream || (!buffer && size && count) ||
        (size && count > (size_t)-1 / size)) {
        errno = EINVAL;
        return 0;
    }
    total = size * count;
    while (written < total) {
        ssize_t result = write(stream->descriptor,
                               (const unsigned char*)buffer + written,
                               total - written);
        if (result <= 0) {
            stream->error = 1;
            break;
        }
        written += (size_t)result;
    }
    return written / size;
}

int fseek(FILE* stream, long offset, int origin)
{
    if (!stream || lseek(stream->descriptor, offset, origin) < 0) {
        if (stream)
            stream->error = 1;
        return -1;
    }
    stream->eof = 0;
    stream->has_ungot_character = 0;
    return 0;
}

long ftell(FILE* stream)
{
    if (!stream)
        return -1;
    return lseek(stream->descriptor, 0, SEEK_CUR);
}

int fputc(int character, FILE* stream)
{
    unsigned char value = (unsigned char)character;
    return fwrite(&value, 1, 1, stream) == 1 ? value : EOF;
}

int fputs(const char* string, FILE* stream)
{
    size_t length = strlen(string);
    if (fwrite(string, 1, length, stream) != length)
        return EOF;
    return (int)length;
}

static int tcc_emit_padding(FILE* stream, char character, int count)
{
    while (count-- > 0)
        if (fputc(character, stream) == EOF)
            return -1;
    return 0;
}

static int tcc_emit_string(FILE* stream, const char* string, size_t length,
                           int width, int left, char pad)
{
    int padding = width > (int)length ? width - (int)length : 0;
    if (!left && tcc_emit_padding(stream, pad, padding) < 0)
        return -1;
    if (fwrite(string, 1, length, stream) != length)
        return -1;
    if (left && tcc_emit_padding(stream, ' ', padding) < 0)
        return -1;
    return (int)length + padding;
}

static int tcc_format(FILE* stream, const char* format, va_list arguments)
{
    int total = 0;
    while (*format) {
        int width = 0;
        int precision = -1;
        int left = 0;
        char pad = ' ';
        char length = 0;
        char conversion;
        char text[64];
        size_t text_length = 0;

        if (*format != '%') {
            if (fputc((unsigned char)*format++, stream) == EOF)
                return -1;
            total++;
            continue;
        }
        format++;
        if (*format == '%') {
            if (fputc('%', stream) == EOF)
                return -1;
            format++;
            total++;
            continue;
        }
        while (*format == '-' || *format == '0' || *format == '+' ||
               *format == ' ' || *format == '#') {
            if (*format == '-')
                left = 1;
            else if (*format == '0')
                pad = '0';
            format++;
        }
        if (*format == '*') {
            width = va_arg(arguments, int);
            format++;
            if (width < 0) {
                left = 1;
                width = -width;
            }
        } else {
            while (*format >= '0' && *format <= '9') {
                if (width < 10000)
                    width = width * 10 + (*format - '0');
                format++;
            }
        }
        if (*format == '.') {
            format++;
            precision = 0;
            if (*format == '*') {
                precision = va_arg(arguments, int);
                format++;
            } else {
                while (*format >= '0' && *format <= '9') {
                    if (precision < 10000)
                        precision = precision * 10 + (*format - '0');
                    format++;
                }
            }
        }
        if (*format == 'z' || *format == 't' || *format == 'j' ||
            *format == 'l' || *format == 'h' || *format == 'L')
            length = *format++;
        if (length == 'l' && *format == 'l') {
            length = 'q';
            format++;
        }
        conversion = *format++;

        if (conversion == 's') {
            const char* value = va_arg(arguments, const char*);
            if (!value)
                value = "(null)";
            text_length = strlen(value);
            if (precision >= 0 && text_length > (size_t)precision)
                text_length = (size_t)precision;
            int emitted = tcc_emit_string(stream, value, text_length,
                                          width, left, ' ');
            if (emitted < 0)
                return -1;
            total += emitted;
            continue;
        }
        if (conversion == 'c') {
            text[0] = (char)va_arg(arguments, int);
            text_length = 1;
        } else if (conversion == 'd' || conversion == 'i' ||
                   conversion == 'u' || conversion == 'o' ||
                   conversion == 'x' || conversion == 'X' ||
                   conversion == 'p') {
            unsigned long long value;
            unsigned base = conversion == 'o' ? 8 :
                            (conversion == 'x' || conversion == 'X' ||
                             conversion == 'p') ? 16 : 10;
            int negative = 0;
            char reversed[32];
            size_t digits = 0;
            if (conversion == 'd' || conversion == 'i') {
                long long signed_value;
                if (length == 'z' || length == 't' || length == 'j' ||
                    length == 'l' || length == 'q')
                    signed_value = va_arg(arguments, long);
                else
                    signed_value = va_arg(arguments, int);
                if (signed_value < 0) {
                    negative = 1;
                    value = 0ULL - (unsigned long long)signed_value;
                } else {
                    value = (unsigned long long)signed_value;
                }
            } else if (conversion == 'p') {
                value = (uintptr_t)va_arg(arguments, void*);
            } else if (length == 'z' || length == 't' || length == 'j' ||
                       length == 'l' || length == 'q') {
                value = va_arg(arguments, unsigned long);
            } else {
                value = va_arg(arguments, unsigned int);
            }
            do {
                unsigned digit = (unsigned)(value % base);
                reversed[digits++] = digit < 10 ? (char)('0' + digit) :
                    (char)((conversion == 'X' ? 'A' : 'a') + digit - 10);
                value /= base;
            } while (value && digits < sizeof(reversed));
            if (precision == 0 && digits == 1 && reversed[0] == '0')
                digits = 0;
            while (digits < (size_t)(precision > 0 ? precision : 0) &&
                   digits < sizeof(reversed))
                reversed[digits++] = '0';
            size_t prefix = (negative ? 1 : 0) +
                            (conversion == 'p' ? 2 : 0);
            size_t full_length = digits + prefix;
            int padding = width > (int)full_length ?
                          width - (int)full_length : 0;
            if (!left && pad != '0' &&
                tcc_emit_padding(stream, ' ', padding) < 0)
                return -1;
            if (negative && fputc('-', stream) == EOF)
                return -1;
            if (conversion == 'p' && fputs("0x", stream) < 0)
                return -1;
            if (!left && pad == '0' && precision < 0 &&
                tcc_emit_padding(stream, '0', padding) < 0)
                return -1;
            while (digits) {
                if (fputc(reversed[--digits], stream) == EOF)
                    return -1;
            }
            if (left && tcc_emit_padding(stream, ' ', padding) < 0)
                return -1;
            total += (int)full_length + padding;
            continue;
        } else {
            errno = EINVAL;
            stream->error = 1;
            return -1;
        }
        int emitted = tcc_emit_string(stream, text, text_length,
                                      width, left, pad);
        if (emitted < 0)
            return -1;
        total += emitted;
    }
    return total;
}

int vfprintf(FILE* stream, const char* format, va_list arguments)
{
    va_list copy;
    int result;
    if (!stream || !format) {
        errno = EINVAL;
        return -1;
    }
    va_copy(copy, arguments);
    result = tcc_format(stream, format, copy);
    va_end(copy);
    return result;
}

int fprintf(FILE* stream, const char* format, ...)
{
    va_list arguments;
    int result;
    va_start(arguments, format);
    result = vfprintf(stream, format, arguments);
    va_end(arguments);
    return result;
}

int vprintf(const char* format, va_list arguments)
{
    return vfprintf(stdout, format, arguments);
}

int printf(const char* format, ...)
{
    va_list arguments;
    int result;
    va_start(arguments, format);
    result = vfprintf(stdout, format, arguments);
    va_end(arguments);
    return result;
}

int feof(FILE* stream) { return stream ? stream->eof : 0; }
int ferror(FILE* stream) { return stream ? stream->error : 1; }

void clearerr(FILE* stream)
{
    if (stream) {
        stream->eof = 0;
        stream->error = 0;
    }
}

int ungetc(int character, FILE* stream)
{
    if (!stream || character == EOF || stream->has_ungot_character)
        return EOF;
    stream->ungot_character = (unsigned char)character;
    stream->has_ungot_character = 1;
    stream->eof = 0;
    return (unsigned char)character;
}

char* fgets(char* buffer, int size, FILE* stream)
{
    int count = 0;
    if (!buffer || size <= 0 || !stream) {
        errno = EINVAL;
        return NULL;
    }
    while (count < size - 1) {
        unsigned char character;
        if (fread(&character, 1, 1, stream) != 1)
            break;
        buffer[count++] = (char)character;
        if (character == '\n')
            break;
    }
    if (!count)
        return NULL;
    buffer[count] = 0;
    return buffer;
}

int sscanf(const char* input, const char* format, ...)
{
    va_list arguments;
    int converted = 0;
    if (!input || !format)
        return EOF;
    va_start(arguments, format);
    while (*format) {
        if (*format == '%' && format[1] == 'd') {
            char* end;
            int value = (int)strtol(input, &end, 10);
            if (end == input)
                break;
            *va_arg(arguments, int*) = value;
            input = end;
            format += 2;
            converted++;
        } else if (*input == *format) {
            input++;
            format++;
        } else {
            break;
        }
    }
    va_end(arguments);
    return converted;
}

char* getenv(const char* name)
{
    (void)name;
    return NULL;
}

static int tcc_digit(int character)
{
    if (character >= '0' && character <= '9')
        return character - '0';
    if (character >= 'a' && character <= 'z')
        return character - 'a' + 10;
    if (character >= 'A' && character <= 'Z')
        return character - 'A' + 10;
    return -1;
}

unsigned long long strtoull(const char* text, char** end, int base)
{
    const char* cursor = text;
    unsigned long long value = 0;
    int negative = 0;
    int any = 0;
    while (*cursor == ' ' || (*cursor >= '\t' && *cursor <= '\r'))
        cursor++;
    if (*cursor == '+' || *cursor == '-') {
        negative = *cursor == '-';
        cursor++;
    }
    if ((base == 0 || base == 16) && cursor[0] == '0' &&
        (cursor[1] == 'x' || cursor[1] == 'X')) {
        base = 16;
        cursor += 2;
    } else if (base == 0) {
        base = *cursor == '0' ? 8 : 10;
    }
    if (base < 2 || base > 36) {
        if (end)
            *end = (char*)text;
        errno = EINVAL;
        return 0;
    }
    for (;;) {
        int digit = tcc_digit((unsigned char)*cursor);
        if (digit < 0 || digit >= base)
            break;
        any = 1;
        value = value * (unsigned)base + (unsigned)digit;
        cursor++;
    }
    if (end)
        *end = (char*)(any ? cursor : text);
    return negative ? 0ULL - value : value;
}

long long strtoll(const char* text, char** end, int base)
{
    return (long long)strtoull(text, end, base);
}

unsigned long strtoul(const char* text, char** end, int base)
{
    return (unsigned long)strtoull(text, end, base);
}

long strtol(const char* text, char** end, int base)
{
    return (long)strtoull(text, end, base);
}

double ldexp(double value, int exponent)
{
    while (exponent > 0) {
        value *= 2.0;
        exponent--;
    }
    while (exponent < 0) {
        value *= 0.5;
        exponent++;
    }
    return value;
}

double strtod(const char* text, char** end)
{
    const char* cursor = text;
    double value = 0.0;
    double fractional_scale = 1.0;
    int negative = 0;
    int base = 10;
    int any = 0;
    int exponent = 0;
    int exponent_negative = 0;
    while (*cursor == ' ' || (*cursor >= '\t' && *cursor <= '\r'))
        cursor++;
    if (*cursor == '+' || *cursor == '-') {
        negative = *cursor == '-';
        cursor++;
    }
    if (cursor[0] == '0' && (cursor[1] == 'x' || cursor[1] == 'X')) {
        base = 16;
        cursor += 2;
    }
    for (;;) {
        int digit = tcc_digit((unsigned char)*cursor);
        if (digit < 0 || digit >= base)
            break;
        any = 1;
        value = value * base + digit;
        cursor++;
    }
    if (*cursor == '.') {
        cursor++;
        for (;;) {
            int digit = tcc_digit((unsigned char)*cursor);
            if (digit < 0 || digit >= base)
                break;
            any = 1;
            fractional_scale /= base;
            value += digit * fractional_scale;
            cursor++;
        }
    }
    if (any && ((base == 10 && (*cursor == 'e' || *cursor == 'E')) ||
                (base == 16 && (*cursor == 'p' || *cursor == 'P')))) {
        cursor++;
        if (*cursor == '+' || *cursor == '-') {
            exponent_negative = *cursor == '-';
            cursor++;
        }
        while (*cursor >= '0' && *cursor <= '9') {
            if (exponent < 10000)
                exponent = exponent * 10 + (*cursor - '0');
            cursor++;
        }
        if (exponent_negative)
            exponent = -exponent;
        if (base == 16)
            exponent *= 4;
        while (exponent > 0 && exponent < 2048) {
            value *= 2.0;
            exponent--;
        }
        while (exponent < 0 && exponent > -2048) {
            value *= 0.5;
            exponent++;
        }
    }
    if (end)
        *end = (char*)(any ? cursor : text);
    return negative ? -value : value;
}

float strtof(const char* text, char** end)
{
    return (float)strtod(text, end);
}

long double strtold(const char* text, char** end)
{
    return (long double)strtod(text, end);
}

int abs(int value)
{
    return value < 0 ? -value : value;
}

void qsort(void* base, size_t count, size_t width,
           int (*compare)(const void*, const void*))
{
    unsigned char* bytes = (unsigned char*)base;
    unsigned char* temporary = width ? malloc(width) : NULL;
    if (!base || !compare || !width || (count > 1 && !temporary))
        return;
    for (size_t i = 1; i < count; i++) {
        size_t position = i;
        memcpy(temporary, bytes + i * width, width);
        while (position &&
               compare(bytes + (position - 1) * width, temporary) > 0) {
            memmove(bytes + position * width,
                    bytes + (position - 1) * width, width);
            position--;
        }
        memcpy(bytes + position * width, temporary, width);
    }
    free(temporary);
}

static int tcc_is_leap_year(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static long long tcc_days_before_year(int year)
{
    long long previous = year - 1;
    return 365LL * previous + previous / 4 - previous / 100 +
           previous / 400;
}

static long long tcc_epoch_from_rtc(const rtc_time_t* rtc)
{
    static const int days_before_month[] =
        { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    int year = rtc->year < 70 ? 2000 + rtc->year : 1900 + rtc->year;
    int month = rtc->month >= 1 && rtc->month <= 12 ? rtc->month : 1;
    long long days = tcc_days_before_year(year) -
                     tcc_days_before_year(1970);
    days += days_before_month[month - 1];
    if (month > 2 && tcc_is_leap_year(year))
        days++;
    days += rtc->day ? rtc->day - 1 : 0;
    return (((days * 24 + rtc->hour) * 60 + rtc->minute) * 60 +
            rtc->second);
}

time_t time(time_t* result)
{
    rtc_time_t rtc;
    time_t value = sys_time(&rtc) == 0 ?
                   (time_t)tcc_epoch_from_rtc(&rtc) : (time_t)-1;
    if (result)
        *result = value;
    return value;
}

struct tm* localtime(const time_t* value)
{
    static struct tm result;
    static const int month_days[] =
        { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    time_t seconds;
    long long days;
    int year = 1970;
    if (!value)
        return NULL;
    seconds = *value;
    if (seconds < 0)
        return NULL;
    days = seconds / 86400;
    seconds %= 86400;
    result.tm_hour = (int)(seconds / 3600);
    result.tm_min = (int)((seconds / 60) % 60);
    result.tm_sec = (int)(seconds % 60);
    while (days >= (tcc_is_leap_year(year) ? 366 : 365)) {
        days -= tcc_is_leap_year(year) ? 366 : 365;
        year++;
    }
    result.tm_year = year - 1900;
    result.tm_yday = (int)days;
    result.tm_wday = (int)((*value / 86400 + 4) % 7);
    result.tm_isdst = 0;
    for (int month = 0; month < 12; month++) {
        int length = month_days[month] +
                     (month == 1 && tcc_is_leap_year(year));
        if (days < length) {
            result.tm_mon = month;
            result.tm_mday = (int)days + 1;
            break;
        }
        days -= length;
    }
    return &result;
}

int gettimeofday(struct timeval* value, struct timezone* zone)
{
    int64_t nanoseconds;
    time_t current_time;
    (void)zone;
    if (!value) {
        errno = EFAULT;
        return -1;
    }
    current_time = time(NULL);
    if (current_time == (time_t)-1) {
        errno = EIO;
        return -1;
    }
    nanoseconds = sys_monotonic_ns();
    if (nanoseconds < 0) {
        errno = EIO;
        return -1;
    }
    value->tv_sec = (long)current_time;
    value->tv_usec = (long)((nanoseconds % 1000000000LL) / 1000LL);
    return 0;
}

int isatty(int descriptor)
{
    return descriptor >= 0 && descriptor <= 2;
}

char* strrchr(const char* string, int character)
{
    const char* match = NULL;
    do {
        if (*string == (char)character)
            match = string;
    } while (*string++);
    return (char*)match;
}

int execvp(const char* file, char* const arguments[])
{
    (void)file;
    (void)arguments;
    errno = ENOSYS;
    return -1;
}

void _exit(int status)
{
    sys_exit(status);
    for (;;)
        __asm__ volatile("hlt");
}

void exit(int status)
{
    _exit(status);
}

#undef main
extern int createos_tcc_main(int argc, char** argv);

int main(int argc, char** argv)
{
    int compile_only = 0;
    int has_text_address = 0;
    size_t count;
    char** arguments;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "-E") ||
            !strcmp(argv[i], "-S") || !strcmp(argv[i], "-ar") ||
            !strcmp(argv[i], "-run") || !strcmp(argv[i], "-shared"))
            compile_only = 1;
        if (!strncmp(argv[i], "-Ttext=", 7) ||
            !strncmp(argv[i], "-Wl,-Ttext=", 11))
            has_text_address = 1;
    }
    if (compile_only || has_text_address)
        return createos_tcc_main(argc, argv);
    if (argc < 0 || (size_t)argc > ((size_t)-1 / sizeof(*arguments)) - 2) {
        errno = E2BIG;
        return 1;
    }
    count = (size_t)argc + 2;
    arguments = malloc(count * sizeof(*arguments));
    if (!arguments) {
        errno = ENOMEM;
        return 1;
    }
    for (int i = 0; i < argc; i++)
        arguments[i] = argv[i];
    arguments[argc] = "-Wl,-Ttext=40000000";
    arguments[argc + 1] = NULL;
    sys_puts("[tcc] entering compiler\n", 0x0F);
    int result = createos_tcc_main(argc + 1, arguments);
    sys_puts("[tcc] compiler returned\n", 0x0F);
    free(arguments);
    sys_puts("[tcc] arguments freed\n", 0x0F);
    return result;
}
