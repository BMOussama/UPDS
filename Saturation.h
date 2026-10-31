#ifndef SATURATION_H
#define SATURATION_H

#include "Fa.h"
#include "Upds.h"

/* Corrected saturation for post* and pre* of a UPDS (see README, "Corrected algorithm").
 *
 * Input: a P-automaton fa (initial state = control state p, final states marked with SetFinal,
 * no transition into p) and a UPDS with one control state.
 * Output: a NEW automaton (fa is not modified).
 *
 * status (if not NULL) receives
 *   UPDS_EXACT : the saturation reached a fixpoint: the result accepts exactly post* (pre*);
 *   UPDS_OVER  : more than `cap` new states were needed; widening was used to stop: the result accepts
 *                every configuration of post* (pre*) and possibly more (it is closed under one step).
 */
typedef enum { UPDS_EXACT = 0, UPDS_OVER = 1 } UpdsStatus;

#define UPDS_DEFAULT_CAP 40

Fa* SaturatePost(Fa* fa, Upds* upds, int cap, UpdsStatus* status);
Fa* SaturatePre(Fa* fa, Upds* upds, int cap, UpdsStatus* status);

#endif
