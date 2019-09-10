
#ifndef STATE_H
#define STATE_H

#include <stdbool.h>

typedef struct Lists ListState;

typedef struct StateName
{
	char* name;
  char* fromAlphabet;
  bool * duplicated;
} StateName;

/*States*/
typedef struct State
{

	StateName* name;
  bool duplicated;
  bool pushed;
  bool acc;


} State;

struct Lists
{
	struct Lists* next;
	State* state;
};




State* CreateState(StateName* name,bool duplicated, bool pushed, bool acc);
StateName* CreateStateName(char* name,char* FromAlphabet);
void DeleteStateName(StateName* stateName);
void DeleteState(State* state);
void PrintState(State* state);
void PrintListState(ListState* liststate);
ListState* InsertState(State* state);

int IsTheSameState(State* s1, State* s2);

#endif
