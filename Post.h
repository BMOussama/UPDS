#ifndef POST_H
#define POST_H

#include "Fa.h"
#include "Upds.h"
#include "Saturation.h"

/* post*(L(fa)) for the UPDS: returns a new automaton (see Saturation.h).
 * Prints a warning on stderr if widening was needed (over-approximation).
 * The 2019 implementation of the report's rules is kept in legacy/Post_2019.c. */
Fa* PostStar(Fa* fa, Upds* upds);

#endif
