#include "lua.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t size;
} luac_allocation_t;

static void* luac_allocator(void* context, void* pointer, size_t old_size,
                            size_t new_size)
{
    luac_allocation_t* allocation;
    size_t copy_size;
    (void)context;

    if (!new_size) {
        if (pointer)
            free((luac_allocation_t*)pointer - 1);
        return NULL;
    }
    if (new_size > (size_t)-1 - sizeof(*allocation))
        return NULL;

    allocation = pointer
        ? (luac_allocation_t*)pointer - 1 : NULL;
    luac_allocation_t* replacement =
        (luac_allocation_t*)malloc(sizeof(*replacement) + new_size);
    if (!replacement)
        return NULL;
    replacement->size = new_size;
    if (pointer) {
        copy_size = allocation->size < new_size
            ? allocation->size : new_size;
        if (old_size && old_size < copy_size)
            copy_size = old_size;
        memcpy(replacement + 1, pointer, copy_size);
        free(allocation);
    }
    return replacement + 1;
}

lua_State* createos_luac_newstate(void)
{
    return lua_newstate(luac_allocator, NULL);
}
