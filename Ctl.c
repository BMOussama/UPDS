/* CTL model checking on the window model: see Ctl.h.
 *
 * Sets of configurations <c, l BOT> are finite automata (Aut, McInternal.h): node c (c < C) reads the lower
 * stack of the configurations of window c. Bottom-up, formula by formula:
 *   atoms               direct construction (they depend on the window and the top letter)
 *   !, &, |             complement (subset construction), products
 *   EX f                one step backwards: c --g--> q  when <c,g> -> <c',w> and c' --w--> q
 *   E[f U g], EG f      pre* on the window pushdown system restricted to the configurations satisfying f.
 *       f is not a simple valuation (it may depend on the whole lower stack), so each stack letter is
 *       annotated with the state of a deterministic bottom-up automaton for f (regular valuations,
 *       Esparza, Kucera, Schwoon): letter (g, d), d = the set of nodes of f's automaton from which the part
 *       of the stack below g is accepted. Then f holds at <c,(g,d)> iff c --g--> q for some q in d.
 *       E[f U g]: pre* of g's automaton (annotated) with the rules of the f-configurations.
 *       EG f:     repeating heads of the f-restricted system (every run there satisfies G f), then pre*.
 *       The result is projected back to plain letters, keeping only consistent annotations.
 *   AX, AF, AG, AU, EF  by duality. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Ctl.h"
#include "Window.h"
#include "McInternal.h"

/* ---------- table of subsets (bitsets of nb bytes) ---------- */
typedef struct { int nb; unsigned char* data; int n, cap; int* next; McMap head; } SetTab;

static void SetInit(SetTab* t, int nbits){ memset(t, 0, sizeof(SetTab)); t->nb = (nbits + 7) / 8; if(!t->nb) t->nb = 1; McMapInit(&t->head); }
static void SetFree(SetTab* t){ free(t->data); free(t->next); McMapFree(&t->head); }
static unsigned char* SetGet(SetTab* t, int i){ return t->data + (size_t) i * (size_t) t->nb; }
static int Bit(unsigned char* s, int i){ return (s[i / 8] >> (i % 8)) & 1; }
static void SetBit(unsigned char* s, int i){ s[i / 8] = (unsigned char)(s[i / 8] | (1 << (i % 8))); }

static int SetHash(SetTab* t, unsigned char* s){
  unsigned h = 2166136261u;
  int i;
  for(i = 0; i < t->nb; i++){ h ^= s[i]; h *= 16777619u; }
  return (int)(h & 0x7fffffff);
}

/* index of set s; added if absent (*isnew = 1) */
static int SetAdd(SetTab* t, unsigned char* s, int* isnew){
  int h = SetHash(t, s), i = McMapGet(&t->head, h, 0, 0);
  for(; i >= 0; i = t->next[i]) if(!memcmp(SetGet(t, i), s, (size_t) t->nb)){ if(isnew) *isnew = 0; return i; }
  if(t->n == t->cap){
    t->cap = t->cap ? 2 * t->cap : 16;
    t->data = (unsigned char*) realloc(t->data, (size_t) t->cap * (size_t) t->nb);
    t->next = (int*) realloc(t->next, (size_t) t->cap * sizeof(int));
  }
  memcpy(SetGet(t, t->n), s, (size_t) t->nb);
  t->next[t->n] = McMapGet(&t->head, h, 0, 0);
  McMapPut(&t->head, h, 0, 0, t->n);
  if(isnew) *isnew = 1;
  return t->n++;
}

typedef struct { McModel* m; int C, G, BOT; } Ctx;

/* ---------- automata operations ---------- */

