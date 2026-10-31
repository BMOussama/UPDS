/* LTL model checking on the window model: see Ltl.h.
 *
 * 1. The negation of the formula is put in negation normal form and translated into a Buchi automaton
 *    (tableau of Gerth, Peled, Vardi and Wolper, then degeneralization). Its states are labelled by
 *    literals; a state must be consistent with the configuration it reads.
 * 2. Product with the window pushdown system (McInternal.h): control states (window, Buchi state).
 *    <(c,b), g> -> <(c',b'), w>  for each rule <c,g> -> <c',w> of the window model, each edge b -> b',
 *    when the label of b holds at <c,g>. Every run of the window model is infinite (stuttering).
 * 3. Accepting runs (Bouajjani, Esparza, Maler; Schwoon): a head <p,g> is repeating when a run from it
 *    comes back to <p,g w> through an accepting control. The heads and the pop summaries are computed by
 *    the saturation engine, the repeating heads by the strongly connected components of the head graph;
 *    an accepting run exists from a configuration iff it is in pre*(repeating heads . Gamma*).
 * 4. The formula is violated iff some initial configuration, with BOT below its lower stack and a
 *    Buchi initial state, is in that pre* set. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Ltl.h"
#include "McInternal.h"

/* ---------- negation normal form, hash-consed ---------- */
enum { N_TRUE, N_FALSE, N_LIT, N_AND, N_OR, N_X, N_U, N_R };
#define MAXSUB 256
#define WORDS (MAXSUB / 32)
typedef struct { int op, a, b, atom, pos; } Sub;     /* N_LIT: atom index, pos = 1 (atom) or 0 (negated) */
typedef struct { unsigned w[WORDS]; } Bits;

typedef struct {
  Sub sub[MAXSUB]; int ns;
  McAtom* atoms[MAXSUB]; int na;
  int overflow;
} Clo;

static int SameAtom(McAtom* x, McAtom* y){
  if(x->kind != y->kind || x->j != y->j) return 0;
  if(!x->name || !y->name) return x->name == y->name;
  return !strcmp(x->name, y->name);
}

static int Find(Clo* L, int op, int a, int b, int atom, int pos){
  int i;
  for(i = 0; i < L->ns; i++){
    Sub* s = &L->sub[i];
    if(s->op == op && s->a == a && s->b == b && s->atom == atom && s->pos == pos) return i;
  }
  return -1;
}

static int Mk(Clo* L, int op, int a, int b, int atom, int pos){
  int i = Find(L, op, a, b, atom, pos);
  if(i >= 0) return i;
  if(L->ns == MAXSUB){ L->overflow = 1; return 0; }
  L->sub[L->ns].op = op; L->sub[L->ns].a = a; L->sub[L->ns].b = b;
  L->sub[L->ns].atom = atom; L->sub[L->ns].pos = pos;
  return L->ns++;
}

static int AtomIndex(Clo* L, McAtom* x){
  int i;
  for(i = 0; i < L->na; i++) if(SameAtom(L->atoms[i], x)) return i;
  L->atoms[L->na] = x;
  return L->na++;
}

static int Nnf(Clo* L, McF* f, int neg){
  int a, b;
  switch(f->op){
    case F_ATOM:
      if(f->atom.kind == MA_TRUE) return Mk(L, neg ? N_FALSE : N_TRUE, -1, -1, -1, 0);
      if(f->atom.kind == MA_FALSE) return Mk(L, neg ? N_TRUE : N_FALSE, -1, -1, -1, 0);
      return Mk(L, N_LIT, -1, -1, AtomIndex(L, &f->atom), !neg);
    case F_NOT: return Nnf(L, f->a, !neg);
    case F_AND: a = Nnf(L, f->a, neg); b = Nnf(L, f->b, neg); return Mk(L, neg ? N_OR : N_AND, a, b, -1, 0);
    case F_OR:  a = Nnf(L, f->a, neg); b = Nnf(L, f->b, neg); return Mk(L, neg ? N_AND : N_OR, a, b, -1, 0);
    case F_IMP: a = Nnf(L, f->a, !neg); b = Nnf(L, f->b, neg); return Mk(L, neg ? N_AND : N_OR, a, b, -1, 0);
    case F_X:   a = Nnf(L, f->a, neg); return Mk(L, N_X, a, -1, -1, 0);
    case F_F:   a = Mk(L, neg ? N_FALSE : N_TRUE, -1, -1, -1, 0); b = Nnf(L, f->a, neg);
                return Mk(L, neg ? N_R : N_U, a, b, -1, 0);
    case F_G:   a = Mk(L, neg ? N_TRUE : N_FALSE, -1, -1, -1, 0); b = Nnf(L, f->a, neg);
                return Mk(L, neg ? N_U : N_R, a, b, -1, 0);
    case F_U:   a = Nnf(L, f->a, neg); b = Nnf(L, f->b, neg); return Mk(L, neg ? N_R : N_U, a, b, -1, 0);
    case F_R:   a = Nnf(L, f->a, neg); b = Nnf(L, f->b, neg); return Mk(L, neg ? N_U : N_R, a, b, -1, 0);
    default: L->overflow = 1; return 0;
  }
}

