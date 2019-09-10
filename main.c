#include <stdlib.h>
#include <stdio.h>
#include "string.h"
#include <stdbool.h>
#include "Upds.h"
#include "Fa.h"
#include "Post.h"



int main () {
	StateName* stateName = CreateStateName("p","");
	State* state = CreateState(stateName,false,false,true);

	StateName* stateName1 = CreateStateName("q","");
	State* state1 = CreateState(stateName1,false,false,false);

	StateName* stateName2 = CreateStateName("q1","");
	State* state2 = CreateState(stateName2,false,false,false);

	StateName* stateName3 = CreateStateName("q2","a");
	State* state3 = CreateState(stateName3,false,false,false);

	Transition* T = CreateTransition(state,state1,"x",true);
	Transition* TT = CreateTransition(state1,state1,"x",true);
	Transition* TTT = CreateTransition(state3,state,"x",true);
	ListTransition * list=CreateListTransition(T);
		/*PrintListTransition(GetPostTransitions(list,state1));*/
		AddToListTransition(list,TT);

	Fa* f = CreateFa(list , state);
	ListTransition * hm;
	hm = GetAccTransitions(f);
  Transition* trans = GetTheFirstTransition(hm);
/*	printf("check\n" );*/

	ListRule* r= CreateListRule(CreateRule(state,"a",state1,NULL,NULL)) ;
	/*AddRule(r,CreateRule(state,"x",state1,"a",NULL));
	AddRule(r,CreateRule(state,"a",state1,NULL,NULL));
	AddRule(r,CreateRule(state,"b",state1,NULL,NULL));
	AddRule(r,CreateRule(state,"c",state1,NULL,NULL));
	AddRule(r,CreateRule(state,"a",state1,"b","a"));
	AddRule(r,CreateRule(state,"b",state1,"c","b"));
	AddRule(r,CreateRule(state,"c",state1,"m",NULL));*/
	AddRule(r,CreateRule(state,"x",state1,"a",NULL));
	AddRule(r,CreateRule(state,"a",state1,NULL,NULL));
	AddRule(r,CreateRule(state,"b",state1,NULL,NULL));
	AddRule(r,CreateRule(state,"c",state1,NULL,NULL));
	AddRule(r,CreateRule(state,"a",state1,"a","b"));
	AddRule(r,CreateRule(state,"b",state1,"c",NULL));
	Upds* upds= CreateUpds(r);
	 /*printf("%d\n",IsTheSameRule(r[3],r[0]));*/
	/*PrintUpds(upds);*/

	/*Rule** r1 = FindRulesPost(upds,"a");
	r1 = DeleteRule(r1,r[0]);
	r1 = DeleteRule(r1,r[3]);
	PrintRules(r1);*/
	/*PrintRules(FindRulesPre(upds,"a"));
	PrintUpds(upds);*/

  /*	PrintListTransition(hm[0]);*/
		PrintFa(PostStar(f,upds));


  return EXIT_SUCCESS;
}
