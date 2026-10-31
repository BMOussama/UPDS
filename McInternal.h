#ifndef MC_INTERNAL_H
#define MC_INTERNAL_H
/* Shared machinery of the LTL (Ltl.c) and CTL (Ctl.c) model checkers on the window model.
 *
 * The window model (Window.h) is an ordinary pushdown system whose control states are the windows
 * c = (w, deep). For model checking, the lower stack gets a bottom letter BOT (never popped), so that
 * "the lower stack is empty" is the head <c, BOT>, and every run is made infinite:
 *   - <c, BOT> stutters (switch BOT -> BOT);
 *   - <c, g> stutters when no rule SURELY applies, i.e. when some upper stack represented by c may block
 *     every rule for g (a rule whose read offset is below the window may not apply).
 * Atoms are simple valuations (they depend on the window and on the top lower letter only). */
#include "WindowInternal.h"

/* ---------- hash map: (k1, k2, k3) -> int ---------- */
typedef struct { int *k1, *k2, *k3, *val; int cap, n; } McMap;
void McMapInit(McMap* m);
void McMapFree(McMap* m);
int  McMapGet(McMap* m, int k1, int k2, int k3);          /* -1 if absent */
void McMapPut(McMap* m, int k1, int k2, int k3, int val);  /* insert or overwrite */

/* ---------- the window pushdown system ---------- */
enum { MC_POP, MC_SW, MC_PUSH };
typedef struct { int kind, c2, g1, g2; } McRule;           /* <c,g> -> <c2,eps> | <c2,g1> | <c2,g1 g2> */

typedef struct {
  WSat s; State** fst; int nf;
  int C, G, BOT;                /* C windows; letters 0..G-1, BOT = G-1 = s.na */
  int* rstart; McRule* rules;   /* rules at <c,g>: rules[rstart[c*G+g]] .. rules[rstart[c*G+g+1]-1] */
} McModel;

void McBuild(McModel* m, Fa* fa, Upds* upds, int k);
void McFree(McModel* m);
int  McFindLetter(McModel* m, const char* name);          /* -1 if not a letter of the model */

/* ---------- formulas (LTL and CTL share the syntax of atoms and boolean operators) ---------- */
enum { MA_TRUE, MA_FALSE, MA_AT, MA_END, MA_UP };
typedef struct { int kind, j, letter; char* name; } McAtom;   /* letter: resolved by McResolve */

enum { F_ATOM, F_NOT, F_AND, F_OR, F_IMP,
       F_X, F_F, F_G, F_U, F_R,                               /* LTL */
       F_EX, F_AX, F_EF, F_AF, F_EG, F_AG, F_EU, F_AU };      /* CTL */
typedef struct McF { int op; struct McF *a, *b; McAtom atom; } McF;

McF* McParse(const char* text, int ctl);     /* NULL on a syntax error (a message is printed) */
void McFreeF(McF* f);
int  McMaxOffset(McF* f);                    /* largest J of the atoms upJ.x */
void McResolve(McModel* m, McF* f);          /* letters of the atoms */
void McPrintAtom(McAtom* a);
/* value of an atom at <c,g>: 1 true, 0 false, 2 unknown (upJ.x with the J-th letter below the window) */
int  McAtomValue(McModel* m, McAtom* a, int c, int g);

/* ---------- pushdown saturation engine (pre*, Schwoon), with acceptance flags ----------
 * Nodes 0..N-1, the first P are control states; letters 0..A-1. A fact (x,a,y,flag) is a transition
 * x --a--> y of the pre* automaton; for y a control, flag = 1 when the run that pops a from x to y can
 * visit a configuration whose control has flag set (Buchi acceptance). */
typedef struct { int x, a, y, flag, next; } McFact;
typedef struct { int kind, sx, sa, tx, b1, b2, flag, next; } McPRule;
typedef struct {
  int N, P, A;
  McFact* f; int nf, capf;
  McPRule* r; int nr, capr;
  McMap facts, heads, byTgt, derived;
  int* work; int nw, capw;
} McEngine;

void McEngInit(McEngine* e, int N, int P, int A);
void McEngFree(McEngine* e);
void McEngRule(McEngine* e, int kind, int sx, int sa, int tx, int b1, int b2, int flag);
void McEngFact(McEngine* e, int x, int a, int y, int flag);
void McEngRun(McEngine* e);
int  McEngFirst(McEngine* e, int x, int a);  /* first fact at (x,a), then e->f[i].next; -1 = end */
/* heads (x,a), x < P, from which a run visits flagged controls infinitely often without ever popping a:
 * returns an array of P*A booleans (to free) */
bool* McEngRepeating(McEngine* e);

/* ---------- finite automata over configurations <c, l BOT> ---------- */
typedef struct { int u, a, v; } McTr;
typedef struct { int n, G; bool* fin; McTr* tr; int nt, cap; int* first; int* nxt; McMap dedup; } Aut;
Aut* AutNew(int n, int G);
int  AutNode(Aut* A, bool fin);
void AutAdd(Aut* A, int u, int a, int v);
void AutIndex(Aut* A);                       /* builds first/nxt: transitions by source */
void AutFree(Aut* A);
Aut* AutFromModel(McModel* m);               /* initial configurations L(fa) (without BOT) */
Aut* AutFromEngine(McEngine* e, bool* fin);
/* is some initial configuration <c, l> such that A accepts l BOT from node start ? */
int  McInitIntersect(Aut* init, Aut* A, int c, int start, int BOT);

#endif
