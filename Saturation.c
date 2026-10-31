/* Corrected post* / pre* saturation for pushdown systems with an upper stack.
 * Python reference with the same algorithm: tests/reference.py.
 *
 * A configuration <p, u, l> is accepted when a path from p reads u with upper transitions (u,eps), then
 * l with lower transitions (eps,l), and ends in a final state.
 *
 * Invariant ("stack pointer discipline"):
 *   TOP states are those reachable from p with upper transitions only: the stack pointer can be there.
 *   Rules are applied only at top states, and a lower transition never enters a top state (it enters the
 *   state's lower twin W(m), which has the same lower transitions as m, kept in sync, and is final iff m is).
 *   So a transition added at the top of the lower stack can never be used in the middle of it.
 *
 * post*:  (i)   pop    <p,g> -> <p,eps>   top q' -(eps,g)-> q  ==> (q',(g,eps),T(q))
 *                      T(q): top copy of q (same lower transitions as q, kept in sync)
 *         (ii)  switch <p,g> -> <p,g'>    top q' -(eps,g)-> q  ==> (q',(eps,g'),q)
 *         (iii) push   <p,g> -> <p,g'g''> top q' -(eps,g)-> q, for every r -(x,eps)-> q' (x is overwritten)
 *                      and for r = p if q' = p (empty upper stack):
 *                      ==> (r,(eps,g'),M(r,g')) and (M(r,g'),(eps,g''),q)
 * pre*:   (i)   pop    top q' -(g,eps)-> q''                 ==> (q',(eps,g),q'')
 *         (ii)  switch top q' -(eps,g')-> q                  ==> (q',(eps,g),q)
 *         (iii) push   top q' -(eps,g')-> q1 -(eps,g'')-> q  ==> (q',(x,eps),N(q,g)) for every letter x,
 *                      (N(q,g),(eps,g),q), and (p,(eps,g),q) if q' = p
 *
 * Each added transition only adds successors (predecessors) of accepted configurations, and at the fixpoint
 * the language is closed under one step, so the result is exact when the saturation stops by itself.
 * After `cap` new states, widening: all new states of one kind for one letter are a single summary state,
 * so the number of states is bounded and the saturation stops; the result is then an over-approximation
 * that is still closed under one step. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Saturation.h"

#define UPPER 0
#define LOWER 1

typedef struct { int f, k, l, t; } Tr;

typedef struct {
  int start;
  /* states */
  int n, capN;
  State** orig;          /* original State* (NULL for new states) */
  char** name;
  bool* final;
  int* twin;             /* lower twin W(m), or -1 */
  int* copy;             /* top copy T(q), or -1 */
  /* letters */
  int na, capA;
  char** letters;
  /* transitions + hash set */
  int nt, capT;
  Tr* tr;
  int hcap;
  int* htab;
  /* keyed new states: (key, letter, tag) -> state; key = -1 for widening summary states */
  int nm, capM;
  int *mkey, *mlet, *mtag, *mval;
  int created, cap, widen, finalChanged;
} Sat;

static void* Grow(void* p, int* capacity, int need, size_t size){
  if(need <= *capacity) return p;
  while(*capacity < need) *capacity = *capacity ? 2 * *capacity : 64;
  return realloc(p, (size_t)(*capacity) * size);
}

/* ---------- states ---------- */

static int AddStateRaw(Sat* s, State* o, const char* nm, bool fin){
  int old = s->capN, i = s->n;
  if(s->n + 1 > s->capN){
    int c = old;
    s->orig  = (State**) Grow(s->orig, &c, s->n + 1, sizeof(State*));  c = old;
    s->name  = (char**)  Grow(s->name, &c, s->n + 1, sizeof(char*));   c = old;
    s->final = (bool*)   Grow(s->final, &c, s->n + 1, sizeof(bool));   c = old;
    s->twin  = (int*)    Grow(s->twin, &c, s->n + 1, sizeof(int));     c = old;
    s->copy  = (int*)    Grow(s->copy, &c, s->n + 1, sizeof(int));
    s->capN = c;
  }
  s->orig[i] = o;
  s->name[i] = (char*) malloc(strlen(nm) + 1);
  strcpy(s->name[i], nm);
  s->final[i] = fin;
  s->twin[i] = -1;
  s->copy[i] = -1;
  s->n++;
  return i;
}

