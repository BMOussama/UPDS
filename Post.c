#include "Post.h"
#include <stdlib.h>
#include <stdio.h>
Fa * PostStar(Fa * fa , Upds * upds){
  Fa * postFa = (Fa*) malloc(sizeof(Fa*));
  postFa = fa;
  ListTransition * acc = GetAccTransitions(fa);
  Transition * t;
  ListRule* r=NULL ;
  ListTransition* pred;
  while(acc ){

    t= GetTheFirstTransition(acc);
    r= FindRulesPost(upds,t->label);
    pred= GetPredTransitions(postFa->listTransition,t->from);
    while(r){
      if(IsPopRule(r->rule)){
        postFa = PopCase(postFa ,r->rule ,t ,pred,acc);
      }else if ( isSwitchRule(r->rule)){
        postFa = SwitchCase(postFa ,r->rule ,t ,pred,acc,upds);
      }else if(isPushRule(r->rule)){
        postFa = PushCase(postFa ,r->rule ,t ,pred,acc);
      }
      r=r->next;
    }

    acc= DeleteFromListTransition(acc,t);
  }

  return postFa;
}

Fa* SwitchCase(Fa* postFa,Rule* rule, Transition* t, ListTransition* pred, ListTransition* acc, Upds* upds){
  Transition* trans,*trans1;
  if(!pred){

      trans = CreateTransition(t->from,t->to,rule->to_stack1,true);
      if(t->from->acc && !ExistTransition(postFa->listTransition,trans)) {
        AddToListTransition(acc,trans);
      FaInsert(postFa,trans);
    }
  }else{
    if(isSelfLoop(t)){
      ListTransition* prT=pred;
      while(prT){
        StateName* stateName = CreateStateName(t->to->name->name,t->to->name->fromAlphabet);
        State* state;
        if(t->to->pushed)
          state = CreateState(stateName,true,true,true);
        else
          state = CreateState(stateName,true,false,true);
        State* checkState = ExistState(postFa,state);
        if(checkState){
          state=checkState;
        }
        if(IsTheSameState(state,prT->transition->from) &&  ExistPopRule(upds->rules,rule->to_stack1))  {
          state=checkState;
          trans = CreateTransition(state,state,rule->to_stack1,false);
          FaInsert(postFa,trans);
          trans = CreateTransition(state,t->to,rule->to_stack1,false);
          FaInsert(postFa, trans);

          trans = CreateTransition(state,t->to,rule->to_stack1,true);
          if(!ExistTransition(postFa->listTransition, trans)){
            AddToListTransition(acc,trans);
            FaInsert(postFa, trans);
          }
          state->acc=true;
        } else if (t->from->pushed){
          trans = CreateTransition(t->from,t->from,rule->to_stack1,true);
          FaInsert(postFa, trans);
        }else{
          stateName = CreateStateName(t->from->name->name,t->from->name->fromAlphabet);
          if(t->from->pushed)
            state = CreateState(stateName,true,true,true);
          else
            state = CreateState(stateName,true,false,true);
          State* checkState = ExistState(postFa,state);
          if(checkState){
            state=checkState;
          }

          trans = CreateTransition(state,t->to,rule->to_stack1,true);
          trans1 = CreateTransition(prT->transition->from,state,prT->transition->label,false);
          FaInsert(postFa, trans1);
          if(!ExistTransition(postFa->listTransition, trans1)){
            FaInsert(postFa, trans);
            AddToListTransition(acc,trans);
            ListRule* listpushrule = ExistPushRule(upds->rules,rule->to_stack1);
            ListTransition* transitions = GetPredTransitions(postFa->listTransition,prT->transition->from);
            while(listpushrule){
              stateName = CreateStateName(prT->transition->from->name->name,listpushrule->rule->to_stack1);
              state = CreateState(stateName,true,true,true);
              State* checkState = ExistState(postFa,state);
              if(checkState){
                state=checkState;
              }
              if (prT->transition->from->duplicated && prT->transition->from->pushed && listpushrule){
                trans = CreateTransition(state,state,listpushrule->rule->to_stack1,false);
                FaInsert(postFa, trans);
                ListRule* switchlist= ExistSwitch(upds->rules,listpushrule->rule->to_stack1);
                PrintRules(switchlist);
                while(switchlist){
                  trans = CreateTransition(state,state,switchlist->rule->to_stack1,false);
                  FaInsert(postFa, trans);
                  switchlist=switchlist->next;
                }

             }
             listpushrule= listpushrule->next;
           }
          }



        }


        prT=prT->next;
      }

    }else{

      if(t->from->duplicated){
        printf("SwitchNOTLOOPDUP\n");
        trans = CreateTransition(t->from,t->to,t->label,false);
        FaInsert(postFa, trans);
        FaInsertList(acc,GetPostTransitions(postFa->listTransition,t->to));
        t->to->acc=true;
      }else{
        ListTransition* prT=pred;
        while(prT){
          StateName* stateName = CreateStateName(t->from->name->name,t->to->name->fromAlphabet);
          State* state;
          if(t->from->pushed)
            state = CreateState(stateName,true,true,true);
          else
            state = CreateState(stateName,true,false,true);
          State* checkState = ExistState(postFa,state);
          if(checkState){
            state=checkState;
          }
          trans = CreateTransition(state,t->to,rule->to_stack1,true);
          trans1 = CreateTransition(prT->transition->from,state,prT->transition->label,false);
          if(!ExistTransition(postFa->listTransition, trans)){
            FaInsert(postFa, trans);
            AddToListTransition(acc,trans);
          }
          FaInsert(postFa, trans1);
          prT=prT->next;
        }

      }
    }

  }
  return postFa;
}

