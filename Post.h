#ifndef POST_H
#define POST_H

#include "Fa.h"
#include "Upds.h"

Fa * PostStar(Fa * fa , Upds * upds);

Fa* PopCase(Fa* postFa,Rule* rule, Transition* t, ListTransition* pred, ListTransition* acc);
Fa* SwitchCase(Fa* postFa,Rule* rule, Transition* t, ListTransition* pred, ListTransition* acc,Upds* upds);
Fa* PushCase(Fa* postFa,Rule* rule, Transition* t, ListTransition* pred, ListTransition* acc);
#endif