static int StateIndex(Sat* s, State* st){
  int i;
  char buf[256];
  for(i = 0; i < s->n; i++)
    if(s->orig[i] && IsTheSameState(s->orig[i], st)) return i;
  if(st->pushed) sprintf(buf, "q(%.100s,%.100s)", st->name->name, st->name->fromAlphabet);
  else if(st->duplicated) sprintf(buf, "D%.200s", st->name->name);
  else sprintf(buf, "%.200s", st->name->name);
  return AddStateRaw(s, st, buf, st->final);
}

static int NewState(Sat* s, const char* prefix, int finalLike){
  char buf[64];
  s->created++;
  sprintf(buf, "%s%d", prefix, s->created);
  return AddStateRaw(s, NULL, buf, finalLike >= 0 && s->final[finalLike]);
}

/* ---------- letters ---------- */

static int Letter(Sat* s, char* x){
  int i;
  for(i = 0; i < s->na; i++) if(!strcmp(s->letters[i], x)) return i;
  s->letters = (char**) Grow(s->letters, &s->capA, s->na + 1, sizeof(char*));
  s->letters[s->na] = x;
  return s->na++;
}

/* ---------- transitions ---------- */

static unsigned Hash(int f, int k, int l, int t){
  unsigned h = (unsigned) f * 2654435761u;
  h ^= (unsigned) t * 40503u + (unsigned) l * 97u + (unsigned) k;
  h *= 2246822519u;
  return h ^ (h >> 15);
}

static int Find(Sat* s, int f, int k, int l, int t){
  unsigned i = Hash(f, k, l, t) & (unsigned)(s->hcap - 1);
  while(s->htab[i]){
    Tr* x = &s->tr[s->htab[i] - 1];
    if(x->f == f && x->k == k && x->l == l && x->t == t) return 1;
    i = (i + 1) & (unsigned)(s->hcap - 1);
  }
  return 0;
}

static void Rehash(Sat* s, int newcap){
  int i;
  free(s->htab);
  s->hcap = newcap;
  s->htab = (int*) calloc((size_t) newcap, sizeof(int));
  for(i = 0; i < s->nt; i++){
    Tr* x = &s->tr[i];
    unsigned j = Hash(x->f, x->k, x->l, x->t) & (unsigned)(newcap - 1);
    while(s->htab[j]) j = (j + 1) & (unsigned)(newcap - 1);
    s->htab[j] = i + 1;
  }
}

/* adds a transition if new; returns 1 if added */
static int Insert(Sat* s, int f, int k, int l, int t){
  unsigned j;
  if(Find(s, f, k, l, t)) return 0;
  s->tr = (Tr*) Grow(s->tr, &s->capT, s->nt + 1, sizeof(Tr));
  s->tr[s->nt].f = f; s->tr[s->nt].k = k; s->tr[s->nt].l = l; s->tr[s->nt].t = t;
  s->nt++;
  if(2 * s->nt > s->hcap) Rehash(s, 2 * s->hcap);
  else{
    j = Hash(f, k, l, t) & (unsigned)(s->hcap - 1);
    while(s->htab[j]) j = (j + 1) & (unsigned)(s->hcap - 1);
    s->htab[j] = s->nt;
  }
  return 1;
}

/* ---------- new states ---------- */

static int Keyed(Sat* s, int key, int let, int tag){
  int i;
  for(i = 0; i < s->nm; i++)
    if(s->mkey[i] == key && s->mlet[i] == let && s->mtag[i] == tag) return s->mval[i];
  return -1;
}

static void SetKeyed(Sat* s, int key, int let, int tag, int val){
  int c = s->capM;
  s->mkey = (int*) Grow(s->mkey, &c, s->nm + 1, sizeof(int)); c = s->capM;
  s->mlet = (int*) Grow(s->mlet, &c, s->nm + 1, sizeof(int)); c = s->capM;
  s->mtag = (int*) Grow(s->mtag, &c, s->nm + 1, sizeof(int)); c = s->capM;
  s->mval = (int*) Grow(s->mval, &c, s->nm + 1, sizeof(int));
  s->capM = c;
  s->mkey[s->nm] = key; s->mlet[s->nm] = let; s->mtag[s->nm] = tag; s->mval[s->nm] = val;
  s->nm++;
}

/* widening: one summary state per kind and letter; final if it summarizes a final state */
static int Summary(Sat* s, int tag, int let, int like){
  static const char* prefix[] = { "SW", "ST", "SM", "SN" };
  int v = Keyed(s, -1, let, tag);
  if(v < 0){ v = NewState(s, prefix[tag], -1); SetKeyed(s, -1, let, tag, v); }
  if(like >= 0 && s->final[like] && !s->final[v]){ s->final[v] = true; s->finalChanged = 1; }
  return v;
}

