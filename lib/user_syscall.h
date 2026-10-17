// ============================================================
// CreateOS 0.0.1
// ============================================================

#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include "../../kernel/header/drivers/netdev.h"
#include "../../kernel/header/drivers/netstack.h"
#include "../../kernel/header/drivers/usb.h"
#include "../../kernel/header/drivers/pci.h"
#include "kernel_version.h"

/* ============================================================
 * Syscall numbers (matching kernel syscall.h)
 * ============================================================ */
#define SYS_EXIT        0
#define SYS_PUTC        1
#define SYS_PUTS        2
#define SYS_GETC        3
#define SYS_OPEN       10
#define SYS_READ       11
#define SYS_WRITE      12
#define SYS_CLOSE      13
#define SYS_STAT       14
#define SYS_MKDIR      15
#define SYS_UNLINK     16
#define SYS_LIST       17
#define SYS_SEEK       18
#define SYS_CHDIR      19
#define SYS_CHMOD      20
#define SYS_CHOWN      21
#define SYS_CWD        22
#define SYS_FORK       30
#define SYS_EXEC       31
#define SYS_WAIT       32
#define SYS_GETPID     33
#define SYS_YIELD      34
#define SYS_KILL       35
#define SYS_LIST       17
#define SYS_USER_ADD       40
#define SYS_USER_LOGIN     41
#define SYS_USER_LOGOUT    42
#define SYS_USER_FIND      43
#define SYS_USER_SETPASS   44
#define SYS_USER_CURRENT   45
#define SYS_USER_LOCK      46
#define SYS_USER_UNLOCK    47
#define SYS_USER_DISABLE   48
#define SYS_USER_ENABLE    49
#define SYS_USER_COUNT     53
#define SYS_USER_SETROLE   54
#define SYS_USER_GET       55
#define SYS_TICKS      50
#define SYS_SLEEP      51
#define SYS_TIME       52
#define SYS_ALLOC      60
#define SYS_FREE       61
#define SYS_DEBUG      70
#define SYS_FONT_SET   75
#define SYS_FONT_GET   76
#define SYS_FONT_COUNT 118

#define SYS_STATFS     71
#define SYS_TTY_NAME   72
#define SYS_TTY_ACTIVATE 73
#define SYS_MOUNT      77
#define SYS_UNMOUNT    78
#define SYS_TTY_INFO   79
#define SYS_SYNC       80
#define SYS_HISTORY_COUNT 81
#define SYS_HISTORY_GET   82
#define SYS_HISTORY_ADD   83
#define SYS_FSCK          84
#define SYS_USB_INFO      85
#define SYS_FILE_REPLACE  86
#define SYS_READ_AT       87
#define SYS_FSTAT         88
#define SYS_GETDENTS64    89
#define SYS_TTY_SET_CURSOR 90
#define SYS_NET_COUNT     91
#define SYS_NET_INFO      92
#define SYS_NET_DHCP      93
#define SYS_NET_PING      94
#define SYS_KERNEL_VERSION 95
#define SYS_NET_TCP       96
#define SYS_NET_RESOLVE   97
#define SYS_TTY_FULLSCREEN_BEGIN 98
#define SYS_TTY_FULLSCREEN_END   99
#define SYS_TTY_MOUSE_POSITION  100
#define SYS_APP_RETURN_EXPLORER 101
#define SYS_NET_TCP_STREAM_OPEN 102
#define SYS_NET_TCP_STREAM_WRITE 103
#define SYS_NET_TCP_STREAM_READ 104
#define SYS_NET_TCP_STREAM_CLOSE 105
#define SYS_RANDOM      106
#define SYS_USER_VERIFY_PASSWORD 107
#define SYS_POWER_INFO 108
#define SYS_POWER_SET_IDLE 109
#define SYS_PCI_INFO 110
#define SYS_MONOTONIC_NS 111
#define SYS_TTY_POINTER_MODE 112
#define SYS_TTY_POLLCHAR 113
#define SYS_TTY_CURSOR_POSITION 114
#define SYS_CLIPBOARD_SET 115
#define SYS_CLIPBOARD_GET 116
#define SYS_TTY_SHELL_INPUT 117
#define SYS_MAX        128
#define SYS_RENAME     119
#define SYS_NET_TCP_LISTEN 120
#define SYS_NET_TCP_ACCEPT 121
#define SYS_NET_TCP_LISTENER_CLOSE 122
#define SYS_TTY_CTRL_C_INTERRUPT 123
#define SYS_RUN        124
#define SYS_UMASK      125
#define SYS_ACCESS     126
#define SYS_TTY_TAKE_CTRL_C 127

