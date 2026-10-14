// ============================================================
// CreateOS 0.0.1
// ============================================================

/*
 * userlib.c — User-space C library implementation using syscalls
 * Implements standard C functions on top of the syscall interface
 */
#include "user_syscall.h"
#include "stdlib.h"

/* ============================================================
 * String functions (string.h)
 * ============================================================ */
size_t strlen(const char* s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (n && *src) {
        *d++ = *src++;
        n--;
    }
    while (n) {
        *d++ = '\0';
        n--;
    }
    return dest;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && *s1 == *s2) { s1++; s2++; n--; }
    return n ? (unsigned char)*s1 - (unsigned char)*s2 : 0;
}

char* strcat(char* dest, const char* src) {
    char* d = dest + strlen(dest);
    while ((*d++ = *src++));
    return dest;
}

char* strchr(const char* s, int c) {
    while (*s && *s != (char)c) s++;
    return (*s == (char)c) ? (char*)s : 0;
}

char* strstr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    if (!needle[0]) return (char*)haystack;
    for (; *haystack; haystack++) {
        const char* candidate = haystack;
        const char* pattern = needle;
        while (*candidate && *pattern && *candidate == *pattern) {
            candidate++;
            pattern++;
        }
        if (!*pattern) return (char*)haystack;
    }
    return NULL;
}

void* memset(void* s, int c, size_t n) {
    char* d = (char*)s;
    while (n--) *d++ = (char)c;
    return s;
}

void* memcpy(void* dest, const void* src, size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    /* Simple implementation - may overlap but we handle forward only */
    if (dest <= src) return memcpy(dest, src, n);
    char* d = (char*)dest + n;
    const char* s = (const char*)src + n;
    while (n--) *--d = *--s;
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const char* a = (const char*)s1;
    const char* b = (const char*)s2;
    while (n--) {
        if (*a != *b) return (unsigned char)*a - (unsigned char)*b;
        a++; b++;
    }
    return 0;
}

/* vsnprintf implementation */
int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    int len = 0;
    const char *p = format;
    while (*p && (size == 0 || len < (int)size - 1)) {
        if (*p == '%') {
            p++;
            if (*p == '%') {
                if (str && size > 0 && len < (int)size - 1) {
                    str[len] = '%';
                }
                len++;
                p++;
            } else if (*p == 's') {
                const char *s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                int l = strlen(s);
                if (str && size > 0) {
                    if (len + l >= (int)size - 1) {
                        l = (int)size - 1 - len;
                    }
                    memcpy(str + len, s, l);
                    len += l;
                } else {
                    len += l;
                }
                p++;
            } else if (*p == 'd' || *p == 'i') {
                int val = va_arg(ap, int);
                char tmp[16];
                int i = 0, neg = 0;
                if (val < 0) { neg = 1; val = -val; }
                if (val == 0) {
                    tmp[i++] = '0';
                } else {
                    while (val > 0) {
                        tmp[i++] = '0' + (val % 10);
                        val /= 10;
                    }
                }
                if (neg) {
                    if (str && size > 0 && len < (int)size - 1) {
                        str[len] = '-';
                    }
                    len++;
                }
                while (i-- > 0) {
                    if (str && size > 0 && len < (int)size - 1) {
                        str[len] = tmp[i];
                    }
                    len++;
                }
                p++;
            } else if (*p == 'c') {
                int c = va_arg(ap, int);
                if (str && size > 0 && len < (int)size - 1) {
                    str[len] = (char)c;
                }
                len++;
                p++;
            } else if (*p == 'x' || *p == 'X') {
                unsigned int val = va_arg(ap, unsigned int);
                char tmp[16];
                int i = 0;
                if (val == 0) {
                    tmp[i++] = '0';
                } else {
                    while (val > 0) {
                        int d = val & 0xF;
                        tmp[i++] = (d < 10) ? '0' + d : 'a' + (d - 10);
                        val >>= 4;
                    }
                }
                while (i-- > 0) {
                    char c = tmp[i];
                    if (*p == 'X' && c >= 'a' && c <= 'f') {
                        c = c - 'a' + 'A';
                    }
                    if (str && size > 0 && len < (int)size - 1) {
                        str[len] = c;
                    }
                    len++;
                }
                p++;
            } else {
                // Unknown format specifier: output '%' and the character
                if (str && size > 0 && len < (int)size - 1) {
                    str[len] = '%';
                }
                len++;
                if (str && size > 0 && len < (int)size - 1) {
                    str[len] = *p;
                }
                len++;
                p++;
            }
        } else {
            if (str && size > 0 && len < (int)size - 1) {
                str[len] = *p;
            }
            len++;
            p++;
        }
    }
    if (str && size > 0) {
        str[len] = '\0';
    }
    return len;
}

