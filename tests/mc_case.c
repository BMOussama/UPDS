/* Driver used by tests/fuzz_mc.py: reads a case (format of tests/run_case.c, without MODE) and checks
 * formulas on the window model.
 *   K n | K auto       window size
 *   LTL formula        prints "LTL 1" (holds), "LTL 0" (violated) or "LTL -1" (syntax error)
 *   CTL formula        same with CTL
 *   BUCHI formula      prints the Buchi automaton of the negation (debugging) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "Upds.h"
#include "Fa.h"
#include "Window.h"
#include "Ltl.h"
#include "Ctl.h"

static char* interned[256]; static int nInterned = 0;
static char* Intern(const char* s){
  int i; char* c;
  for(i=0;i<nInterned;i++) if(!strcmp(interned[i],s)) return interned[i];
  c = (char*) malloc(strlen(s)+1); strcpy(c,s); interned[nInterned++] = c; return c;
}
static State* states[256]; static int nStates = 0;
static State* GetState(const char* n){
  int i;
  for(i=0;i<nStates;i++) if(!strcmp(states[i]->name->name,n)) return states[i];
  states[nStates] = CreateState(CreateStateName(Intern(n),Intern("")),false,false,false);
  return states[nStates++];
}

int main(void){
  char line[1024], a[64], b[64], c[64], d[64], e[64];
  int k = WINDOW_AUTO, nq = 0, i;
  char* queries[256];
  State* init = NULL;
  ListTransition* l = NULL;
  ListRule* r = NULL;
  Fa* fa;
  Upds* upds;
  char* alphabet[64];
  int nalpha = 0;
  while(fgets(line,sizeof line,stdin)){
    int n = sscanf(line,"%63s %63s %63s %63s %63s",a,b,c,d,e);
    if(n<=0) continue;
    if(!strcmp(a,"K")) k = strcmp(b,"auto") ? atoi(b) : WINDOW_AUTO;
    else if(!strcmp(a,"LTL") || !strcmp(a,"CTL") || !strcmp(a,"BUCHI")){
      line[strcspn(line,"\n")] = 0;
      queries[nq] = (char*) malloc(strlen(line)+1); strcpy(queries[nq++], line);
    }
    else if(!strcmp(a,"INIT")){ init = GetState(b); init->acc = true; }
    else if(!strcmp(a,"F")) SetFinal(GetState(b),true);
    else if(!strcmp(a,"A")){
      char* tok = strtok(line+1," \t\n");
      while(tok && nalpha<64){ alphabet[nalpha++] = Intern(tok); tok = strtok(NULL," \t\n"); }
    }
    else if(!strcmp(a,"T") && n==5){
      Transition* t = CreateTransition(GetState(b),GetState(e),Intern(d),!strcmp(c,"L"));
      if(!l) l = CreateListTransition(t); else AddToListTransition(l,t);
    }else if(!strcmp(a,"Q") && n>=4){
      char f[64], g[64], h[64], i2[64], j2[64];
      int m = sscanf(line,"%63s %63s %63s %63s %63s %63s",f,g,h,i2,j2,e);
      Rule* rule = CreateRule(init,Intern(i2),init,m>=5?Intern(j2):NULL,m>=6?Intern(e):NULL);
      RuleSetRead(rule,atoi(g),Intern(h));
      if(!r) r = CreateListRule(rule); else AddRule(r,rule);
    }else if(!strcmp(a,"R")){
      Rule* rule = CreateRule(init,Intern(b),init,n>=3?Intern(c):NULL,n>=4?Intern(d):NULL);
      if(!r) r = CreateListRule(rule); else AddRule(r,rule);
    }
  }
  upds = CreateUpds(r);
  UpdsSetAlphabet(upds,alphabet,nalpha);
  fa = CreateFa(l,init);
  for(i = 0; i < nq; i++){
    char* q = queries[i];
    if(!strncmp(q,"LTL",3)) printf("LTL %d\n", LtlCheck(fa,upds,k,q+3));
    else if(!strncmp(q,"CTL",3)) printf("CTL %d\n", CtlCheck(fa,upds,k,q+3));
    else LtlPrintAutomaton(q+5);
  }
  return 0;
}
