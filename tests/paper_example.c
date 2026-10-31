#include <stdlib.h>
#include <stdbool.h>
#include "Upds.h"
#include "Fa.h"
#include "Post.h"
/* Report example: r1 <p,x>-><p,a>, r2 <p,a>-><p,eps>, r3 <p,b>-><p,eps>, r4 <p,a>-><p,ab>; c0 = <p, eps, x x*> */
int main(void){
  State* p = CreateState(CreateStateName("p",""),false,false,true);
  State* q = CreateState(CreateStateName("q",""),false,false,false);
  ListTransition* l = CreateListTransition(CreateTransition(p,q,"x",true));
  Fa* fa; ListRule* r; Upds* upds;
  SetFinal(q,true);
  AddToListTransition(l, CreateTransition(q,q,"x",true));
  fa = CreateFa(l,p);
  r = CreateListRule(CreateRule(p,"x",p,"a",NULL));
  AddRule(r, CreateRule(p,"a",p,NULL,NULL));
  AddRule(r, CreateRule(p,"b",p,NULL,NULL));
  AddRule(r, CreateRule(p,"a",p,"a","b"));
  upds = CreateUpds(r);
  PrintFa(PostStar(fa,upds));
  return 0;
}
