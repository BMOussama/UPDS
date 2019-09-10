
#ifndef TRANSITION_H
#define TRANSITION_H

#include "State.h"
typedef struct Trans Transition;
typedef struct List ListTransition;

struct Trans
{

  State* from;
  State* to;
  char* label;
  bool lower;

	ListTransition* list;	/* for listing all transitions in the FA   */
  ListTransition* postList;
};

struct List
{
	struct List* next;	/* for listing all transitions in the FA   */
  Transition* transition;
};



Transition* CreateTransition(State* from, State* to,char* label,bool lower);

/* insert */
void AddNextTransition(Transition* T, Transition* next);
void AddPostTransition(Transition* T, Transition* post);
/* ************************* */

/* affichage */
void PrintTransition(Transition* T);
void PrintNextTransition(Transition* T);
void PrintPostTransition(Transition* T);
void PrintListTransition(ListTransition* list);
/* ************************* */

/* verification */
bool isSelfLoop(Transition* t);
/* ************************* */

/* Manipulation */
ListState* getPostListState(Transition* T);
/*  ******************** */
void DeleteTranistion(Transition* T);

ListTransition* CreateListTransition(Transition* T);
void AddToListTransition(ListTransition* list, Transition* t);
ListTransition* DeleteFromListTransition(ListTransition* list, Transition* t);

Transition* GetTheFirstTransition(ListTransition* list);

int IsTheSameTransition(Transition* t1 , Transition* t2);
int ExistTransition(ListTransition* list, Transition* t);


#endif
