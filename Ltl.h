#ifndef LTL_H
#define LTL_H

#include "Fa.h"
#include "Upds.h"

/* LTL model checking of a UPDS in the bounded-window model (Window.h).
 *
 * Formula syntax:
 *   atoms   at.X       the program is at point X: the top of the lower stack is X
 *           at.end     the lower stack is empty (the run has ended)
 *           upJ.x      the J-th upper letter above the stack pointer is x (up1 = [esp-4])
 *           true, false
 *   ops     !f   f & g   f | g   f -> g   X f   F f   G f   f U g   f R g   ( f )
 * Runs are infinite: a configuration where no rule applies (or the empty stack) repeats forever.
 *
 * LtlCheck returns 1 if every run from L(fa) satisfies the formula, 0 if some run violates it,
 * -1 if the formula cannot be parsed. k is the window size (WINDOW_AUTO: the largest read offset of the
 * rules and of the formula). A "holds" answer is always right for the full UPDS; a "violated" answer is
 * exact for the window model (an upper letter below the window is unknown and may take any value). */
int LtlCheck(Fa* fa, Upds* upds, int k, const char* formula);

/* prints the Buchi automaton built for the negation of the formula (for debugging) */
void LtlPrintAutomaton(const char* formula);

#endif
