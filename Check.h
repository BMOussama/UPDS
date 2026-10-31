#ifndef CHECK_H
#define CHECK_H

#include "Fa.h"

/* Emptiness of the intersection of two P-automata of a UPDS (both read the upper stack, then the lower
 * stack). Returns 1 if some configuration is accepted by both. */
int FaIntersects(Fa* a, Fa* b);

#endif