/* ---------- tableau ---------- */
static int Has(Bits* s, int i){ return (s->w[i / 32] >> (i % 32)) & 1u; }
static void Put(Bits* s, int i){ s->w[i / 32] |= 1u << (i % 32); }
static void Del(Bits* s, int i){ s->w[i / 32] &= ~(1u << (i % 32)); }
static int Empty(Bits* s){ int i; for(i = 0; i < WORDS; i++) if(s->w[i]) return 0; return 1; }
static int Same(Bits* x, Bits* y){ return !memcmp(x, y, sizeof(Bits)); }

typedef struct { int* inc; int ninc, cap; Bits nw, old, next; } GNode;

static void IncAdd(GNode* n, int id){
  int i;
  for(i = 0; i < n->ninc; i++) if(n->inc[i] == id) return;
  if(n->ninc == n->cap){ n->cap = n->cap ? 2 * n->cap : 4; n->inc = (int*) realloc(n->inc, (size_t) n->cap * sizeof(int)); }
  n->inc[n->ninc++] = id;
}

static GNode Copy(GNode* n){
  GNode c = *n;
  c.inc = (int*) malloc((size_t)(n->cap ? n->cap : 1) * sizeof(int));
  if(n->ninc) memcpy(c.inc, n->inc, (size_t) n->ninc * sizeof(int));
  return c;
}

typedef struct {
  Clo L;
  GNode* nodes; int nn, capn;      /* tableau nodes; id of nodes[i] is i + 1, id 0 = the initial marker */
  /* degeneralized Buchi automaton: state b = (node, counter) */
  int NB, m;                       /* m = number of until subformulas */
  int* untils;
  int* bnode; bool* bacc; bool* binit;
  int* sstart; int* succ;          /* successors of b: succ[sstart[b]] .. succ[sstart[b+1]-1] */
} Buchi;

static void AddNew(Clo* L, GNode* n, int f){ (void) L; if(!Has(&n->old, f)) Put(&n->nw, f); }

static void Tableau(Buchi* B, int root){
  Clo* L = &B->L;
  GNode* stack = NULL;
  int sp = 0, capS = 0;
  GNode start;
  memset(&start, 0, sizeof(GNode));
  IncAdd(&start, 0);
  Put(&start.nw, root);
#define PUSHN(N) do { if(sp == capS){ capS = capS ? 2 * capS : 16; stack = (GNode*) realloc(stack, (size_t) capS * sizeof(GNode)); } stack[sp++] = (N); } while(0)
  PUSHN(start);
  while(sp > 0){
    GNode q = stack[--sp];
    int eta, i;
    if(Empty(&q.nw)){
      for(i = 0; i < B->nn; i++) if(Same(&B->nodes[i].old, &q.old) && Same(&B->nodes[i].next, &q.next)) break;
      if(i < B->nn){
        int j;
        for(j = 0; j < q.ninc; j++) IncAdd(&B->nodes[i], q.inc[j]);
        free(q.inc);
      }else{
        GNode s;
        if(B->nn == B->capn){ B->capn = B->capn ? 2 * B->capn : 16; B->nodes = (GNode*) realloc(B->nodes, (size_t) B->capn * sizeof(GNode)); }
        B->nodes[B->nn++] = q;
        memset(&s, 0, sizeof(GNode));
        IncAdd(&s, B->nn);
        s.nw = q.next;
        PUSHN(s);
      }
      continue;
    }
    for(eta = 0; !Has(&q.nw, eta); eta++);
    Del(&q.nw, eta);
    if(Has(&q.old, eta)){ PUSHN(q); continue; }
    {
      Sub s = L->sub[eta];
      GNode q2;
      int neg;
      switch(s.op){
        case N_FALSE: free(q.inc); break;
        case N_TRUE: Put(&q.old, eta); PUSHN(q); break;
        case N_LIT:
          neg = Find(L, N_LIT, -1, -1, s.atom, !s.pos);
          if(neg >= 0 && Has(&q.old, neg)){ free(q.inc); break; }
          Put(&q.old, eta); PUSHN(q); break;
        case N_AND: AddNew(L, &q, s.a); AddNew(L, &q, s.b); Put(&q.old, eta); PUSHN(q); break;
        case N_X: Put(&q.next, s.a); Put(&q.old, eta); PUSHN(q); break;
        case N_OR: case N_U: case N_R:
          q2 = Copy(&q);
          Put(&q.old, eta); Put(&q2.old, eta);
          if(s.op == N_OR){ AddNew(L, &q, s.a); AddNew(L, &q2, s.b); }
          else if(s.op == N_U){ AddNew(L, &q, s.a); Put(&q.next, eta); AddNew(L, &q2, s.b); }
          else{ AddNew(L, &q, s.b); Put(&q.next, eta); AddNew(L, &q2, s.a); AddNew(L, &q2, s.b); }
          PUSHN(q); PUSHN(q2);
          break;
      }
    }
  }
#undef PUSHN
  free(stack);
}