#define KEY_LEFT        0x90
#define KEY_RIGHT       0x91
#define KEY_UP          0x92
#define KEY_DOWN        0x93
#define KEY_DELETE      0x94
#define KEY_TAB         0x95
#define KEY_SHIFT_TAB   0x96
#define KEY_INTERRUPT   0x97
#define KEY_TTY_NEXT    0x98
#define KEY_SCROLL_UP   0x99
#define KEY_SCROLL_DOWN 0x9A
#define KEY_MOUSE_MOVE  0x9B
#define KEY_MOUSE_LEFT  0x9C
#define KEY_MOUSE_UP    0x9D
#define KEY_MOUSE_DOWN  0x9E
#define KEY_CTRL_TAB    0x9F
#define KEY_SHIFT_LEFT  0xA0
#define KEY_SHIFT_RIGHT 0xA1
#define KEY_SHIFT_UP    0xA2
#define KEY_SHIFT_DOWN  0xA3
#define KEY_CTRL_C      0xA4
#define KEY_CTRL_V      0xA5
#define KEY_SHIFT_HOME  0xA6
#define KEY_SHIFT_END   0xA7
#define KEY_MOUSE_DRAG  0xA8
#define KEY_MOUSE_RELEASE 0xA9
#define KEY_CTRL_X      0xAA
#define USER_NAME_MAX  64
#define TTY_FLAG_ACTIVE    0x01
#define TTY_FLAG_GRAPHICAL 0x02
#define TTY_FLAG_FRAMEBUFFER 0x04

/* ============================================================
 * Types
 * ============================================================ */
typedef long ssize_t;
typedef unsigned long size_t;
typedef int pid_t;

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
} rtc_time_t;

/* The shared filesystem ABI declarations are consumed directly here. */
#ifndef FS_TYPE_NONE
#include "../../kernel/header/fs/vfs.h"
#include "../../kernel/header/fs/fs.h"
#endif

/* File open flags */
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

/* User roles */
#define ROLE_GUEST  0
#define ROLE_USER   1
#define ROLE_ADMIN  2
#define ROLE_ROOT   3

#define USER_ADD_SUCCESS 1
#define USER_ADD_INVALID_INPUT (-1)
#define USER_ADD_PERMISSION_DENIED (-2)
#define USER_ADD_LIMIT_REACHED (-3)
#define USER_ADD_ALREADY_EXISTS (-4)
#define USER_ADD_NO_SECURE_RANDOM (-5)
#define USER_ADD_DATABASE_FAILED (-6)
#define USER_LOGIN_PROFILE_ERROR (-2)

typedef struct {
    char name[32];
    char password_hash[32];
    int role;
    char home[64];
    int locked;
} user_t;

typedef struct {
    char name[16];
    uint32_t index;
    uint32_t flags;
    uint32_t width;
    uint32_t height;
} tty_info_t;

/* ============================================================
 * Syscall wrapper declarations (implemented in syscalls.S)
 * ============================================================ */
extern void sys_exit(int status);
extern int sys_putc(char c, int color);
extern int sys_puts(const char* str, int color);
extern char sys_getc(void);
extern int sys_putchar(char c);
extern int sys_clear(void);
extern int sys_set_cursor(int column, int row);
extern int sys_beep(int frequency, int milliseconds);
extern int sys_font_set(int font);
extern int sys_font_get(void);
extern int sys_font_count(void);

extern int sys_open(const char* path, int flags, int mode);
extern ssize_t sys_read(int fd, void* buf, size_t count);
extern ssize_t sys_read_path(const char* path, void* buf, size_t count);
extern ssize_t sys_read_at(const char* path, void* buf, size_t count,
                           size_t offset);
extern int64_t sys_seek(int fd, int64_t offset, int whence);
extern ssize_t sys_write(int fd, const void* buf, size_t count);
extern ssize_t sys_write_path(const char* path, const void* buf, size_t count);
extern int sys_file_replace(const char* path, const void* buf, size_t count);
extern int sys_power_info(void);
extern int sys_power_set_idle(unsigned int minutes);
extern int sys_pci_info(int operation, int index, pci_info_t* info);
extern int64_t sys_monotonic_ns(void);
extern int sys_close(int fd);
extern int sys_stat(const char* path, fs_stat_t* st);
extern int sys_fstat(int fd, fs_stat_t* st);
extern ssize_t sys_getdents64(int fd, void* buffer, size_t size);
extern int sys_mkdir(const char* path, int mode);
extern int sys_unlink(const char* path);
extern int sys_rename(const char* old_path, const char* new_path);
extern int sys_list(const char* path, char names[][USER_NAME_MAX], int max);
extern int sys_chmod(const char* path, int mode);
extern int sys_chown(const char* path, int uid, int gid);
extern int sys_chdir(const char* path);
extern int sys_access(const char* path, int mode);
extern uint32_t sys_umask(uint32_t mask);

extern int sys_fork(void);
extern int sys_exec(const char* path, char* const argv[]);
extern int sys_wait(int* status);
extern int sys_run(const char* path, char* const argv[]);
extern pid_t sys_getpid(void);
extern int sys_yield(void);
extern int sys_kill(pid_t pid);