/* removes the nodes that are not reachable from a control or cannot reach a final node */
static Aut* Trim(Ctx* X, Aut* A){
  bool *fw = (bool*) calloc((size_t) A->n + 1, sizeof(bool)), *bw = (bool*) calloc((size_t) A->n + 1, sizeof(bool));
  int* id = (int*) malloc(((size_t) A->n + 1) * sizeof(int));
  int* st = (int*) malloc(((size_t) A->n + 1) * sizeof(int));
  int sp = 0, i, changed = 1, n = X->C;
  Aut* R;
  AutIndex(A);
  for(i = 0; i < X->C; i++){ fw[i] = true; st[sp++] = i; }
  while(sp > 0){
    int u = st[--sp], j;
    for(j = A->first[u]; j >= 0; j = A->nxt[j]) if(!fw[A->tr[j].v]){ fw[A->tr[j].v] = true; st[sp++] = A->tr[j].v; }
  }
  for(i = 0; i < A->n; i++) bw[i] = A->fin[i];
  while(changed){
    changed = 0;
    for(i = 0; i < A->nt; i++) if(bw[A->tr[i].v] && !bw[A->tr[i].u]){ bw[A->tr[i].u] = true; changed = 1; }
  }
  for(i = 0; i < A->n; i++) id[i] = i < X->C ? i : (fw[i] && bw[i] ? n++ : -1);
  R = AutNew(n, X->G);
  for(i = 0; i < A->n; i++) if(id[i] >= 0) R->fin[id[i]] = A->fin[i];
  for(i = 0; i < A->nt; i++){
    McTr t = A->tr[i];
    if(id[t.u] >= 0 && id[t.v] >= 0 && bw[t.v] && (t.u < X->C ? bw[t.u] : 1)) AutAdd(R, id[t.u], t.a, id[t.v]);
  }
  AutIndex(R);
  free(fw); free(bw); free(id); free(st);
  AutFree(A);
  return R;
}

static Aut* Atom(Ctx* X, McAtom* a){
  Aut* A = AutNew(X->C, X->G);
  int any = AutNode(A, false), end = AutNode(A, true), c, g;
  for(g = 0; g < X->G; g++) AutAdd(A, any, g, g == X->BOT ? end : any);
  for(c = 0; c < X->C; c++) for(g = 0; g < X->G; g++)
    if(McAtomValue(X->m, a, c, g) == 1) AutAdd(A, c, g, g == X->BOT ? end : any);   /* unknown: false */
  return Trim(X, A);
}

/* complement among the valid configurations (lower stack in Gamma* BOT) */
static Aut* Not(Ctx* X, Aut* A){
  Aut* R = AutNew(X->C, X->G);
  int end = AutNode(R, true), c, g, i, qh = 0, isnew;
  SetTab T;
  int* node = NULL;
  int capNode = 0;
  unsigned char* tmp;
  SetInit(&T, A->n);
  tmp = (unsigned char*) malloc((size_t) X->G * (size_t) T.nb);
  for(c = 0; c < X->C; c++){
    memset(tmp, 0, (size_t) T.nb);
    SetBit(tmp, c);
    SetAdd(&T, tmp, NULL);
  }
  capNode = T.n;
  node = (int*) malloc((size_t) capNode * sizeof(int));
  for(c = 0; c < X->C; c++) node[c] = c;
  while(qh < T.n){
    int u = node[qh];
    memset(tmp, 0, (size_t) X->G * (size_t) T.nb);
    for(i = 0; i < A->n; i++){
      int j;
      if(!Bit(SetGet(&T, qh), i)) continue;
      for(j = A->first[i]; j >= 0; j = A->nxt[j]) SetBit(tmp + (size_t) A->tr[j].a * (size_t) T.nb, A->tr[j].v);
    }
    for(g = 0; g < X->G; g++){
      unsigned char* s = tmp + (size_t) g * (size_t) T.nb;
      if(g == X->BOT){
        int acc = 0;
        for(i = 0; i < A->n; i++) if(Bit(s, i) && A->fin[i]) acc = 1;
        if(!acc) AutAdd(R, u, g, end);
      }else{
        int id = SetAdd(&T, s, &isnew);
        if(isnew){
          if(id >= capNode){ capNode = 2 * capNode + 16; node = (int*) realloc(node, (size_t) capNode * sizeof(int)); }
          node[id] = AutNode(R, false);
        }
        AutAdd(R, u, g, node[id]);
      }
    }
    qh++;
  }
  free(tmp); free(node); SetFree(&T);
  AutFree(A);
  return Trim(X, R);
}

