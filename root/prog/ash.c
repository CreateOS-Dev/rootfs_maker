// ============================================================
// CreateOS 0.0.1
// ============================================================

#include "ash.h"

/*
    ASH = "Additive Shift Hash"
    - Très simple
    - Déterministe
    - 32 bits → encodé en hex (8 caractères)
*/

void ash_hash(const char* in, char* out) {
    unsigned int h = 0xABCDEF01;  // seed

    while (*in) {
        h ^= (unsigned int)(*in);
        h = (h << 5) | (h >> 27);  // rotation
        h += (unsigned int)(*in * 31);
        in++;
    }

    // encode en hex
    const char* hex = "0123456789ABCDEF";
    for (int i = 0; i < 8; i++) {
        out[i] = hex[(h >> ((7 - i) * 4)) & 0xF];
    }
    out[8] = 0;
}
