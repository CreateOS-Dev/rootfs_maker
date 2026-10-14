// ============================================================
// CreateOS 0.0.1
// ============================================================

#include "group.h"
#include "drivers/vga.h"
#include "lib/string.h"

static group_t groups[16];
static int group_count = 0;

void group_init(void) {
    for (int i = 0; i < 16; i++)
        groups[i].active = 0;

    // groupes de base
    strcpy(groups[0].name, "root");
    groups[0].active = 1;

    strcpy(groups[1].name, "admin");
    groups[1].active = 1;

    strcpy(groups[2].name, "user");
    groups[2].active = 1;

    group_count = 3;
}

int group_add(const char* name) {
    if (group_count >= 16) {
        vga_print("groupadd: max groups reached\n", 0x0C);
        return -1;
    }

    for (int i = 0; i < group_count; i++) {
        if (!strcmp(groups[i].name, name)) {
            vga_print("groupadd: group exists\n", 0x0C);
            return -2;
        }
    }

    strcpy(groups[group_count].name, name);
    groups[group_count].active = 1;

    vga_print("Group added.\n", 0x0A);
    return group_count++;
}

int group_find_id(const char* name) {
    for (int i = 0; i < group_count; i++) {
        if (!strcmp(groups[i].name, name))
            return i;
    }
    return -1;
}

const char* group_get_name(int id) {
    if (id < 0 || id >= group_count) return "?";
    return groups[id].name;
}