/* intersection (isOr = 0) or union (isOr = 1); a side may be dead (-1) in a union */
static Aut* Prod(Ctx* X, Aut* A, Aut* B, int isOr){
  Aut* R = AutNew(X->C, X->G);
  McMap map;
  int *qu = NULL, *qv = NULL, cap = 0, n = 0, qh = 0, c, g;
  int *ta = (int*) malloc(((size_t) A->n + 1) * sizeof(int)), *tb = (int*) malloc(((size_t) B->n + 1) * sizeof(int));
  McMapInit(&map);
#define PAIR(U, V, ID) do { int id_ = McMapGet(&map, (U), (V), 0); \
    if(id_ < 0){ bool f_ = isOr ? (((U) >= 0 && A->fin[U]) || ((V) >= 0 && B->fin[V])) : (A->fin[U] && B->fin[V]); \
      id_ = n < X->C ? n : AutNode(R, f_); if(n < X->C) R->fin[id_] = f_; \
      McMapPut(&map, (U), (V), 0, id_); \
      if(n == cap){ cap = 2 * cap + 16; qu = (int*) realloc(qu, (size_t) cap * sizeof(int)); qv = (int*) realloc(qv, (size_t) cap * sizeof(int)); } \
      qu[n] = (U); qv[n] = (V); n++; } (ID) = id_; } while(0)
  for(c = 0; c < X->C; c++){ int d; PAIR(c, c, d); (void) d; }
  while(qh < n){
    int u = qu[qh], v = qv[qh], src, i, j, j2;
    src = McMapGet(&map, u, v, 0);
    qh++;
    for(g = 0; g < X->G; g++){
      int na = 0, nb = 0;
      if(u >= 0) for(j = A->first[u]; j >= 0; j = A->nxt[j]) if(A->tr[j].a == g) ta[na++] = A->tr[j].v;
      if(v >= 0) for(j = B->first[v]; j >= 0; j = B->nxt[j]) if(B->tr[j].a == g) tb[nb++] = B->tr[j].v;
      if(isOr){ if(!na && !nb) continue; if(!na) ta[na++] = -1; if(!nb) tb[nb++] = -1; }
      for(i = 0; i < na; i++) for(j2 = 0; j2 < nb; j2++){
        int d;
        PAIR(ta[i], tb[j2], d);
        AutAdd(R, src, g, d);
      }
    }
  }
#undef PAIR
  free(qu); free(qv); free(ta); free(tb); McMapFree(&map);
  AutFree(A); AutFree(B);
  return Trim(X, R);
}

/* nodes reached from node u of A by the word w (|w| <= 2), into out; returns their number */
static int Read(Aut* A, int u, const int* w, int len, int* out, int* tmp){
  int n = 1, i, j, s;
  out[0] = u;
  for(s = 0; s < len; s++){
    int m = 0;
    for(i = 0; i < n; i++) for(j = A->first[out[i]]; j >= 0; j = A->nxt[j]) if(A->tr[j].a == w[s]){
      int k2, dup = 0;
      for(k2 = 0; k2 < m; k2++) if(tmp[k2] == A->tr[j].v) dup = 1;
      if(!dup) tmp[m++] = A->tr[j].v;
    }
    for(i = 0; i < m; i++) out[i] = tmp[i];
    n = m;
  }
  return n;
}

static Aut* EX(Ctx* X, Aut* A){
  McModel* m = X->m;
  Aut* R = AutNew(X->C + A->n, X->G);
  int *out = (int*) malloc(((size_t) A->n + 1) * sizeof(int)), *tmp = (int*) malloc(((size_t) A->n + 1) * sizeof(int));
  int i, c, g, r;
  for(i = 0; i < A->n; i++) R->fin[X->C + i] = A->fin[i];
  for(i = 0; i < A->nt; i++) AutAdd(R, X->C + A->tr[i].u, A->tr[i].a, X->C + A->tr[i].v);
  for(c = 0; c < X->C; c++) for(g = 0; g < X->G; g++)
    for(r = m->rstart[c * m->G + g]; r < m->rstart[c * m->G + g + 1]; r++){
      McRule* x = &m->rules[r];
      int w[2], len = x->kind == MC_POP ? 0 : x->kind == MC_SW ? 1 : 2, n;
      w[0] = x->g1; w[1] = x->g2;
      n = Read(A, x->c2, w, len, out, tmp);
      for(i = 0; i < n; i++) AutAdd(R, c, g, X->C + out[i]);
    }
  free(out); free(tmp);
  AutFree(A);
  return Trim(X, R);
}

/* ---------- annotation by a bottom-up deterministic automaton of f ---------- */
typedef struct { int nD, d0; int* delta; SetTab T; } Ann;

