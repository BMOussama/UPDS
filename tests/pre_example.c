#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "Upds.h"
#include "Fa.h"
#include "Pre.h"
/* pre* examples with the report's rules:
 *   r1 <p,x>-><p,a>  r2 <p,a>-><p,eps>  r3 <p,b>-><p,eps>  r4 <p,a>-><p,ab>
 * Usage: ./pre_example 1   C = <p, eps, abb>        (report example, final state f)
 *        ./pre_example 2   C = <p, a b*, x>         (non-empty upper stack)
 *        ./pre_example 3   C = <p, b, a x*>                                   */
static State* S(char* n){ return CreateState(CreateStateName(n,""),false,false,false); }

int main(int argc, char** argv){
  int ex = argc>1 ? atoi(argv[1]) : 1;
  State *p=S("p"), *q1=S("q1"), *q2=S("q2"), *f=S("f");
  ListTransition* l;
  ListRule* r;
  Fa* fa;
  p->acc=true;
  SetFinal(f,true);
  if(ex==1){
    l = CreateListTransition(CreateTransition(p,q1,"a",true));
    AddToListTransition(l, CreateTransition(q1,q2,"b",true));
    AddToListTransition(l, CreateTransition(q2,f,"b",true));
  }else if(ex==2){
    l = CreateListTransition(CreateTransition(p,q1,"a",false));
    AddToListTransition(l, CreateTransition(q1,q1,"b",false));
    AddToListTransition(l, CreateTransition(q1,f,"x",true));
  }else{
    l = CreateListTransition(CreateTransition(p,q1,"b",false));
    AddToListTransition(l, CreateTransition(q1,f,"a",true));
    AddToListTransition(l, CreateTransition(f,f,"x",true));
  }
  fa = CreateFa(l,p);
  r = CreateListRule(CreateRule(p,"x",p,"a",NULL));
  AddRule(r, CreateRule(p,"a",p,NULL,NULL));
  AddRule(r, CreateRule(p,"b",p,NULL,NULL));
  AddRule(r, CreateRule(p,"a",p,"a","b"));
  PrintFa(PreStar(fa,CreateUpds(r)));
  return 0;
}