/* generalized -> ordinary Buchi: b = node * m' + i, counter i in 0..m'-1 (m' = max(m, 1)) */
static void Degeneralize(Buchi* B){
  int i, n, b, mm, ns = 0, cap = 0;
  B->m = 0;
  B->untils = (int*) malloc((size_t) B->L.ns * sizeof(int) + 1);
  for(i = 0; i < B->L.ns; i++) if(B->L.sub[i].op == N_U) B->untils[B->m++] = i;
  mm = B->m ? B->m : 1;
  B->NB = B->nn * mm;
  B->bnode = (int*) malloc((size_t)(B->NB + 1) * sizeof(int));
  B->bacc = (bool*) calloc((size_t) B->NB + 1, sizeof(bool));
  B->binit = (bool*) calloc((size_t) B->NB + 1, sizeof(bool));
  B->sstart = (int*) malloc((size_t)(B->NB + 1) * sizeof(int));
  B->succ = NULL;
#define INF(N, I) (!B->m || Has(&B->nodes[N].old, B->L.sub[B->untils[I]].b) || !Has(&B->nodes[N].old, B->untils[I]))
  for(b = 0; b < B->NB; b++){
    int nd = b / mm, ci = b % mm, nc;
    B->bnode[b] = nd;
    B->bacc[b] = ci == 0 && INF(nd, 0);
    for(i = 0; i < B->nodes[nd].ninc; i++) if(B->nodes[nd].inc[i] == 0 && ci == 0) B->binit[b] = true;
    nc = INF(nd, ci) ? (ci + 1) % mm : ci;
    B->sstart[b] = ns;
    for(n = 0; n < B->nn; n++){                       /* edges nd -> n: nd + 1 is in the incoming set of n */
      for(i = 0; i < B->nodes[n].ninc; i++) if(B->nodes[n].inc[i] == nd + 1) break;
      if(i == B->nodes[n].ninc) continue;
      if(ns == cap){ cap = cap ? 2 * cap : 16; B->succ = (int*) realloc(B->succ, (size_t) cap * sizeof(int)); }
      B->succ[ns++] = n * mm + nc;
    }
  }
  B->sstart[B->NB] = ns;
#undef INF
}

static void BuchiFree(Buchi* B){
  int i;
  for(i = 0; i < B->nn; i++) free(B->nodes[i].inc);
  free(B->nodes); free(B->untils); free(B->bnode); free(B->bacc); free(B->binit); free(B->sstart); free(B->succ);
}

/* Buchi automaton of the negation of f; 0 if the formula is too large */
static int BuildBuchi(Buchi* B, McF* f){
  int root;
  memset(B, 0, sizeof(Buchi));
  root = Nnf(&B->L, f, 1);
  if(B->L.overflow){ fprintf(stderr, "LTL: formula too large\n"); return 0; }
  Tableau(B, root);
  Degeneralize(B);
  return 1;
}

/* can the literals of tableau node nd hold at <c,g> ? Unknown upper letters may take any value, but a
 * position holds one letter */