/* ============================================================
 * Memory allocation (stdlib.h) - using syscalls
 * ============================================================ */
#define USER_MALLOC_ALIGNMENT 16U
#define USER_MALLOC_PAGE_SIZE 4096U
#define USER_MALLOC_REGION_SIZE (64U * 1024U)
#define USER_MALLOC_REGION_COUNT 64

typedef struct __attribute__((aligned(USER_MALLOC_ALIGNMENT))) user_heap_block {
    size_t size;
    struct user_heap_block* next;
    struct user_heap_block* previous;
    int free;
} user_heap_block_t;

typedef struct {
    void* base;
    user_heap_block_t* first;
    size_t size;
} user_heap_region_t;

static user_heap_region_t user_heap_regions[USER_MALLOC_REGION_COUNT];

static size_t user_heap_align(size_t size)
{
    if (size > (size_t)-1 - (USER_MALLOC_ALIGNMENT - 1))
        return 0;
    return (size + USER_MALLOC_ALIGNMENT - 1) &
           ~(size_t)(USER_MALLOC_ALIGNMENT - 1);
}

static user_heap_region_t* user_heap_add_region(size_t payload_size)
{
    size_t required;
    size_t region_size;
    user_heap_region_t* region = NULL;

    if (payload_size > (size_t)-1 - sizeof(user_heap_block_t))
        return NULL;
    required = payload_size + sizeof(user_heap_block_t);
    region_size = USER_MALLOC_REGION_SIZE;
    if (required > region_size) {
        if (required > (size_t)-1 - (USER_MALLOC_PAGE_SIZE - 1))
            return NULL;
        region_size = (required + USER_MALLOC_PAGE_SIZE - 1) &
                      ~(size_t)(USER_MALLOC_PAGE_SIZE - 1);
    }
    for (size_t i = 0; i < USER_MALLOC_REGION_COUNT; i++) {
        if (!user_heap_regions[i].base) {
            region = &user_heap_regions[i];
            break;
        }
    }
    if (!region)
        return NULL;

    region->base = sys_mem_alloc(region_size);
    if (!region->base)
        return NULL;
    region->size = region_size;
    region->first = region->base;
    region->first->size = region_size - sizeof(user_heap_block_t);
    region->first->next = NULL;
    region->first->previous = NULL;
    region->first->free = 1;
    return region;
}

static void* user_heap_allocate(user_heap_region_t* region, size_t size)
{
    user_heap_block_t* block;
    for (block = region->first; block; block = block->next) {
        if (!block->free || block->size < size)
            continue;
        if (block->size - size >=
            sizeof(user_heap_block_t) + USER_MALLOC_ALIGNMENT) {
            user_heap_block_t* remainder =
                (user_heap_block_t*)((unsigned char*)(block + 1) + size);
            remainder->size = block->size - size -
                              sizeof(user_heap_block_t);
            remainder->next = block->next;
            remainder->previous = block;
            remainder->free = 1;
            if (remainder->next)
                remainder->next->previous = remainder;
            block->next = remainder;
            block->size = size;
        }
        block->free = 0;
        return block + 1;
    }
    return NULL;
}

void* malloc(size_t size) {
    user_heap_region_t* region;
    void* allocation;
    size = user_heap_align(size);
    if (!size)
        return NULL;
    for (size_t i = 0; i < USER_MALLOC_REGION_COUNT; i++) {
        region = &user_heap_regions[i];
        if (region->base &&
            (allocation = user_heap_allocate(region, size)) != NULL)
            return allocation;
    }
    region = user_heap_add_region(size);
    return region ? user_heap_allocate(region, size) : NULL;
}

