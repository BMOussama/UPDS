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
	int read_offset;	/* 0: no condition; j >= 1: the rule applies only if the j-th upper letter
				   above the stack pointer ([esp-4j]) is read_letter (see RuleSetRead) */
	char* read_letter;

} Rule;

typedef struct ListRule{
	Rule * rule;
	struct ListRule * next;
}ListRule;

typedef struct Upds{
  ListRule*  rules;
  char** alphabet;     /* stack alphabet Gamma (optional, see UpdsSetAlphabet) */
  int nalphabet;
} Upds;



Rule* CreateRule(State* from_state,char* from_stack, State* to_state,char* to_stack1, char* to_stack2);
/* Read condition: the rule applies only when the j-th letter of the upper stack, counted from the stack
 * pointer (j = 1 is the letter just above it, i.e. [esp-4]), is `letter`. Supported by the window model
 * (Window.h), where it is exact when j <= k; SaturatePost / SaturatePre ignore it (over-approximation). */
void RuleSetRead(Rule* rule, int j, char* letter);
int HasReadRules(Upds* upds);
int MaxReadOffset(Upds* upds);

Upds* CreateUpds(ListRule* rules);
/* Declares the stack alphabet Gamma. Needed by pre*: a push overwrites ANY letter of Gamma, including
 * letters that appear in no rule. Without it, Gamma = letters of the rules and of the automaton. */
void UpdsSetAlphabet(Upds* upds, char** letters, int n);
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
