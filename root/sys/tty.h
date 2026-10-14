// ============================================================
// CreateOS 0.0.1
// ============================================================

#ifndef TTY_H
#define TTY_H

#define TTY_COUNT 3

void tty_init(void);
void tty_switch(int id);
int  tty_current(void);

#endif