void* calloc(size_t count, size_t size) {
    size_t total;
    void* allocation;

    if (size && count > (size_t)-1 / size) return NULL;
    total = count * size;
    allocation = malloc(total);
    if (allocation) memset(allocation, 0, total);
    return allocation;
}

void free(void* ptr) {
    if (!ptr)
        return;
    for (size_t i = 0; i < USER_MALLOC_REGION_COUNT; i++) {
        user_heap_region_t* region = &user_heap_regions[i];
        if (!region->base)
            continue;
        for (user_heap_block_t* block = region->first; block;
             block = block->next) {
            if ((void*)(block + 1) != ptr)
                continue;
            block->free = 1;
            if (block->next && block->next->free) {
                block->size += sizeof(user_heap_block_t) + block->next->size;
                block->next = block->next->next;
                if (block->next)
                    block->next->previous = block;
            }
            if (block->previous && block->previous->free) {
                block = block->previous;
                block->size += sizeof(user_heap_block_t) + block->next->size;
                block->next = block->next->next;
                if (block->next)
                    block->next->previous = block;
            }
            if (!block->previous && !block->next) {
                void* base = region->base;
                memset(region, 0, sizeof(*region));
                (void)sys_mem_free(base);
            }
            return;
        }
    }
    (void)sys_mem_free(ptr);
}

void* realloc(void* ptr, size_t size)
{
    size_t old_size = 0;
    void* replacement;
    if (!ptr)
        return malloc(size);
    if (!size) {
        free(ptr);
        return NULL;
    }
    for (size_t i = 0; i < USER_MALLOC_REGION_COUNT; i++) {
        user_heap_region_t* region = &user_heap_regions[i];
        if (!region->base)
            continue;
        for (user_heap_block_t* block = region->first; block;
             block = block->next) {
            if ((void*)(block + 1) == ptr && !block->free) {
                old_size = block->size;
                break;
            }
        }
        if (old_size)
            break;
    }
    if (!old_size)
        return NULL;
    replacement = malloc(size);
    if (!replacement)
        return NULL;
    memcpy(replacement, ptr, old_size < size ? old_size : size);
    free(ptr);
    return replacement;
}

int atoi(const char* str) {
    int result = 0;
    int sign = 1;
    while (*str == ' ' || *str == '\t' || *str == '\n') str++;
    if (*str == '-') { sign = -1; str++; }
    else if (*str == '+') str++;
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return result * sign;
}

void abort(void) {
    sys_exit(1);
}

/* ============================================================
 * stdio.h - Standard C library I/O (wrappers around syscalls)
 * ============================================================ */
int putchar(int c) {
    return sys_putchar((char)c);
}

int puts(const char* s) {
    sys_puts(s, 0x0F);
    sys_putchar('\n');
    return 0;
}

int printf(const char* fmt, ...) {
    /* Simple printf - only supports %s, %d, %c, %x, %% */
    va_list args;
    va_start(args, fmt);
    char buf[256];
    int len = 0;

    const char* p = fmt;
    while (*p && len < 255) {
        if (*p == '%') {
            p++;
            if (*p == 's') {
                const char* str = va_arg(args, const char*);
                if (!str) str = "(null)";
                int l = strlen(str);
                if (len + l >= 255) l = 255 - len;
                memcpy(buf + len, str, l);
                len += l;
            } else if (*p == 'd' || *p == 'i') {
                int val = va_arg(args, int);
                char tmp[16];
                int i = 0;
                int neg = 0;
                if (val < 0) { neg = 1; val = -val; }
                do { tmp[i++] = '0' + (val % 10); val /= 10; } while (val > 0);
                if (neg) tmp[i++] = '-';
                while (i--) {
                    if (len >= 255) break;
                    buf[len++] = tmp[i];
                }
            } else if (*p == 'c') {
                int c = va_arg(args, int);
                if (len < 255) buf[len++] = (char)c;
            } else if (*p == 'x' || *p == 'X') {
                unsigned int val = va_arg(args, unsigned int);
                char tmp[16];
                int i = 0;
                if (val == 0) { tmp[i++] = '0'; }
                else {
                    while (val > 0) {
                        int d = val & 0xF;
                        tmp[i++] = (d < 10) ? '0' + d : 'a' + (d - 10);
                        val >>= 4;
                    }
                }
                while (i--) {
                    if (len >= 255) break;
                    buf[len++] = tmp[i];
                }
            } else if (*p == '%') {
                if (len < 255) buf[len++] = '%';
            }
        } else {
            if (len < 255) buf[len++] = *p;
        }
        p++;
    }
    buf[len] = '\0';
    va_end(args);

    sys_puts(buf, 0x0F);
    return len;
}

