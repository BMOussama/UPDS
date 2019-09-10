
#ifndef FA_H
#define FA_H


#include "Transition.h"
#include "State.h"

typedef struct Fa Fa;

struct Fa
{
	ListTransition* listTransition;
  State* initial;
};


Fa* CreateFa(ListTransition* ListTransition, State* intial);

void FaInsert(Fa* fa,Transition* T);
void FaInsertList(ListTransition* list1, ListTransition* list);
void FaConcat(ListTransition* list1, ListTransition* list);
ListTransition * GetAccTransitions(Fa* fa);
ListTransition * GetPredTransitions(ListTransition* list,State* to_state);
ListTransition * GetPostTransitions(ListTransition* list, State* from_state);
State * ExistState(Fa* fa, State* state);

ListTransition* GetPredDupPush(Fa*fa,State* state);

ListTransition* ChangeListToState(ListTransition* list, State* state);
int HaveSelfLoop(Fa* fa, State* state);
void PrintFa(Fa* fa);
#endif
