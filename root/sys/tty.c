// ============================================================
// CreateOS 0.0.1
// ============================================================

#include "proc/tty.h"
#include "drivers/vga.h"
#include "lib/string.h"
#include <stdint.h>

#define VGA_MEM   ((uint16_t*)0xB8000)
#define VGA_COLS  80
#define VGA_ROWS  25
#define VGA_SIZE  (VGA_COLS * VGA_ROWS)

typedef struct {
    uint16_t buffer[VGA_SIZE];
    int cursor_x;
    int cursor_y;
    int initialized;
} tty_t;

static tty_t g_ttys[TTY_COUNT];
static int   g_current_tty = 0;

static void tty_save(int id) {
    if (id < 0 || id >= TTY_COUNT) return;

    int x, y;
    vga_get_cursor(&x, &y);
    g_ttys[id].cursor_x = x;
    g_ttys[id].cursor_y = y;

    memcpy(g_ttys[id].buffer, VGA_MEM, VGA_SIZE * sizeof(uint16_t));
    g_ttys[id].initialized = 1;
}

static void tty_restore(int id) {
    if (id < 0 || id >= TTY_COUNT) return;

    if (!g_ttys[id].initialized) {
        vga_clear();
        vga_set_cursor(0, 0);
        g_ttys[id].cursor_x = 0;
        g_ttys[id].cursor_y = 0;
        g_ttys[id].initialized = 1;
        return;
    }

    memcpy(VGA_MEM, g_ttys[id].buffer, VGA_SIZE * sizeof(uint16_t));
    vga_set_cursor(g_ttys[id].cursor_x, g_ttys[id].cursor_y);
}

void tty_init(void) {
    for (int i = 0; i < TTY_COUNT; i++) {
        g_ttys[i].initialized = 0;
        g_ttys[i].cursor_x = 0;
        g_ttys[i].cursor_y = 0;
    }
    g_current_tty = 0;
    vga_clear();
    vga_set_cursor(0, 0);
    g_ttys[0].initialized = 1;
}

void tty_switch(int id) {
    if (id < 0 || id >= TTY_COUNT) return;
    if (id == g_current_tty) return;

    tty_save(g_current_tty);
    g_current_tty = id;
    tty_restore(g_current_tty);
}

int tty_current(void) {
    return g_current_tty;
}
