#include <stdlib.h>
#include <stdio.h>
#include "State.h"
#include <stdbool.h>

State* CreateState(StateName* name,bool duplicated, bool pushed, bool acc){
    State* state = (State*) malloc(sizeof(State));
    state->name = name;
    state->duplicated = duplicated;
    state->pushed = pushed;
    state->acc = acc;
    return state;
}

StateName* CreateStateName(char* name,char* fromAlphabet){
  StateName* stateName = (StateName*) malloc(sizeof(StateName));
  stateName->name = name;

  stateName-> fromAlphabet = fromAlphabet;
  stateName->duplicated= false;
  return stateName;
}
ListState* InsertState(State* state){
  ListState* s= (ListState*) malloc(sizeof(ListState));
  s-> next = NULL;
  s-> state = state;
}



void DeleteStateName(StateName* stateName){
    free(stateName);
}

void DeleteState(State* state){
    DeleteStateName(state->name);
    free(state);
}

void PrintState(State* state){
  if (state->duplicated && state->pushed && !state->name->duplicated) printf("D(%s,%s)",state->name->name,state->name->fromAlphabet);
  if (state->duplicated && state->pushed && state->name->duplicated) printf("D(D%s,%s)",state->name->name,state->name->fromAlphabet);
  if (state->pushed && !state->duplicated && !state->name->duplicated) printf("q(%s,%s)",state->name->name,state->name->fromAlphabet);
  if (state->duplicated && !state->pushed) printf("D%s",state->name->name);
  if (!state->duplicated && !state->pushed) printf("%s",state->name->name);
}

void PrintListState(ListState* listState){
  ListState* cpt=listState;
  while(cpt->next){
    PrintState(cpt->state);
    printf("--");
    cpt= cpt-> next;
  }
  PrintState(cpt->state);
}


int IsTheSameState(State* s1, State* s2){
  if ( !strcmp(s1->name->name ,s2->name->name) && !strcmp(s1->name->fromAlphabet ,s2->name->fromAlphabet)
  && (s1->duplicated ==s2->duplicated) && (s1->pushed ==s2->pushed) ) return 1;
  return 0;
}