static int Twin(Sat* s, int m){
  if(s->twin[m] < 0){
    int v = s->widen ? Summary(s, 0, -1, m) : NewState(s, "W", m);
    s->twin[m] = v;
  }
  return s->twin[m];
}

static int Copy(Sat* s, int q){
  if(s->copy[q] < 0){
    int v = s->widen ? Summary(s, 1, -1, q) : NewState(s, "T", q);
    s->copy[q] = v;
  }
  return s->copy[q];
}

static int Mid(Sat* s, int key, int let, int tag){       /* tag 2 = M (post*), 3 = N (pre*) */
  int v = Keyed(s, key, let, tag);
  if(v < 0){
    v = s->widen ? Summary(s, tag, let, -1) : NewState(s, tag == 2 ? "M" : "N", -1);
    SetKeyed(s, key, let, tag, v);
  }
  return v;
}

/* ---------- saturation ---------- */

typedef struct { Tr* a; int n, cap; } TrList;

static void Push(TrList* L, int f, int k, int l, int t){
  L->a = (Tr*) Grow(L->a, &L->cap, L->n + 1, sizeof(Tr));
  L->a[L->n].f = f; L->a[L->n].k = k; L->a[L->n].l = l; L->a[L->n].t = t;
  L->n++;
}

static bool* TopStates(Sat* s, int size){
  bool* top = (bool*) calloc((size_t) size, sizeof(bool));
  int i, ch = 1;
  top[s->start] = true;
  while(ch){
    ch = 0;
    for(i = 0; i < s->nt; i++){
      Tr* x = &s->tr[i];
      if(x->k == UPPER && top[x->f] && !top[x->t]){ top[x->t] = true; ch = 1; }
    }
  }
  return top;
}

static void RulesPost(Sat* s, Upds* upds, bool* top, TrList* nw){
  int i, j, nt = s->nt;
  for(i = 0; i < nt; i++){
    Tr x = s->tr[i];
    ListRule* r;
    if(x.k != LOWER || !top[x.f]) continue;
    for(r = upds->rules; r; r = r->next){
      Rule* rule = r->rule;
      if(Letter(s, rule->from_stack) != x.l) continue;
      if(IsPopRule(rule))
        Push(nw, x.f, UPPER, x.l, Copy(s, x.t));
      else if(isSwitchRule(rule))
        Push(nw, x.f, LOWER, Letter(s, rule->to_stack1), x.t);
      else if(isPushRule(rule)){
        int g1 = Letter(s, rule->to_stack1), g2 = Letter(s, rule->to_stack2);
        for(j = 0; j <= nt; j++){
          int from;
          if(j < nt){
            if(s->tr[j].k != UPPER || s->tr[j].t != x.f) continue;
            from = s->tr[j].f;
          }else{
            if(x.f != s->start) continue;          /* empty upper stack */
            from = s->start;
          }
          {
            int m = Mid(s, from, g1, 2);
            Push(nw, from, LOWER, g1, m);
            Push(nw, m, LOWER, g2, x.t);
          }
        }
      }
    }
  }
}

static void RulesPre(Sat* s, Upds* upds, bool* top, TrList* nw){
  int i, j, a, nt = s->nt;
  for(i = 0; i < nt; i++){
    Tr x = s->tr[i];
    ListRule* r;
    if(!top[x.f]) continue;
    for(r = upds->rules; r; r = r->next){
      Rule* rule = r->rule;
      int lhs = Letter(s, rule->from_stack);
      if(IsPopRule(rule)){
        if(x.k == UPPER && x.l == lhs) Push(nw, x.f, LOWER, lhs, x.t);
      }else if(isSwitchRule(rule)){
        if(x.k == LOWER && x.l == Letter(s, rule->to_stack1)) Push(nw, x.f, LOWER, lhs, x.t);
      }else if(isPushRule(rule)){
        int g1 = Letter(s, rule->to_stack1), g2 = Letter(s, rule->to_stack2);
        if(x.k != LOWER || x.l != g1) continue;
        for(j = 0; j < nt; j++){
          Tr y = s->tr[j];
          if(y.k != LOWER || y.f != x.t || y.l != g2) continue;
          {
            int nq = Mid(s, y.t, lhs, 3);
            for(a = 0; a < s->na; a++) Push(nw, x.f, UPPER, a, nq);
            Push(nw, nq, LOWER, lhs, y.t);
            if(x.f == s->start) Push(nw, s->start, LOWER, lhs, y.t);
          }
        }
      }
    }
  }
}