static int LabelOk(Buchi* B, McModel* m, int nd, int c, int g){
  int pos[64], i, pass;
  for(i = 0; i < 64; i++) pos[i] = -1;
  for(pass = 0; pass < 2; pass++)
    for(i = 0; i < B->L.ns; i++){
      Sub* s = &B->L.sub[i];
      McAtom* a;
      int v;
      if(s->op != N_LIT || !Has(&B->nodes[nd].old, i)) continue;
      if((pass == 0) != (s->pos == 1)) continue;          /* positive literals first */
      a = B->L.atoms[s->atom];
      v = McAtomValue(m, a, c, g);
      if(v == 2){
        if(s->pos){ if(pos[a->j] >= 0 && pos[a->j] != a->letter) return 0; pos[a->j] = a->letter; }
        else if(pos[a->j] == a->letter) return 0;
      }else if(v != s->pos) return 0;
    }
  return 1;
}

int LtlCheck(Fa* fa, Upds* upds, int k, const char* formula){
  McF* f = McParse(formula, 0);
  Buchi B;
  McModel M;
  McEngine E;
  Aut *init, *pre;
  bool *rep, *fin;
  int c, g, b, i, P, ACC, violated = 0;
  if(!f) return -1;
  if(!BuildBuchi(&B, f)){ McFreeF(f); return -1; }
  if(k < 0){ k = MaxReadOffset(upds); if(McMaxOffset(f) > k) k = McMaxOffset(f); }
  McBuild(&M, fa, upds, k);
  McResolve(&M, f);

  P = M.C * B.NB; ACC = P;
  McEngInit(&E, P + 1, P, M.G);
  for(c = 0; c < M.C; c++) for(g = 0; g < M.G; g++) for(b = 0; b < B.NB; b++){
    int x = c * B.NB + b, r, j;
    if(B.sstart[b] == B.sstart[b + 1] || !LabelOk(&B, &M, B.bnode[b], c, g)) continue;
    for(r = M.rstart[c * M.G + g]; r < M.rstart[c * M.G + g + 1]; r++){
      McRule* x2 = &M.rules[r];
      for(j = B.sstart[b]; j < B.sstart[b + 1]; j++)
        McEngRule(&E, x2->kind, x, g, x2->c2 * B.NB + B.succ[j], x2->g1, x2->g2, B.bacc[b]);
    }
  }
  McEngRun(&E);                                    /* pop summaries */
  rep = McEngRepeating(&E);
  for(i = 0; i < P * M.G; i++) if(rep[i]) McEngFact(&E, i / M.G, i % M.G, ACC, 0);
  for(g = 0; g < M.G; g++) McEngFact(&E, ACC, g, ACC, 0);
  McEngRun(&E);                                    /* pre* of the repeating heads */

  fin = (bool*) calloc((size_t) P + 1, sizeof(bool));
  fin[ACC] = true;
  pre = AutFromEngine(&E, fin);
  init = AutFromModel(&M);
  for(c = 0; c < M.C && !violated; c++){
    if(init->first[c] < 0 && !init->fin[c]) continue;
    for(b = 0; b < B.NB && !violated; b++) if(B.binit[b]) violated = McInitIntersect(init, pre, c, c * B.NB + b, M.BOT);
  }
  AutFree(pre); AutFree(init); free(fin); free(rep);
  McEngFree(&E); McFree(&M); BuchiFree(&B); McFreeF(f);
  return !violated;
}

void LtlPrintAutomaton(const char* formula){
  McF* f = McParse(formula, 0);
  Buchi B;
  int b, i, j;
  if(!f) return;
  if(!BuildBuchi(&B, f)){ McFreeF(f); return; }
  printf("Buchi automaton of !(%s): %d states, %d until subformulas\n", formula, B.NB, B.m);
  for(b = 0; b < B.NB; b++){
    int first = 1;
    printf("  s%d%s%s  [", b, B.binit[b] ? " init" : "", B.bacc[b] ? " acc" : "");
    for(i = 0; i < B.L.ns; i++){
      Sub* s = &B.L.sub[i];
      if(s->op != N_LIT || !Has(&B.nodes[B.bnode[b]].old, i)) continue;
      printf("%s%s", first ? "" : " & ", s->pos ? "" : "!");
      McPrintAtom(B.L.atoms[s->atom]);
      first = 0;
    }
    printf("%s]  ->", first ? "true" : "");
    for(j = B.sstart[b]; j < B.sstart[b + 1]; j++) printf(" s%d", B.succ[j]);
    printf("\n");
  }
  BuchiFree(&B); McFreeF(f);
}
