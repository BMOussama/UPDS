#ifndef WINDOW_INTERNAL_H
#define WINDOW_INTERNAL_H
/* Internals of the window model, shared with Ltl.c and Ctl.c. */
#include "Fa.h"
#include "Upds.h"

#define EPS (-1)

typedef struct { int f, l, t; } WTr;

typedef struct {
  /* letters */
  char** let; int na, capA;
  /* windows / control states */
  int k, W, C;
  int* off;              /* off[len] = number of windows shorter than len */
  int* wlen;             /* per control */
  int* wlet;             /* per control, k letters */
  /* PDS automaton */
  int nn, capNodes;      /* nodes: 0..C-1 controls, then the others */
  bool* final;
  WTr* tr; int nt, capT;
  int* htab; int hcap;
  int* pushKey; int npush, capPush;   /* push node (control*na + letter) -> node, stored in pushNode */
  int* pushNode;
} WSat;

void WBuild(WSat* s, Fa* fa, Upds* upds, int k, int post, State*** pfst, int* pnf);
int WLetter(WSat* s, char* x);
int PopCtrl(WSat* s, int c, int g);
int EraseCtrl(WSat* s, int c);
int ReadOk(WSat* s, int c, Rule* rule);

#endif
