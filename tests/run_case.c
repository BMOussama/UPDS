/* Generic driver used by tests/fuzz.py.
 * Reads a case from stdin and prints the post* or pre* automaton.
 * Input lines:
 *   MODE post|pre                  corrected saturation (Saturation.h)
 *   MODE wpost K | wpre K          bounded-window model with a window of K letters (Window.h)
 *   INIT p                         initial state (control state)
 *   F    s                         final state
 *   A    x y z ...                 stack alphabet Gamma
 *   T from U|L label to            transition, U = upper (label,eps), L = lower (eps,label)
 *   R lhs                          pop    <p,lhs> -> <p,eps>
 *   R lhs a                        switch <p,lhs> -> <p,a>
 *   R lhs a b                      push   <p,lhs> -> <p,ab>
 *   Q j x lhs [a [b]]              the same rules, applied only if the j-th upper letter is x
 *   MODE wpost auto                window size = largest read offset (WINDOW_AUTO)                 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "Upds.h"
#include "Fa.h"
#include "Post.h"
#include "Pre.h"
#include "Saturation.h"
#include "Window.h"

static char* interned[256]; static int nInterned = 0;
static char* Intern(const char* s){            /* same string -> same pointer (Post.c compares labels by pointer) */
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
  char line[256], a[64], b[64], c[64], d[64], e[64];
  int post = 1, window = -2;
  State* init = NULL;
  ListTransition* l = NULL;
  ListRule* r = NULL;
  Fa* fa;
  char* alphabet[64];
  int nalpha = 0;
  while(fgets(line,sizeof line,stdin)){
    int n = sscanf(line,"%63s %63s %63s %63s %63s",a,b,c,d,e);
    if(n<=0) continue;
    if(!strcmp(a,"MODE")){
      post = !strcmp(b,"post") || !strcmp(b,"wpost");
      if(b[0]=='w') window = n>=3 ? (strcmp(c,"auto") ? atoi(c) : WINDOW_AUTO) : 2;
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
  {
    UpdsStatus status;
    Upds* upds = CreateUpds(r);
    UpdsSetAlphabet(upds,alphabet,nalpha);
    fa = CreateFa(l,init);
    if(window != -2){
      fa = post ? WindowPostStar(fa,upds,window) : WindowPreStar(fa,upds,window);
      printf("STATUS window\n");
    }else{
      fa = post ? SaturatePost(fa,upds,UPDS_DEFAULT_CAP,&status)
                : SaturatePre(fa,upds,UPDS_DEFAULT_CAP,&status);
      printf("STATUS %s\n", status==UPDS_EXACT ? "exact" : "over");
    }
  }
  PrintFa(fa);
  return 0;
}