int sprintf(char* str, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = 0;

    const char* p = fmt;
    while (*p) {
        if (*p == '%') {
            p++;
            if (*p == 's') {
                const char* str_arg = va_arg(args, const char*);
                if (!str_arg) str_arg = "(null)";
                int l = strlen(str_arg);
                strcpy(str + len, str_arg);
                len += l;
            } else if (*p == 'd' || *p == 'i') {
                int val = va_arg(args, int);
                char tmp[16];
                int i = 0;
                int neg = 0;
                if (val < 0) { neg = 1; val = -val; }
                do { tmp[i++] = '0' + (val % 10); val /= 10; } while (val > 0);
                if (neg) tmp[i++] = '-';
                while (i--) {
                    str[len++] = tmp[i];
                }
            } else if (*p == 'c') {
                int c = va_arg(args, int);
                str[len++] = (char)c;
            } else if (*p == 'x' || *p == 'X') {
                unsigned int val = va_arg(args, unsigned int);
                char tmp[16];
                int i = 0;
                if (val == 0) { tmp[i++] = '0'; }
                else {
                    while (val > 0) {
                        int d = val & 0xF;
                        tmp[i++] = (d < 10) ? '0' + d : 'a' + (d - 10);
                        val >>= 4;
                    }
                }
                while (i--) {
                    str[len++] = tmp[i];
                }
            } else if (*p == '%') {
                str[len++] = '%';
            }
        } else {
            str[len++] = *p;
        }
        p++;
    }
    str[len] = '\0';
    va_end(args);
    return len;
}

int snprintf(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(str, size, format, args);
    va_end(args);
    return ret;
}

static int userland_ensure_directory(const char* path)
{
    fs_stat_t status;
    if (sys_stat(path, &status) == 0)
        return status.type == FS_TYPE_DIR;
    return sys_mkdir(path, 0755) > 0;
}

static int userland_ensure_file(const char* path, const char* contents)
{
    fs_stat_t status;
    if (sys_stat(path, &status) == 0)
        return status.type == FS_TYPE_FILE;
    return sys_file_replace(path, contents, strlen(contents)) == 0;
}

int user_setup_home(const user_t* user)
{
    static const char shell_config[] =
        "completion=on\n"
        "history=on\n";
    static const char apps_config[] =
        "system_shortcuts=/app/system\n";
    char path[160];

    if (!user || strncmp(user->home, "/home/", 6) != 0 ||
        !user->home[6] || strlen(user->home) >= sizeof(path))
        return 0;

    if (!userland_ensure_directory(user->home))
        return 0;
    if (snprintf(path, sizeof(path), "%s/.config", user->home) >=
            (int)sizeof(path) ||
        !userland_ensure_directory(path))
        return 0;
    if (snprintf(path, sizeof(path), "%s/.config/shell.conf", user->home) >=
            (int)sizeof(path) ||
        !userland_ensure_file(path, shell_config))
        return 0;
    if (snprintf(path, sizeof(path), "%s/.config/apps.conf", user->home) >=
            (int)sizeof(path) ||
        !userland_ensure_file(path, apps_config))
        return 0;
    if (snprintf(path, sizeof(path), "%s/.config/keyboards", user->home) >=
            (int)sizeof(path) ||
        !userland_ensure_directory(path))
        return 0;
    if (snprintf(path, sizeof(path), "%s/Applications", user->home) >=
            (int)sizeof(path) ||
        !userland_ensure_directory(path))
        return 0;
    if (snprintf(path, sizeof(path), "%s/Applications/Explorer.APPL",
                 user->home) >= (int)sizeof(path) ||
        !userland_ensure_file(path, "/app/system/bin/explorer.APPL"))
        return 0;
    if (snprintf(path, sizeof(path), "%s/Applications/TextEdit.APPL",
                 user->home) >= (int)sizeof(path) ||
        !userland_ensure_file(path, "/app/system/bin/textedit.APPL"))
        return 0;
    if (snprintf(path, sizeof(path), "%s/Applications/Settings.APPL",
                 user->home) >= (int)sizeof(path) ||
        !userland_ensure_file(path, "/app/system/bin/settings.APPL"))
        return 0;
    return 1;
}