static void AnnBuild(Ctx* X, Aut* A, Ann* D){
  int i, g, d, cap = 0;
  unsigned char* tmp;
  SetInit(&D->T, A->n);
  tmp = (unsigned char*) calloc((size_t) X->G * (size_t) D->T.nb + 1, 1);
  for(i = 0; i < A->n; i++) if(A->fin[i]) SetBit(tmp, i);
  D->d0 = SetAdd(&D->T, tmp, NULL);
  D->delta = NULL;
  for(d = 0; d < D->T.n; d++){
    memset(tmp, 0, (size_t) X->G * (size_t) D->T.nb);
    for(i = 0; i < A->nt; i++) if(Bit(SetGet(&D->T, d), A->tr[i].v)) SetBit(tmp + (size_t) A->tr[i].a * (size_t) D->T.nb, A->tr[i].u);
    if((d + 1) * X->G > cap){ cap = 2 * (d + 1) * X->G; D->delta = (int*) realloc(D->delta, (size_t) cap * sizeof(int)); }
    for(g = 0; g < X->G; g++) D->delta[d * X->G + g] = SetAdd(&D->T, tmp + (size_t) g * (size_t) D->T.nb, NULL);
  }
  D->nD = D->T.n;
  free(tmp);
}

/* does f hold at <c, (g, d)> ? */
static int AnnHolds(Aut* A, Ann* D, int c, int g, int d){
  int j;
  for(j = A->first[c]; j >= 0; j = A->nxt[j]) if(A->tr[j].a == g && Bit(SetGet(&D->T, d), A->tr[j].v)) return 1;
  return 0;
}

/* rules of the annotated window system, from the configurations where f holds */
static void AnnRules(Ctx* X, McEngine* E, Aut* F, Ann* D, int flag){
  McModel* m = X->m;
  int c, g, d, r;
  for(c = 0; c < X->C; c++) for(g = 0; g < X->G; g++) for(d = 0; d < D->nD; d++){
    int a = g * D->nD + d;
    if(!AnnHolds(F, D, c, g, d)) continue;
    for(r = m->rstart[c * m->G + g]; r < m->rstart[c * m->G + g + 1]; r++){
      McRule* x = &m->rules[r];
      if(x->kind == MC_POP) McEngRule(E, MC_POP, c, a, x->c2, -1, -1, flag);
      else if(x->kind == MC_SW) McEngRule(E, MC_SW, c, a, x->c2, x->g1 * D->nD + d, -1, flag);
      else McEngRule(E, MC_PUSH, c, a, x->c2, x->g1 * D->nD + D->delta[d * X->G + x->g2], x->g2 * D->nD + d, flag);
    }
  }
}

/* back to plain letters, keeping the consistent annotations: node (x, t), t = the annotation required for
 * the rest of the stack (-1 at the top: any) */
static Aut* Project(Ctx* X, McEngine* E, bool* fin, Ann* D){
  Aut* R = AutNew(X->C, X->G);
  McMap map;
  int *qx = NULL, *qt = NULL, cap = 0, n = 0, qh = 0, c, a;
  McMapInit(&map);
  for(c = 0; c < X->C; c++){
    McMapPut(&map, c, -1, 0, c);
    if(n == cap){ cap = 2 * cap + 16; qx = (int*) realloc(qx, (size_t) cap * sizeof(int)); qt = (int*) realloc(qt, (size_t) cap * sizeof(int)); }
    qx[n] = c; qt[n] = -1; n++;
  }
  while(qh < n){
    int x = qx[qh], t = qt[qh], u = McMapGet(&map, x, t, 0), j;
    qh++;
    for(a = 0; a < E->A; a++){
      int g = a / D->nD, d = a % D->nD;
      if(t >= 0 && D->delta[d * X->G + g] != t) continue;
      for(j = McEngFirst(E, x, a); j >= 0; j = E->f[j].next){
        int y = E->f[j].y, v = McMapGet(&map, y, d, 0);
        if(v < 0){
          v = AutNode(R, fin[y] && d == D->d0);
          McMapPut(&map, y, d, 0, v);
          if(n == cap){ cap = 2 * cap + 16; qx = (int*) realloc(qx, (size_t) cap * sizeof(int)); qt = (int*) realloc(qt, (size_t) cap * sizeof(int)); }
          qx[n] = y; qt[n] = d; n++;
        }
        AutAdd(R, u, g, v);
      }
    }
  }
  free(qx); free(qt); McMapFree(&map);
  return Trim(X, R);
}

