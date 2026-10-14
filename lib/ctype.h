// ============================================================
// CreateOS 0.0.1
// ============================================================

#ifndef _CTYPE_H
#define _CTYPE_H

#include <stddef.h>

static inline int isspace(int c)
{
    return (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v');
}

static inline int isdigit(int c)
{
    return (c >= '0' && c <= '9');
}

static inline int isalpha(int c)
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

static inline int isalnum(int c)
{
    return (isalpha(c) || isdigit(c));
}

static inline int isprint(int c)
{
    return (c >= 0x20 && c <= 0x7E);
}

static inline int islower(int c)
{
    return (c >= 'a' && c <= 'z');
}

static inline int isupper(int c)
{
    return (c >= 'A' && c <= 'Z');
}

static inline int toupper(int c)
{
    if (islower(c))
        return c - ('a' - 'A');
    return c;
}

static inline int tolower(int c)
{
    if (isupper(c))
        return c + ('a' - 'A');
    return c;
}

#endif /* _CTYPE_H */