int user_apply_keyboard_layout(const user_t* user)
{
    char path[160];
    char selection[64];
    fs_stat_t status;
    ssize_t size;

    if (!user ||
        snprintf(path, sizeof(path), "%s/.config/keyboards/default",
                 user->home) >= (int)sizeof(path))
        return -1;
    if (sys_stat(path, &status) < 0)
        return 0;
    if (status.type != FS_TYPE_FILE)
        return -1;
    size = sys_read_path(path, selection, sizeof(selection));
    if (size <= 0 || (size_t)size > sizeof(selection))
        return -1;
    return sys_file_replace(path, selection, (size_t)size) == 0 ? 1 : -1;
}

/* ============================================================
 * VGA Console Helper Functions
 * These wrap syscalls for easier console output
 * ============================================================ */
void vga_putc(char c, uint8_t color) {
    (void)sys_putc(c, color);
}

void vga_print(const char* str, uint8_t color) {
    sys_puts(str, color);
}

void vga_clear(void) {
    sys_clear();
}

char vga_getc(void) {
    return sys_getc();
}

void vga_putchar(char c) {
    sys_putchar(c);
}

void vga_put_uint(uint32_t num, uint8_t color) {
    /* Print unsigned integer in decimal */
    char buf[12];
    int i = 11;
    buf[i] = '\0';
    if (num == 0) buf[--i] = '0';
    while (num > 0) {
        buf[--i] = '0' + (num % 10);
        num /= 10;
    }
    sys_puts(&buf[i], color);
}

/* ============================================================
 * Character type functions (ctype.h)
 * ============================================================ */
int isalpha(int c);
int isdigit(int c);

int isalnum(int c) {
    return (isalpha(c) || isdigit(c));
}

int isalpha(int c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

int isascii(int c) {
    return (c >= 0 && c <= 127);
}

int isblank(int c) {
    return (c == ' ' || c == '\t');
}

int iscntrl(int c) {
    return (c >= 0 && c <= 31) || (c == 127);
}

int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

int isgraph(int c) {
    return (c >= 33 && c <= 126);
}

int islower(int c) {
    return (c >= 'a' && c <= 'z');
}

int isprint(int c) {
    return (c >= 32 && c <= 126);
}

int ispunct(int c) {
    return (isgraph(c) && !isalnum(c));
}

int isspace(int c) {
    return (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v');
}

int isupper(int c) {
    return (c >= 'A' && c <= 'Z');
}

int isxdigit(int c) {
    return ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F'));
}

int tolower(int c) {
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 'a';
    return c;
}

int toupper(int c) {
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';
    return c;
}

/* ============================================================
 * String functions (string.h) - extended
 * ============================================================ */
int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        int c1 = tolower(*s1++);
        int c2 = tolower(*s2++);
        if (c1 != c2)
            return c1 - c2;
    }
    return tolower(*s1) - tolower(*s2);
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
    if (n == 0)
        return 0;

    while (n-- > 0 && *s1 && *s2) {
        int c1 = tolower(*s1++);
        int c2 = tolower(*s2++);
        if (c1 != c2)
            return c1 - c2;
    }
    if (n == 0)
        return 0;
    return tolower(*s1) - tolower(*s2);
}