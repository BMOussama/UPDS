#include <stdlib.h>
#include <stdio.h>
#include "Upds.h"
#include "State.h"
#include "string.h"
Rule* CreateRule(State* from_state,char* from_stack, State* to_state,char* to_stack1, char* to_stack2){
  Rule* l = (Rule*) malloc(sizeof(Rule));
  l-> from_state = from_state;
  l -> from_stack = from_stack;
  l -> to_state = to_state;

  if(to_stack1) l -> to_stack1 = to_stack1;
  else l -> to_stack1 = NULL;
  if(to_stack2) l -> to_stack2 = to_stack2;
  else l -> to_stack2 = NULL;
  return l;
}

Upds* CreateUpds(ListRule* rules){
  Upds* upds = (Upds*) malloc(sizeof(Upds));
  upds -> rules = rules;
  return upds;
}

ListRule* CreateListRule(Rule* r){
  ListRule* list = (ListRule*) malloc(sizeof(ListRule*));
  list -> rule = r;
  list -> next = NULL;
}

void AddRule(ListRule* list , Rule* r){
  ListRule* list1 = list;
  ListRule* pred;
  int i=0;
  while(list1){
    if(IsTheSameRule(list1->rule,r)) i=1;
    pred=list1;
    list1= list1 -> next;
  }
  if(i==0) pred->next=CreateListRule(r);
}

void PrintUpds(Upds* upds){
  ListRule* r = upds->rules;
  printf("--Rules--\n");
  while(r){
    PrintRule(r->rule);
    r=r->next;
  }
}

void PrintRules(ListRule* r){
  ListRule* rules = r;
  while(rules){
    PrintRule(rules->rule);
    rules=rules->next;
  }
}

void PrintRule(Rule* r){
  if(r){
    printf("<");
    PrintState(r->from_state);
    printf(",%s> -> <",r->from_stack);
    PrintState(r->to_state);
    if(r->to_stack1) printf(",%s", r-> to_stack1);
    if(r->to_stack2) printf("%s", r-> to_stack2);
    printf(">\n");
  }
}



ListRule* FindRulesPost(Upds* r,char* from_stack){
  ListRule * found=NULL;
  ListRule * check = r -> rules;
  while(check ){
    if(!strcmp(check -> rule->from_stack , from_stack)){
       if(!found) found=CreateListRule(check->rule);
       else AddRule(found,check->rule);
     }
    check= check->next;

  }

  return found;
}

void DeleteRule(ListRule*rules,Rule* rule){
  ListRule* r =rules;
  ListRule* pred=rules;
  while(r && !IsTheSameRule(rule,r->rule)){
    pred=r;
    r=r->next;
  }
  if(r){
    pred->next=r->next;
  }
}

int IsTheSameRule(Rule* r1, Rule* r2){
  if(IsTheSameState(r1->from_state,r2->from_state) && IsTheSameState(r1->to_state,r2->to_state) &&
  !strcmp(r1->from_stack,r2->from_stack) ){
    if(r1->to_stack1 == NULL && r2->to_stack1==NULL){
      return 1;
    }else if (r1->to_stack1 != NULL && r2->to_stack1!=NULL){
      if(!strcmp(r1->to_stack1,r2->to_stack1)){
        if(r1->to_stack2 == NULL && r2->to_stack2==NULL){
          return 1;
        }else if (r1->to_stack2 != NULL && r2->to_stack2!=NULL){
          if(!strcmp(r1->to_stack2,r2->to_stack2))
            return 1;
          return 0;
        }else{
          return 0;
        }
      }else{
        return 0;
      }
    }else{
      return 0;
    }
    return 1;
  }
  return 0;
}

Rule * GetFirstRule(ListRule* r1){
  if(r1)
    return r1->rule;
}

int IsPopRule(Rule* r){
  if(r->to_stack1 == NULL)
    return 1;
  return 0;
}

int isPushRule(Rule*r){
  if(r->to_stack1 != NULL && r->to_stack2!=NULL)
    return 1;
  return 0;
}

int isSwitchRule(Rule* r){
  if(r->to_stack2 == NULL && r->to_stack1!= NULL)
    return 1;
  return 0;
}

int ExistPopRule(ListRule* r,char* from_stack){
  ListRule* rules = r;
  while(rules){
    if(IsPopRule(rules->rule)){
      if(!strcmp(rules->rule->from_stack,from_stack)){
        return 1;
      }
    }
    rules=rules->next;
  }
  return 0;
}

ListRule* ExistPushRule(ListRule* r,char* from_stack){
  ListRule* rules = r;
  ListRule* copyList=CopyRules(r);
  ListRule* list1=NULL;
  while(rules){
    if(isPushRule(rules->rule)){
      if(!strcmp(rules->rule->from_stack,from_stack) && rules->rule->to_stack1 &&
  rules->rule->to_stack2 && ExistPopRule(r,rules->rule->to_stack1) && ExistPopRule(r,rules->rule->to_stack2) ){
        if(!list1) {
          list1=CreateListRule(rules->rule);
          DeleteRule(copyList,rules->rule);
          return ConcatRules(list1,ExistPushRule(copyList,rules->rule->to_stack1));
        }else {
          AddRule(list1,rules->rule);
          DeleteRule(copyList,rules->rule);
          return ConcatRules(list1,ExistPushRule(copyList,rules->rule->to_stack1));
        }
      }
    }
    rules=rules->next;
  }
  if(!list1) return NULL;
}

ListRule* ExistSwitch(ListRule* list, char* from_stack){
  ListRule* rules = list;
  ListRule* copyList=CopyRules(list);
  ListRule* list1=NULL;

  while(rules){
    if(isSwitchRule(rules->rule)){
      if(!strcmp(rules->rule->from_stack,from_stack) && ExistPopRule(list,rules->rule->to_stack1)){
        if(!list1) {
          list1=CreateListRule(rules->rule);
          DeleteRule(copyList,rules->rule);
          return ConcatRules(list1,ExistSwitch(copyList,rules->rule->to_stack1));
        }
        else{
           AddRule(list1,rules->rule);
           DeleteRule(copyList,rules->rule);
           return ConcatRules(list1,ExistSwitch(copyList,rules->rule->to_stack1));
         }
      }
    }
    rules=rules->next;
  }

  if(!list1) return NULL;
}

ListRule* ConcatRules(ListRule* list1, ListRule* list2){
  ListRule* list = list2;
  while(list){
    AddRule(list1,list->rule);
    list=list->next;
  }
  return list1;
}
ListRule* GetPushRulesList(Upds* upds){
  ListRule* rules = upds->rules;
  ListRule* new=NULL;
  while(rules){
    if(isPushRule(rules->rule)){
        if(!new) new=CreateListRule(rules->rule);
        else AddRule(new,rules->rule);
    }
    rules=rules->next;
  }
  return new;
}

ListRule* FindListRulesByLabel(Upds* upds,char* stack ){
  ListRule* list = upds->rules;
  ListRule* check= NULL;
  while(list){
    if(!strcmp(list->rule->from_stack,stack)){
      if(!check) check = CreateListRule(list->rule);
      else AddRule(check,list->rule);
    }
    list=list->next;
  }
  return check;
}

ListRule* CopyRules(ListRule* list){
  ListRule* list1= CreateListRule(list->rule);
  ListRule* check = list->next;
  while(check){
    AddRule(list1,check->rule);
    check=check->next;
  }
  return list1;
}
