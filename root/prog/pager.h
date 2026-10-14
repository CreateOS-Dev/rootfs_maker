// ============================================================
// CreateOS 0.0.1
// ============================================================

#ifndef PAGER_H
#define PAGER_H

/* API simple */
void pager_reset(int lines_per_page);
int  pager_line(void);

/* API avancée */
void pager_start(int lines_per_page);
void pager_add_line(const char* s);
void pager_flush(void);

#endif