Fa* PopCase(Fa* postFa,Rule* rule, Transition* t, ListTransition* pred, ListTransition* acc){
  Transition* trans;
  if(!pred){
    /* UPPER STACK EMPTY*/
    trans = CreateTransition(t->from,t->to,t->label,false);
    if(!ExistTransition(postFa->listTransition,trans)){
      FaInsert(postFa,trans);
      t->to->acc=true;
      FaInsertList(acc,GetPostTransitions(postFa->listTransition,t->to));
    }
  }else{
    if(isSelfLoop(t)){
      ListTransition* prT=pred;
      while(prT){

          StateName* stateName = CreateStateName(t->from->name->name,t->from->name->fromAlphabet);
          State* state;
          if(t->from->pushed)
            state = CreateState(stateName,true,true,true);
          else
            state = CreateState(stateName,true,false,true);
          State* checkState = ExistState(postFa,state);
          if(checkState)  {
            state=checkState;
          }

          if(IsTheSameState(state,prT->transition->from)){
              trans = CreateTransition(state,state,t->label,false);
              FaInsert(postFa,trans);
              trans = CreateTransition(state,t->to,t->label,false);
              FaInsert(postFa, trans);
          }else{

            trans = CreateTransition(state,t->to,t->label,false);
            if(!ExistTransition(postFa->listTransition,trans)){
                  FaInsert(postFa, trans);
                  t->to->acc=true;
                  FaConcat(acc,GetPostTransitions(postFa->listTransition,t->to));
                  trans = CreateTransition(prT->transition->from,state,prT->transition->label,false);
                  FaInsert(postFa, trans);
            }
          }
        prT=prT->next;
      }
    }else{
      if(t->from->duplicated){
        trans = CreateTransition(t->from,t->to,t->label,false);
        FaInsert(postFa, trans);
        FaInsertList(acc,GetPostTransitions(postFa->listTransition,t->to));
        t->to->acc=true;

      }else{
        ListTransition* prT=pred;
        while(prT){
          StateName* stateName = CreateStateName(t->from->name->name,t->from->name->fromAlphabet);
          State* state;
          if(t->from->pushed)
            state = CreateState(stateName,true,true,true);
          else
            state = CreateState(stateName,true,false,true);
          State* checkState = ExistState(postFa,state);
          if(checkState)  {
            state=checkState;
          }
          if(!IsTheSameState(state,prT->transition->from)){
            trans = CreateTransition(state,t->to,t->label,false);
            FaInsert(postFa, trans);
            trans = CreateTransition(prT->transition->from,state,prT->transition->label,false);
            FaInsert(postFa, trans);
            t->to->acc=true;
            FaInsertList(acc,GetPostTransitions(postFa->listTransition,t->to));
          }
            prT=prT->next;
        }

      }

    }
  }
  return postFa;
}

