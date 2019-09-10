#include <stdlib.h>
#include <stdio.h>
#include "Transition.h"
#include "State.h"
#include "Fa.h"



Fa* CreateFa(ListTransition* ListTransition, State* initial){
  Fa* f = (Fa*) malloc(sizeof(Fa));
  f-> listTransition = ListTransition;
  f -> initial = initial;
  return f;
}

void PrintFa(Fa* f){
  printf("---Initial state--- \n");
  PrintState(f->initial);
  printf("\n---Transitions list---\n");
  PrintListTransition(f-> listTransition);
}

void FaInsert(Fa* fa,Transition* T){
  AddToListTransition(fa->listTransition,T);
}

void FaInsertList(ListTransition* list1, ListTransition* list){
  ListTransition* list2= list;
  while(list2){
    AddToListTransition(list1,list2->transition);
    list2=list2->next;
  }
}

void FaConcat(ListTransition* list1, ListTransition* list){
  ListTransition * pred;
  while(list1){
    pred=list1;
    list1=list1->next;
  }
  while(list){
    pred->next=CreateListTransition(list->transition);
    list=list->next;
  }

}

ListTransition * GetAccTransitions(Fa* fa){

  ListTransition * list1 =NULL;
  ListTransition * check = fa->listTransition;
  Transition * t = fa->listTransition->transition;

  while(check){

      if ( t->from->acc == true && t->lower==true){
        if (!list1) list1 = CreateListTransition(t);
        else AddToListTransition(list1,t);

      }
      check = check-> next;
      if(check)
        t= check-> transition;
  }
  return list1;
}

ListTransition * GetPredTransitions(ListTransition* list,State* to_state){
  ListTransition * list1 = NULL;
  ListTransition * list2 = list;
  while(list2){
    if(IsTheSameState(list2->transition->to,to_state) && !list2->transition->lower){
      if(!list1) list1=CreateListTransition(list2->transition);
      else AddToListTransition(list1,list2->transition);
    }
    list2=list2->next;
  }
  return list1;
}

ListTransition * GetPostTransitions(ListTransition* list, State* from_state){
  ListTransition * list1 = NULL;
  ListTransition * list2 = list;
  while(list2){
    if(IsTheSameState(list2->transition->from,from_state)  ){
      if(!list1) list1=CreateListTransition(list2->transition);
      else AddToListTransition(list1,list2->transition);
    }
    list2=list2->next;
  }
  return list1;
}
ListTransition* GetPredDupPush(Fa*fa,State* state){
	ListTransition* pred = GetPredTransitions(fa->listTransition,state);
	ListTransition* result=NULL;
	while(pred){
		if(pred->transition->from->pushed && pred->transition->from->duplicated){
			if(!result) result = CreateListTransition(pred->transition);
			else AddToListTransition(result,pred->transition);
		}
		pred=pred->next;
	}
	return result;
}
/*void LoopMySelf(Fa* postFa,ListState* listState, ListTransition* listTransition, State* p){
  if(!ExistState(listState,state)){
    InsertState(listState,state);
    AddToListTransition(postFA,)
    ListTransition* list= listTransition;
    while(list){

      list=list->next;
    }
  }
}*/

State * ExistState(Fa* fa, State* state){
  ListTransition* list = fa->listTransition;
  while(list){
      if(IsTheSameState(list->transition->from,state))
        return  list->transition->from;
      if(IsTheSameState(list->transition->to,state))
        return  list->transition->to;
      list= list->next;
  }
  return NULL;
}

ListTransition* ChangeListToState(ListTransition* list, State* state){
  ListTransition* list1=list;
  ListTransition* list2=NULL;
  while(list1){
    if(!list2) list2=CreateListTransition(CreateTransition(list1->transition->from,state,list1->transition->label,list1->transition->lower));
    else AddToListTransition(list2,CreateTransition(list1->transition->from,state,list1->transition->label,list1->transition->lower));
    list1=list1->next;
  }
  return list2;
}

int HaveSelfLoop(Fa* fa, State* state){
  ListTransition * list = fa->listTransition;
  while(list){
    if(IsTheSameState(state,list->transition->from) && IsTheSameState(state,list->transition->from) &&
    list->transition->lower==false)
      return 1;
    list=list->next;
  }
  return 0;
}
