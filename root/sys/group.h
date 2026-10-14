// ============================================================
// CreateOS 0.0.1
// ============================================================

#ifndef GROUP_H
#define GROUP_H

typedef struct {
    char name[16];
    int active;
} group_t;

void group_init(void);
int group_add(const char* name);
int group_find_id(const char* name);
const char* group_get_name(int id);

#endif