Fa* PushCase(Fa* postFa,Rule* rule, Transition* t, ListTransition* pred, ListTransition* acc){
  Transition* trans1,*trans2;

  if(!pred){
    StateName* stateName = CreateStateName(t->from->name->name,rule->to_stack1);
    State* state = CreateState(stateName,false,true,false);
    State* checkState = ExistState(postFa,state);
    if(checkState)  {
      state=checkState;
    }
    trans1 = CreateTransition(t->from,state,rule->to_stack1,true);
    if(!ExistTransition(postFa->listTransition,trans1)){
      FaInsert(postFa,trans1);
      AddToListTransition(acc,trans1);
    }
    trans2 = CreateTransition(state,t->to,rule->to_stack2,true);
    if(state->acc) {
      AddToListTransition(acc,trans2);
    }
    FaInsert(postFa,trans2);
  }else{
    if(isSelfLoop(t)){
      ListTransition* prT=pred;

      while(prT){
        if(prT->transition->from->duplicated && prT->transition->from->pushed){
          StateName* stateName = CreateStateName(t->from->name->name,rule->to_stack1);
          State* state = CreateState(stateName,false,true,false);
          State* checkState = ExistState(postFa,state);
          if(checkState)  {
            state=checkState;
          }

          trans1 = CreateTransition(prT->transition->from,state,rule->to_stack1,true);
          if(!ExistTransition(postFa->listTransition,trans1)){
            FaInsertList(acc,CreateListTransition(trans1));
            FaInsert(postFa,trans1);

          }
          trans2 = CreateTransition(state,t->to,rule->to_stack2,true);
          if(!ExistTransition(postFa->listTransition,trans1)){
            FaInsert(postFa,trans2);
          }

        }else{
          StateName* stateName = CreateStateName(prT->transition->from->name->name,rule->to_stack1);
          State* state = CreateState(stateName,false,true,false);
          State* checkState = ExistState(postFa,state);
          if(checkState)  {
            state=checkState;
          }
          trans1 = CreateTransition(prT->transition->from,state,rule->to_stack1,true);
          if(!ExistTransition(postFa->listTransition,trans1)){
            FaInsertList(acc,CreateListTransition(trans1));
            FaInsert(postFa,trans1);

          }
          trans2 = CreateTransition(state,t->to,rule->to_stack2,true);
          if(!ExistTransition(postFa->listTransition,trans1)){
            FaInsert(postFa,trans2);

          }
        }
        prT=prT->next;
      }

    }else{
      if(t->from->duplicated && t->from->pushed){
        ListTransition* prT=pred;
        StateName* stateName ;
        stateName = CreateStateName(t->from->name->name,rule->to_stack1);
        State* state = CreateState(stateName,false,true,false);
        State* checkState = ExistState(postFa,state);
        if(checkState)  {
          state=checkState;
        }
        trans1 = CreateTransition(t->from,state,rule->to_stack1,true);
        if(!ExistTransition(postFa->listTransition,trans1)){
          FaConcat(acc,CreateListTransition(trans1));
          FaInsert(postFa,trans1);

        }
        while(prT){
          trans1 = CreateTransition(prT->transition->from,state,rule->to_stack1,true);
          if(!ExistTransition(postFa->listTransition,trans1)){
            FaConcat(acc,CreateListTransition(trans1));
            FaInsert(postFa,trans1);

          }
          trans2 = CreateTransition(state,t->to,rule->to_stack2,true);

          FaInsert(postFa,trans2);
          prT=prT->next;
        }

      }else{

        ListTransition* prT=pred;
        State* state;
           while(prT ){
           if(prT->transition->from->duplicated && prT->transition->from->pushed && !IsTheSameState(prT->transition->from,prT->transition->to) ){
             StateName* stateName ;
             stateName = CreateStateName(prT->transition->from->name->name,rule->to_stack1);
             state = CreateState(stateName,false,true,false);
             State* checkState = ExistState(postFa,state);
             if(checkState)  {
               state=checkState;
             }
             trans1 = CreateTransition(prT->transition->from,state,rule->to_stack1,true);

             if(!ExistTransition(postFa->listTransition,trans1)){

               FaConcat(acc,CreateListTransition(trans1));
               FaInsert(postFa,trans1);
               trans2 = CreateTransition(state,t->to,rule->to_stack2,true);
               FaInsert(postFa,trans2);
             }

             if(HaveSelfLoop(postFa,t->from) && t->from->duplicated){
               trans1 = CreateTransition(t->from,state,rule->to_stack1,true);
                FaInsert(postFa,trans1);
                AddToListTransition(acc,trans1);
             }

           }else if ( !(prT->transition->from->duplicated && prT->transition->from->pushed) && !IsTheSameState(prT->transition->from,prT->transition->to) ){

              StateName* stateName ;

              if(prT->transition->from->duplicated && !strcmp(prT->transition->from->name->name,t->to->name->name) )
                {
                  stateName = CreateStateName(t->from->name->name,rule->to_stack1);
                }
              else{
                if(t->to->pushed)
                  stateName = CreateStateName(t->to->name->name,rule->to_stack1);
                else
                  stateName = CreateStateName(prT->transition->from->name->name,rule->to_stack1);
              }
              state = CreateState(stateName,false,true,false);
              State* checkState = ExistState(postFa,state);
              if(checkState)  {
                state=checkState;
              }

              trans1 = CreateTransition(prT->transition->from,state,rule->to_stack1,true);

              if(!ExistTransition(postFa->listTransition,trans1)){

                FaConcat(acc,CreateListTransition(trans1));
                FaInsert(postFa,trans1);
                trans2 = CreateTransition(state,t->to,rule->to_stack2,true);
                FaInsert(postFa,trans2);
              }

           }
           if(HaveSelfLoop(postFa,t->from) && t->from->duplicated){
             trans1 = CreateTransition(t->from,state,rule->to_stack1,true);
               FaInsert(postFa,trans1);
               AddToListTransition(acc,trans1);


           }
           prT=prT->next;
         }

      }

    }
  }
  return postFa;
}
