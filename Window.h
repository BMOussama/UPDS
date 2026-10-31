#ifndef WINDOW_H
#define WINDOW_H

#include "Fa.h"
#include "Upds.h"

/* Bounded-window model of a UPDS (always regular, always computable).
 *
 * A program reads above the stack pointer only at bounded offsets ([esp-4], [esp-8], ...). The window
 * model keeps the LAST k letters of the upper stack exactly and forgets what is below them:
 * an abstract upper stack is (w, deep) where w is a word of length <= k and deep says that unknown
 * letters may lie below w. It stands for the upper stacks  w  (deep = false)  or  Gamma* w  (deep = true).
 *   pop  g : w := last k letters of w.g   (deep becomes true when a letter falls out of the window)
 *   push   : w := w minus its last letter (if w is empty and deep, an unknown letter is erased)
 * The pairs (w, deep) are finitely many, so they become control states of an ordinary pushdown system
 * (one control state per pair) and post* / pre* are computed with the classic saturation (Schwoon):
 * the result is exact for this model and always terminates.
 *
 * With respect to the full UPDS, the result contains every reachable configuration (it is exact on the
 * last k letters of the upper stack; below them it allows any letters). When every reachable upper stack
 * has at most k letters, it is exact.
 *
 * Input: as for SaturatePost (one control state = fa->initial, final states marked with SetFinal,
 * no transition into the initial state). Declare the stack alphabet with UpdsSetAlphabet.
 * Output: a new P-automaton of the UPDS (upper transitions, then lower transitions).
 *
 * Read rules (RuleSetRead): a rule that requires the j-th upper letter to be x is applied only in
 * windows where it is x (exact when j <= k), or where that letter is below the window and unknown.
 * k = WINDOW_AUTO uses the largest read offset of the rules, so that every read is exact. */
#define WINDOW_AUTO (-1)
Fa* WindowPostStar(Fa* fa, Upds* upds, int k);
Fa* WindowPreStar(Fa* fa, Upds* upds, int k);

#endif
