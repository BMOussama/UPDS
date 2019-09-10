#ifndef UPDS_H
#define UPDS_H
#include "State.h"

typedef struct Rule
{
	State* from_state;	/* left-hand-side control location	*/
	char* from_stack;	/* left-hand-side stack symbol		*/
	State* to_state;	/* right-hand-side control location	*/
	char* to_stack1;	/* right-hand-side first stack symbol	*/
	char* to_stack2;	/* right-hand-side second stack symbol	*/

} Rule;

typedef struct ListRule{
	Rule * rule;
	struct ListRule * next;
}ListRule;

typedef struct Upds{
  ListRule*  rules;
} Upds;



Rule* CreateRule(State* from_state,char* from_stack, State* to_state,char* to_stack1, char* to_stack2);
Upds* CreateUpds(ListRule* rules);
ListRule* CreateListRule(Rule* r);
void AddRule(ListRule* list , Rule* r);
void PrintUpds(Upds* upds);
void PrintRule(Rule* r);
void PrintRules(ListRule * r);

ListRule* FindRulesPost(Upds* r , char* from_stack);
void DeleteRule(ListRule*rules,Rule* rule);
int IsTheSameRule(Rule* r1, Rule* r2);

Rule * GetFirstRule(ListRule* r1);

int IsPopRule(Rule* r);
int isPushRule(Rule*r);
int isSwitchRule(Rule* r);
int ExistPopRule(ListRule* r,char* from_stack);
ListRule* ExistPushRule(ListRule* r,char* from_stack);
ListRule* GetPushRulesList(Upds* upds);
ListRule* FindListRulesByLabel(Upds* upds,char* stack );
ListRule* CopyRules(ListRule* list);
ListRule* ExistSwitch(ListRule* list, char* from_stack);
ListRule* ConcatRules(ListRule* list1, ListRule* list2);
#endif
