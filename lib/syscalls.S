# ============================================================
# CreateOS 0.0.1
# ============================================================
/*
 * userlib/syscalls.S - User-space syscall wrappers using SYSCALL/SYSRET
 * These are called from user-space C code to invoke kernel syscalls
 * Only the sys_* prefixed versions are exported here.
 * Standard C library functions (putchar, puts, etc.) are implemented in userlib.c
 */

.section .text
.code64

/*
 * Syscall calling convention (Linux-style):
 * rax = syscall number
 * rdi = arg1
 * rsi = arg2
 * rdx = arg3
 * r10 = arg4
 * r8  = arg5
 * r9  = arg6
 * syscall instruction
 * rcx = return IP, r11 = return RFLAGS
 * rax = return value
 */

#define SYSCALL_SYS(name, num) \
    .global sys_##name; \
    .type sys_##name, @function; \
    sys_##name: \
        mov $num, %rax; \
        mov %rcx, %r10; \
        syscall; \
        ret

/* Process management */
SYSCALL_SYS(exit, 0)

/* VGA/Console */
SYSCALL_SYS(putc, 1)
SYSCALL_SYS(puts, 2)
SYSCALL_SYS(getc, 3)
SYSCALL_SYS(putchar, 4)
SYSCALL_SYS(clear, 5)
SYSCALL_SYS(set_cursor, 90)
SYSCALL_SYS(beep, 74)
SYSCALL_SYS(font_set, 75)
SYSCALL_SYS(font_get, 76)
SYSCALL_SYS(font_count, 118)

/* Filesystem */
SYSCALL_SYS(open, 10)
SYSCALL_SYS(read, 11)
SYSCALL_SYS(read_path, 11)
SYSCALL_SYS(read_at, 87)
SYSCALL_SYS(write, 12)
SYSCALL_SYS(write_path, 12)
SYSCALL_SYS(close, 13)
SYSCALL_SYS(stat, 14)
SYSCALL_SYS(seek, 18)
SYSCALL_SYS(fstat, 88)
SYSCALL_SYS(getdents64, 89)
SYSCALL_SYS(mkdir, 15)
SYSCALL_SYS(unlink, 16)
SYSCALL_SYS(rename, 119)
SYSCALL_SYS(list, 17)
SYSCALL_SYS(chdir, 19)
SYSCALL_SYS(chmod, 20)
SYSCALL_SYS(chown, 21)
SYSCALL_SYS(umask, 125)
SYSCALL_SYS(access, 126)

/* Process */
SYSCALL_SYS(fork, 30)
SYSCALL_SYS(exec, 31)
SYSCALL_SYS(wait, 32)
SYSCALL_SYS(run, 124)
SYSCALL_SYS(getpid, 33)
SYSCALL_SYS(yield, 34)
SYSCALL_SYS(kill, 35)

/* User management */
SYSCALL_SYS(user_add, 40)
SYSCALL_SYS(user_login, 41)
SYSCALL_SYS(user_logout, 42)
SYSCALL_SYS(user_find, 43)
SYSCALL_SYS(user_setpass, 44)
SYSCALL_SYS(user_current, 45)
SYSCALL_SYS(user_lock, 46)
SYSCALL_SYS(user_unlock, 47)
SYSCALL_SYS(user_disable, 48)
SYSCALL_SYS(user_enable, 49)
SYSCALL_SYS(user_count, 53)
SYSCALL_SYS(user_setrole, 54)
SYSCALL_SYS(user_get, 55)
SYSCALL_SYS(user_verify_password, 107)
SYSCALL_SYS(power_info, 108)
SYSCALL_SYS(power_set_idle, 109)
SYSCALL_SYS(pci_info, 110)
SYSCALL_SYS(monotonic_ns, 111)

/* Time */
SYSCALL_SYS(ticks, 50)
SYSCALL_SYS(sleep, 51)
SYSCALL_SYS(time, 52)
SYSCALL_SYS(getcwd, 22)

/* Memory */
SYSCALL_SYS(mem_alloc, 60)
SYSCALL_SYS(mem_free, 61)

/* Debug */
SYSCALL_SYS(debug, 70)

/* Filesystem stats */
SYSCALL_SYS(statfs, 71)
SYSCALL_SYS(tty_name, 72)
SYSCALL_SYS(tty_activate, 73)
SYSCALL_SYS(tty_info, 79)
SYSCALL_SYS(mount, 77)
SYSCALL_SYS(unmount, 78)
SYSCALL_SYS(sync, 80)
SYSCALL_SYS(history_count, 81)
SYSCALL_SYS(history_get, 82)
SYSCALL_SYS(history_add, 83)
SYSCALL_SYS(fsck, 84)
SYSCALL_SYS(usb_info, 85)
SYSCALL_SYS(net_count, 91)
SYSCALL_SYS(net_info, 92)
SYSCALL_SYS(net_dhcp, 93)
SYSCALL_SYS(net_ping, 94)
SYSCALL_SYS(net_tcp, 96)
SYSCALL_SYS(net_resolve, 97)
SYSCALL_SYS(tty_fullscreen_begin, 98)
SYSCALL_SYS(tty_fullscreen_end, 99)
SYSCALL_SYS(tty_mouse_position, 100)
SYSCALL_SYS(tty_pointer_mode, 112)
SYSCALL_SYS(tty_pollchar, 113)
SYSCALL_SYS(tty_cursor_position, 114)
SYSCALL_SYS(clipboard_set, 115)
SYSCALL_SYS(clipboard_get, 116)
SYSCALL_SYS(tty_shell_input, 117)
SYSCALL_SYS(tty_ctrl_c_interrupt, 123)
SYSCALL_SYS(tty_take_ctrl_c, 127)
SYSCALL_SYS(app_return_explorer, 101)
SYSCALL_SYS(net_tcp_stream_open, 102)
SYSCALL_SYS(net_tcp_stream_write, 103)
SYSCALL_SYS(net_tcp_stream_read, 104)
SYSCALL_SYS(net_tcp_stream_close, 105)
SYSCALL_SYS(net_tcp_listen, 120)
SYSCALL_SYS(net_tcp_accept, 121)
SYSCALL_SYS(net_tcp_listener_close, 122)
SYSCALL_SYS(random, 106)
SYSCALL_SYS(kernel_version, 95)
SYSCALL_SYS(file_replace, 86)

.section .note.GNU-stack,"",@progbits