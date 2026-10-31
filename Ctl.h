#ifndef CTL_H
#define CTL_H

#include "Fa.h"
#include "Upds.h"

/* CTL model checking of a UPDS in the bounded-window model (Window.h).
 *
 * Formula syntax:
 *   atoms   at.X  at.end  upJ.x  true  false      (as in Ltl.h)
 *   ops     !f   f & g   f | g   f -> g   ( f )
 *           EX f  AX f  EF f  AF f  EG f  AG f  E[f U g]  A[f U g]   (also E(f U g); "AGEF f" = AG EF f)
 * Runs are infinite: a configuration where no rule applies (or the empty stack) repeats forever.
 *
 * CtlCheck returns 1 if every configuration of L(fa) satisfies the formula, 0 if some does not, -1 if the
 * formula cannot be parsed. k is the window size (WINDOW_AUTO: the largest read offset of the rules and of
 * the formula). The answer is exact for the window model, where a rule whose read offset is below the
 * window may apply or not (the run may also block there), and an atom upJ.x whose letter is below the
 * window is false. It is exact for the full UPDS when no reachable upper stack leaves the window (no
 * letter ever falls out of it and the initial upper stacks fit in it). */
int CtlCheck(Fa* fa, Upds* upds, int k, const char* formula);

#endif