static Fa* Run(Fa* fa, Upds* upds, int cap, UpdsStatus* status, int post){
  Sat S;
  Sat* s = &S;
  ListTransition* l;
  ListRule* r;
  TrList nw;
  int i;
  bool* top;
  Fa* out;
  State** st;
  ListTransition *head = NULL, *tail = NULL;

  memset(s, 0, sizeof(Sat));
  memset(&nw, 0, sizeof(TrList));
  s->cap = cap;
  s->hcap = 256;
  s->htab = (int*) calloc((size_t) s->hcap, sizeof(int));
  s->start = StateIndex(s, fa->initial);

  /* alphabet: declared Gamma, rules, then automaton */
  for(i = 0; i < upds->nalphabet; i++) Letter(s, upds->alphabet[i]);
  for(r = upds->rules; r; r = r->next){
    Letter(s, r->rule->from_stack);
    if(r->rule->to_stack1) Letter(s, r->rule->to_stack1);
    if(r->rule->to_stack2) Letter(s, r->rule->to_stack2);
  }
  for(l = fa->listTransition; l; l = l->next){
    Transition* t = l->transition;
    Insert(s, StateIndex(s, t->from), t->lower ? LOWER : UPPER, Letter(s, t->label), StateIndex(s, t->to));
  }

  /* discipline on the input: lower transitions entering a top state go to its twin */
  top = TopStates(s, s->n);
  for(i = 0; i < s->nt; i++)
    if(s->tr[i].k == LOWER && top[s->tr[i].t]) s->tr[i].t = Twin(s, s->tr[i].t);
  free(top);
  Rehash(s, s->hcap);

  for(;;){
    int added = 0, size, n0;
    bool* top2;
    if(s->created > s->cap) s->widen = 1;
    s->finalChanged = 0;
    n0 = s->n;
    top = TopStates(s, n0);
    nw.n = 0;
    if(post) RulesPost(s, upds, top, &nw); else RulesPre(s, upds, top, &nw);

    /* new top states: targets of new upper transitions */
    size = s->n + 4 * nw.n + 64;
    top2 = (bool*) calloc((size_t) size, sizeof(bool));
    for(i = 0; i < n0; i++) top2[i] = top[i];
    for(i = 0; i < nw.n; i++) if(nw.a[i].k == UPPER) top2[nw.a[i].t] = true;
    free(top);

    /* twins and top copies follow the lower transitions of their state */
    for(i = 0; i < s->nt + nw.n; i++){
      Tr x = i < s->nt ? s->tr[i] : nw.a[i - s->nt];
      if(x.k != LOWER) continue;
      if(s->twin[x.f] >= 0) Push(&nw, s->twin[x.f], LOWER, x.l, x.t);
      if(s->copy[x.f] >= 0) Push(&nw, s->copy[x.f], LOWER, x.l, x.t);
    }
    for(i = 0; i < nw.n; i++){
      Tr x = nw.a[i];
      if(x.k == LOWER && x.t < size && top2[x.t]) x.t = Twin(s, x.t);
      added += Insert(s, x.f, x.k, x.l, x.t);
    }
    /* a twin created just now must follow its state too: run another round */
    free(top2);
    if(!added && !s->finalChanged) break;
  }

  /* read conditions are not supported here: they were ignored, which only adds behaviours */
  if(status) *status = (s->widen || HasReadRules(upds)) ? UPDS_OVER : UPDS_EXACT;

  /* build the result automaton */
  st = (State**) malloc((size_t) s->n * sizeof(State*));
  for(i = 0; i < s->n; i++){
    if(s->orig[i]) st[i] = s->orig[i];
    else{
      st[i] = CreateState(CreateStateName(s->name[i], ""), false, false, false);
      SetFinal(st[i], s->final[i]);
    }
  }
  for(i = 0; i < s->nt; i++){
    Tr x = s->tr[i];
    ListTransition* node = CreateListTransition(CreateTransition(st[x.f], st[x.t], s->letters[x.l], x.k == LOWER));
    if(!head) head = node; else tail->next = node;
    tail = node;
  }
  out = CreateFa(head, st[s->start]);

  free(st); free(s->orig); free(s->final); free(s->twin); free(s->copy);
  free(s->letters); free(s->tr); free(s->htab);
  free(s->mkey); free(s->mlet); free(s->mtag); free(s->mval); free(nw.a);
  free(s->name);   /* names of new states are kept by their StateName */
  return out;
}

Fa* SaturatePost(Fa* fa, Upds* upds, int cap, UpdsStatus* status){ return Run(fa, upds, cap, status, 1); }
Fa* SaturatePre(Fa* fa, Upds* upds, int cap, UpdsStatus* status){ return Run(fa, upds, cap, status, 0); }
