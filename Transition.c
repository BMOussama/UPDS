#include <stdlib.h>
#include <stdio.h>
#include "State.h"
#include "Transition.h"
#include <stdbool.h>

Transition* CreateTransition(State* from, State* to,char* label,bool lower){
  Transition* T = (Transition*)malloc(sizeof(Transition));
  T->from = from;
  T->to = to;
  T -> label = label;
  T-> lower = lower;
  T-> list = NULL;
  return T;
}

void PrintListTransition(ListTransition* list){
  ListTransition* cpt=list;
    while(cpt){
      PrintTransition(cpt->transition);
      printf("\n");
      cpt = cpt->next;
    }
}

void AddNextTransition(Transition* T, Transition* next){
  ListTransition* l = CreateListTransition(next);
  ListTransition* permute;
  if(!T->list){
    T->list=l;
  }else{
    permute=T->list;
    while(permute->next){
      permute = permute->next;
    }
    permute->next=l;
  }
}
void AddPostTransition(Transition* T, Transition* post){
  ListTransition* l = CreateListTransition(post);
  ListTransition* permute;
  if(!T->postList){
    T->postList=l;
  }else{
    permute=T->postList;
    while(permute->next){
      permute = permute->next;
    }
    permute->next=l;
  }
}

void PrintTransition(Transition* T){
  printf("(");
  PrintState(T-> from);
  if(T->lower) printf(",(.,%s),",T->label);
  else printf(",(%s,.),",T->label);
  PrintState(T-> to);
  printf(")");
}
void PrintNextTransition(Transition* T){
    ListTransition* cpt=T->list;
    if(cpt){
      do{
        PrintTransition(cpt->transition);
        printf("\n");
        cpt = cpt->next;
      } while(cpt->next);
      PrintTransition(cpt->transition);
    }
}
void PrintPostTransition(Transition* T){
  ListTransition* cpt=T->postList;
    do{
      PrintTransition(cpt->transition);
      printf("\n");
      cpt = cpt->next;
    } while(cpt);
}

bool isSelfLoop(Transition* t){
  if(IsTheSameState(t->from ,t-> to)) return true;
  return false;
}

ListState* getPostListState(Transition* T){
  ListTransition* cpt=T->postList;
  ListState* listState ;
  if(cpt){
    listState = InsertState(cpt->transition->from);
    while(cpt->next){
      cpt = cpt->next;
      listState->next = InsertState(cpt->transition->from);
    }
  return listState;
  }else return NULL;
}

void DeleteTranistion(Transition* T){

}
ListTransition* CreateListTransition(Transition* T){
  ListTransition* l = (ListTransition*) malloc(sizeof(ListTransition));
  l -> next = NULL;
  l -> transition = T;
}

void AddToListTransition(ListTransition* list, Transition* t){
  ListTransition* permute = list->next;
  ListTransition* pred=list;
  ListTransition* toAdd= CreateListTransition(t);

  if(!permute ){
    if(!IsTheSameTransition(list->transition,t))
      list->next=toAdd;
  }else{
    while(permute && !IsTheSameTransition(pred->transition,t) ){
      pred=permute;
      permute=permute->next;
    }
    if(!IsTheSameTransition(pred->transition,t))
      pred->next=toAdd;
  }
}

ListTransition* DeleteFromListTransition(ListTransition* list, Transition* t){
  ListTransition* newList = list;
  ListTransition* pred=newList;

 if( newList->transition == t)
 {
   list = list->next;
 }else{
    while((newList->transition != t) && (newList)){
      pred = newList;
      newList= newList-> next;
    }

    if( (newList->transition == t) ){
        pred->next = newList->next;newList->next=NULL;
    }
  }
  return list;
}

Transition* GetTheFirstTransition(ListTransition* list){
  Transition* t = list-> transition;
  return t;
}

int IsTheSameTransition(Transition* t1 , Transition* t2){
  if ( IsTheSameState(t1->from,t2->from) && IsTheSameState(t1->to,t2->to) &&
  (t1->label == t2-> label) && (t1->lower == t2->lower))
    return 1;
  return 0;
}

int ExistTransition(ListTransition* list, Transition* t){
  ListTransition* list1 = list;
  while (list1) {
    if(IsTheSameTransition(list1->transition,t))
      return 1;
    list1=list1->next;
  }
  return 0;
}
