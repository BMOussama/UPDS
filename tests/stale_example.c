/* Example: a function reads a stale value left above the stack pointer by an earlier call.
 *
 * Lower-stack letters are program points (one control state p):
 *   m0  main, before calling f        m1  main, after f returned
 *   f0  f, entry                      f1  f, after allocating its local
 *   v   f's local value               z   the same cell, cleared
 *   g0  g, which reads [esp-4]        ok / bad  g's two outcomes
 * Rules:
 *   m0 -> f0 m1   call f (push)          f0 -> f1 v   f allocates its local (push)
 *   f1 -> eps     f returns (pop)        v  -> eps    its local is popped (it stays above the pointer)
 *   m1 -> g0      main continues with g
 *   g0 -> bad     only if [esp-4] = v    (read rule: g sees f's stale local)
 *   g0 -> ok      only if [esp-4] = y, for every other letter y
 * Fixed version: f clears its local before popping it: v -> z (switch), z -> eps.
 *
 * Property:  AG not bad   (CTL)   =   G not bad   (LTL)   =   not EF bad
 * checked as: post*(C) intersected with { <p, Gamma*, bad Gamma*> } is empty. */
#include <stdio.h>
#include <stdbool.h>
#include "Upds.h"
#include "Fa.h"
#include "Window.h"
#include "Check.h"
#include "Ltl.h"
#include "Ctl.h"

static char* G[] = { "m0", "m1", "f0", "f1", "v", "z", "g0", "ok", "bad" };
#define NG 9

static State* S(char* n){ return CreateState(CreateStateName(n, ""), false, false, false); }

static Upds* Program(State* p, int fixed){
  ListRule* r = CreateListRule(CreateRule(p, "m0", p, "f0", "m1"));
  Rule* rule;
  int i;
  AddRule(r, CreateRule(p, "f0", p, "f1", "v"));
  AddRule(r, CreateRule(p, "f1", p, NULL, NULL));
  if(fixed){ AddRule(r, CreateRule(p, "v", p, "z", NULL)); AddRule(r, CreateRule(p, "z", p, NULL, NULL)); }
  else AddRule(r, CreateRule(p, "v", p, NULL, NULL));
  AddRule(r, CreateRule(p, "m1", p, "g0", NULL));
  rule = CreateRule(p, "g0", p, "bad", NULL); RuleSetRead(rule, 1, "v"); AddRule(r, rule);
  for(i = 0; i < NG; i++) if(G[i][0] != 'v'){
    rule = CreateRule(p, "g0", p, "ok", NULL); RuleSetRead(rule, 1, G[i]); AddRule(r, rule);
  }
  {
    Upds* u = CreateUpds(r);
    UpdsSetAlphabet(u, G, NG);
    return u;
  }
}

/* { <p, Gamma*, top Gamma*> }: the configurations where the program is at point `top` */
static Fa* AtPoint(char* top){
  State *p = S("p"), *f = S("f");
  ListTransition* l = CreateListTransition(CreateTransition(p, f, top, true));
  int i;
  SetFinal(f, true);
  for(i = 0; i < NG; i++){
    AddToListTransition(l, CreateTransition(p, p, G[i], false));
    AddToListTransition(l, CreateTransition(f, f, G[i], true));
  }
  return CreateFa(l, p);
}

static const char* LTL[] = {
  "G !at.bad",                      /* the property */
  "F at.ok",                        /* g takes its normal branch */
  "G (at.g0 -> up1.v)",             /* when g runs, [esp-4] still holds f's local */
  "F at.end",                       /* the program ends (no: ok / bad block, i.e. repeat forever) */
  "at.m0 & X at.f0 & X X at.f1",
  "!at.g0 U at.g0",
  "F G (at.ok | at.bad)",
  "G F at.f0"
};
static const char* CTL[] = {
  "AG !at.bad", "EF at.bad", "AF at.ok", "EF at.ok", "AG (at.g0 -> AX at.bad)",
  "AG (at.g0 -> up1.v)", "A[!at.g0 U at.m1]", "AG EF (at.ok | at.bad)", "EG !at.bad", "AX AX at.f1"
};
#define NLTL (int)(sizeof LTL / sizeof *LTL)
#define NCTL (int)(sizeof CTL / sizeof *CTL)

int main(void){
  int fixed, i, k;
  for(fixed = 0; fixed <= 1; fixed++){
    State *p = S("p"), *q = S("q");
    Fa *init, *reach;
    Upds* upds = Program(p, fixed);
    SetFinal(q, true);
    init = CreateFa(CreateListTransition(CreateTransition(p, q, "m0", true)), p);   /* C = <p, eps, m0> */
    reach = WindowPostStar(init, upds, WINDOW_AUTO);
    printf("%s version: EF bad = %s  ->  AG not bad %s\n", fixed ? "fixed" : "buggy",
           FaIntersects(reach, AtPoint("bad")) ? "true " : "false",
           FaIntersects(reach, AtPoint("bad")) ? "VIOLATED (g can read f's stale local)" : "HOLDS");
    printf("              EF ok  = %s\n", FaIntersects(reach, AtPoint("ok")) ? "true" : "false");
    for(k = WINDOW_AUTO; k <= 2; k += 3){        /* k = auto (1), then 2 (every upper stack fits) */
      printf("  window k = %s\n", k < 0 ? "auto" : "2");
      for(i = 0; i < NLTL; i++) printf("    LTL %-32s %s\n", LTL[i], LtlCheck(init, upds, k, LTL[i]) == 1 ? "holds" : "VIOLATED");
      for(i = 0; i < NCTL; i++) printf("    CTL %-32s %s\n", CTL[i], CtlCheck(init, upds, k, CTL[i]) == 1 ? "holds" : "VIOLATED");
    }
  }
  return 0;
}
