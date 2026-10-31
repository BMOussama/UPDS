#include <stdlib.h>
#include <string.h>
#include "Check.h"

/* states of an automaton, by pointer */
static int Index(State*** st, int* n, int* cap, State* s){
  int i;
  for(i = 0; i < *n; i++) if((*st)[i] == s) return i;
  if(*n == *cap){ *cap = *cap ? 2 * *cap : 64; *st = (State**) realloc(*st, (size_t)(*cap) * sizeof(State*)); }
  (*st)[(*n)++] = s;
  return *n - 1;
}

/* product exploration: (state of a, state of b, phase), phase 0 = still reading the upper stack */
int FaIntersects(Fa* a, Fa* b){
  State **sa = NULL, **sb = NULL;
  int na = 0, nb = 0, ca = 0, cb = 0, found = 0, qh = 0, qt = 0;
  ListTransition *x, *y;
  bool* seen;
  int* queue;
  Index(&sa, &na, &ca, a->initial);
  Index(&sb, &nb, &cb, b->initial);
  for(x = a->listTransition; x; x = x->next){ Index(&sa, &na, &ca, x->transition->from); Index(&sa, &na, &ca, x->transition->to); }
  for(y = b->listTransition; y; y = y->next){ Index(&sb, &nb, &cb, y->transition->from); Index(&sb, &nb, &cb, y->transition->to); }
  seen = (bool*) calloc((size_t) na * (size_t) nb * 2, sizeof(bool));
  queue = (int*) malloc((size_t) na * (size_t) nb * 2 * sizeof(int));
  seen[0] = true; queue[qt++] = 0;                 /* (initial a, initial b, phase 0) */
  while(qh < qt && !found){
    int cur = queue[qh++], ph = cur % 2, ib = (cur / 2) % nb, ia = cur / 2 / nb;
    if(sa[ia]->final && sb[ib]->final){ found = 1; break; }
    for(x = a->listTransition; x; x = x->next){
      Transition* t = x->transition;
      if(t->from != sa[ia] || (ph == 1 && !t->lower)) continue;
      for(y = b->listTransition; y; y = y->next){
        Transition* u = y->transition;
        int ja, jb, nph, key;
        if(u->from != sb[ib] || u->lower != t->lower || strcmp(u->label, t->label)) continue;
        ja = Index(&sa, &na, &ca, t->to); jb = Index(&sb, &nb, &cb, u->to);
        nph = t->lower ? 1 : ph;
        key = (ja * nb + jb) * 2 + nph;
        if(!seen[key]){ seen[key] = true; queue[qt++] = key; }
      }
    }
  }
  free(sa); free(sb); free(seen); free(queue);
  return found;
}
