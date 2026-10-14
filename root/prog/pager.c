// ============================================================
// CreateOS 0.0.1
// ============================================================

#include "pager.h"
#include "drivers/vga.h"
#include "drivers/keyboard.h"
#include "lib/string.h"

/* Nombre de lignes affichables à l'écran */
#define SCREEN_LINES 25

static int lines = 0;
static int max_lines = SCREEN_LINES - 2;

/* ---------------- API SIMPLE ---------------- */

void pager_reset(int lines_per_page) {
    lines = 0;
    max_lines = (lines_per_page > 0) ? lines_per_page : (SCREEN_LINES - 2);
}

int pager_line(void) {
    lines++;

    if (lines < max_lines)
        return 1;

    vga_print("-- More -- (SPACE pour continuer, q pour quitter)\n", 0x0E);

    while (1) {
        char c = keyboard_get_char();

        if (c == 'q' || c == 'Q')
            return 0;

        if (c == ' ' || c == '\n') {
            lines = 0;
            return 1;
        }
    }
}

/* ---------------- API AVANCÉE ---------------- */

void pager_start(int lines_per_page) {
    pager_reset(lines_per_page);
}

void pager_add_line(const char* s) {
    if (!pager_line())
        return;

    vga_print(s, 0x0F);
    vga_print("\n", 0x0F);
}

void pager_flush(void) {
    /* rien à faire */
}