static Aut* EU(Ctx* X, Aut* F, Aut* G){
  Ann D;
  McEngine E;
  Aut* R;
  int i, d;
  AnnBuild(X, F, &D);
  McEngInit(&E, G->n, X->C, X->G * D.nD);
  for(i = 0; i < G->nt; i++) for(d = 0; d < D.nD; d++) McEngFact(&E, G->tr[i].u, G->tr[i].a * D.nD + d, G->tr[i].v, 0);
  AnnRules(X, &E, F, &D, 0);
  McEngRun(&E);
  R = Project(X, &E, G->fin, &D);
  McEngFree(&E); free(D.delta); SetFree(&D.T);
  AutFree(F); AutFree(G);
  return R;
}

static Aut* EG(Ctx* X, Aut* F){
  Ann D;
  McEngine E;
  Aut* R;
  bool *rep, *fin;
  int i, ACC = X->C;
  AnnBuild(X, F, &D);
  McEngInit(&E, X->C + 1, X->C, X->G * D.nD);
  AnnRules(X, &E, F, &D, 1);
  McEngRun(&E);
  rep = McEngRepeating(&E);
  for(i = 0; i < X->C * E.A; i++) if(rep[i]) McEngFact(&E, i / E.A, i % E.A, ACC, 0);
  for(i = 0; i < E.A; i++) McEngFact(&E, ACC, i, ACC, 0);
  McEngRun(&E);
  fin = (bool*) calloc((size_t) X->C + 1, sizeof(bool));
  fin[ACC] = true;
  R = Project(X, &E, fin, &D);
  free(fin); free(rep);
  McEngFree(&E); free(D.delta); SetFree(&D.T);
  AutFree(F);
  return R;
}

static Aut* True(Ctx* X){ McAtom a; memset(&a, 0, sizeof a); a.kind = MA_TRUE; return Atom(X, &a); }

static Aut* Eval(Ctx* X, McF* f){
  switch(f->op){
    case F_ATOM: return Atom(X, &f->atom);
    case F_NOT: return Not(X, Eval(X, f->a));
    case F_AND: return Prod(X, Eval(X, f->a), Eval(X, f->b), 0);
    case F_OR:  return Prod(X, Eval(X, f->a), Eval(X, f->b), 1);
    case F_IMP: return Prod(X, Not(X, Eval(X, f->a)), Eval(X, f->b), 1);
    case F_EX:  return EX(X, Eval(X, f->a));
    case F_AX:  return Not(X, EX(X, Not(X, Eval(X, f->a))));
    case F_EF:  return EU(X, True(X), Eval(X, f->a));
    case F_AG:  return Not(X, EU(X, True(X), Not(X, Eval(X, f->a))));
    case F_EG:  return EG(X, Eval(X, f->a));
    case F_AF:  return Not(X, EG(X, Not(X, Eval(X, f->a))));
    case F_EU:  return EU(X, Eval(X, f->a), Eval(X, f->b));
    default:{   /* A[f U g] = !( E[!g U (!f & !g)] | EG !g ) */
      Aut* u = EU(X, Not(X, Eval(X, f->b)), Prod(X, Not(X, Eval(X, f->a)), Not(X, Eval(X, f->b)), 0));
      return Not(X, Prod(X, u, EG(X, Not(X, Eval(X, f->b))), 1));
    }
  }
}

int CtlCheck(Fa* fa, Upds* upds, int k, const char* formula){
  McF* f = McParse(formula, 1);
  McModel M;
  Ctx X;
  Aut *bad, *init;
  int c, violated = 0;
  if(!f) return -1;
  if(k < 0){ k = MaxReadOffset(upds); if(McMaxOffset(f) > k) k = McMaxOffset(f); }
  McBuild(&M, fa, upds, k);
  McResolve(&M, f);
  X.m = &M; X.C = M.C; X.G = M.G; X.BOT = M.BOT;
  bad = Not(&X, Eval(&X, f));
  init = AutFromModel(&M);
  for(c = 0; c < M.C && !violated; c++)
    if(init->first[c] >= 0 || init->fin[c]) violated = McInitIntersect(init, bad, c, c, M.BOT);
  AutFree(bad); AutFree(init); McFree(&M); McFreeF(f);
  return !violated;
}
