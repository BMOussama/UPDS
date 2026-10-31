#ifndef PRE_H
#define PRE_H

#include "Fa.h"
#include "Upds.h"
#include "Saturation.h"

/* pre*(L(fa)) for the UPDS: returns a new automaton (see Saturation.h).
 * Prints a warning on stderr if widening was needed (over-approximation). */
Fa* PreStar(Fa* fa, Upds* upds);

#endif