extern int sys_user_add(const char* name, const char* pass, int role);
extern int sys_user_login(const char* name, const char* pass);
extern int sys_user_verify_password(const char* pass);
extern int sys_user_logout(void);
extern int sys_user_setpass(const char* name, const char* pass,
                            const char* current_pass);
extern int sys_user_find(const char* name, user_t* out);
extern int sys_user_setrole(const char* name, int role);
extern int sys_user_get(int index, user_t* out);
extern int sys_user_current(user_t* out);
extern int sys_user_lock(const char* name);
extern int sys_user_unlock(const char* name);
extern int sys_user_disable(const char* name);
extern int sys_user_enable(const char* name);
extern int sys_user_count(void);
int user_setup_home(const user_t* user);
int user_apply_keyboard_layout(const user_t* user);

/* statfs structure (defined earlier in file) */
extern int sys_statfs(const char* path, statfs_t* buf);
extern int sys_tty_name(char* buf);
extern int sys_tty_activate(const char* name);
extern int sys_tty_info(int index, tty_info_t* info);
extern int sys_tty_fullscreen_begin(void);
extern int sys_tty_fullscreen_end(void);
extern int sys_tty_mouse_position(void);
extern int sys_tty_pointer_mode(int mode);
extern int sys_tty_pollchar(void);
extern int sys_tty_cursor_position(void);
extern int sys_clipboard_set(const char* data, size_t length);
extern int sys_clipboard_get(char* data, size_t capacity);
extern int sys_tty_shell_input(int enabled);
extern int sys_tty_ctrl_c_interrupt(int enabled);
extern int sys_tty_take_ctrl_c(void);
extern int sys_app_return_explorer(int enabled);
extern int sys_mount(const char* device, const char* mountpoint, fs_type_t type, int flags, void* data);
extern int sys_unmount(const char* path);
extern int sys_sync(void);
extern int sys_fsck(void);
extern int sys_usb_info(int operation, int controller, int port,
                        usb_info_t* info);
extern int sys_net_count(void);
extern int sys_net_info(int index, netdev_info_t* info);
extern int sys_net_dhcp(int index);
extern int sys_net_ping(int index, const uint8_t address[4],
                        uint32_t timeout_ms);
extern int sys_net_tcp(int index, const uint8_t address[4], uint16_t port,
                       const void* request, uint32_t request_length,
                       void* response);
extern int sys_net_resolve(int index, const char* hostname,
                           uint8_t address[4]);
extern int sys_net_tcp_stream_open(int index, const uint8_t address[4],
                                   uint16_t port, uint32_t timeout_ms);
extern int sys_net_tcp_stream_write(int handle, const void* data,
                                    uint32_t length, uint32_t timeout_ms);
extern int sys_net_tcp_stream_read(int handle, void* data, uint32_t capacity,
                                   uint32_t timeout_ms);
extern int sys_net_tcp_stream_close(int handle);
extern int sys_net_tcp_listen(int index, uint16_t port);
extern int sys_net_tcp_accept(int handle, uint32_t timeout_ms);
extern int sys_net_tcp_listener_close(int handle);
extern int sys_random(void* buffer, size_t length);
extern int sys_kernel_version(createos_kernel_version_t* version);
extern int sys_history_count(void);
extern int sys_history_get(unsigned int index, char* buffer, size_t size);
extern int sys_history_add(const char* line);

extern unsigned long sys_ticks(void);
extern int sys_sleep(unsigned long ms);
extern int sys_time(rtc_time_t* tloc);
extern int sys_getcwd(char* buf, size_t size);

extern void* sys_mem_alloc(size_t size);
extern int sys_mem_free(void* ptr);

extern int sys_debug(void);

/* ============================================================
 * Standard C library functions (user-space implementations in userlib.c)
 * ============================================================ */

/* string.h */
size_t strlen(const char* s);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strcat(char* dest, const char* src);
char* strchr(const char* s, int c);
void* memset(void* s, int c, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);

/* stdlib.h */
void* malloc(size_t size);
void free(void* ptr);
int atoi(const char* str);
void abort(void);

/* stdio.h - standard C library I/O (wrappers around syscalls) */
int putchar(int c);
int puts(const char* s);
int printf(const char* format, ...);
int sprintf(char* str, const char* format, ...);
int snprintf(char* str, size_t size, const char* format, ...);
int vprintf(const char* format, va_list arg);
int vsprintf(char* str, const char* format, va_list arg);
int vsnprintf(char* str, size_t size, const char* format, va_list arg);

/* unistd.h - POSIX I/O (wrappers around syscalls) */
ssize_t write(int fd, const void* buf, size_t count);
ssize_t read(int fd, void* buf, size_t count);

/* ============================================================
 * VGA Console Helper Functions (implemented in userlib.c)
 * These wrap syscalls for easier console output
 * ============================================================ */
void vga_putc(char c, uint8_t color);
void vga_print(const char* str, uint8_t color);
void vga_clear(void);
char vga_getc(void);
void vga_putchar(char c);
void vga_put_uint(uint32_t num, uint8_t color);

#endif /* USER_SYSCALL_H */