/* Bounded-window model of a UPDS: see Window.h. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Window.h"



#include "WindowInternal.h"

static void* WGrow(void* p, int* capacity, int need, size_t size){
  if(need <= *capacity) return p;
  while(*capacity < need) *capacity = *capacity ? 2 * *capacity : 64;
  return realloc(p, (size_t)(*capacity) * size);
}

int WLetter(WSat* s, char* x){
  int i;
  for(i = 0; i < s->na; i++) if(!strcmp(s->let[i], x)) return i;
  s->let = (char**) WGrow(s->let, &s->capA, s->na + 1, sizeof(char*));
  s->let[s->na] = x;
  return s->na++;
}

/* ---------- windows ---------- */

static int Ctrl(WSat* s, const int* w, int len, int deep){
  int i, v = 0;
  for(i = 0; i < len; i++) v = v * s->na + w[i];
  return deep * s->W + s->off[len] + v;
}

static void BuildWindows(WSat* s){
  int len, i, p = 1, c;
  s->off = (int*) malloc((size_t)(s->k + 2) * sizeof(int));
  s->off[0] = 0;
  for(len = 0; len <= s->k; len++){ s->off[len + 1] = s->off[len] + p; p *= s->na; }
  s->W = s->off[s->k + 1];
  s->C = 2 * s->W;
  s->wlen = (int*) malloc((size_t) s->C * sizeof(int));
  s->wlet = (int*) calloc((size_t) s->C * (size_t)(s->k ? s->k : 1), sizeof(int));
  for(c = 0; c < s->C; c++){
    int idx = c % s->W, v;
    len = 0;
    while(idx >= s->off[len + 1]) len++;
    v = idx - s->off[len];
    s->wlen[c] = len;
    for(i = len - 1; i >= 0; i--){ s->wlet[c * s->k + i] = v % s->na; v /= s->na; }
  }
}

int PopCtrl(WSat* s, int c, int g){       /* upper stack w -> last k letters of w.g */
  int w[64], len = s->wlen[c], deep = c >= s->W, i;
  for(i = 0; i < len; i++) w[i] = s->wlet[c * s->k + i];
  if(s->k == 0) return Ctrl(s, w, 0, 1);
  if(len < s->k){ w[len] = g; return Ctrl(s, w, len + 1, deep); }
  for(i = 0; i + 1 < len; i++) w[i] = w[i + 1];
  w[len - 1] = g;
  return Ctrl(s, w, len, 1);
}

int EraseCtrl(WSat* s, int c){             /* push: the last upper letter is overwritten */
  int w[64], len = s->wlen[c], deep = c >= s->W, i;
  for(i = 0; i < len; i++) w[i] = s->wlet[c * s->k + i];
  return Ctrl(s, w, len > 0 ? len - 1 : 0, deep);
}

/* read condition of a rule at window c: exact inside the window; below it (deep) the letter is unknown */
int ReadOk(WSat* s, int c, Rule* rule){
  int j = rule->read_offset, len = s->wlen[c];
  if(j <= 0) return 1;
  if(j <= len) return s->wlet[c * s->k + len - j] == WLetter(s, rule->read_letter);
  return c >= s->W;
}

/* ---------- PDS automaton ---------- */

static int NewNode(WSat* s, bool fin){
  int c = s->capNodes;
  s->final = (bool*) WGrow(s->final, &c, s->nn + 1, sizeof(bool));
  s->capNodes = c;
  s->final[s->nn] = fin;
  return s->nn++;
}

static unsigned WHash(int f, int l, int t){
  unsigned h = (unsigned) f * 2654435761u ^ ((unsigned)(l + 2) * 97u + (unsigned) t * 40503u);
  h *= 2246822519u;
  return h ^ (h >> 15);
}

static void WRehash(WSat* s, int cap){
  int i;
  free(s->htab);
  s->hcap = cap;
  s->htab = (int*) calloc((size_t) cap, sizeof(int));
  for(i = 0; i < s->nt; i++){
    unsigned j = WHash(s->tr[i].f, s->tr[i].l, s->tr[i].t) & (unsigned)(cap - 1);
    while(s->htab[j]) j = (j + 1) & (unsigned)(cap - 1);
    s->htab[j] = i + 1;
  }
}

static int WAdd(WSat* s, int f, int l, int t){
  unsigned j = WHash(f, l, t) & (unsigned)(s->hcap - 1);
  while(s->htab[j]){
    WTr* x = &s->tr[s->htab[j] - 1];
    if(x->f == f && x->l == l && x->t == t) return 0;
    j = (j + 1) & (unsigned)(s->hcap - 1);
  }
  s->tr = (WTr*) WGrow(s->tr, &s->capT, s->nt + 1, sizeof(WTr));
  s->tr[s->nt].f = f; s->tr[s->nt].l = l; s->tr[s->nt].t = t;
  s->nt++;
  s->htab[j] = s->nt;
  if(2 * s->nt > s->hcap) WRehash(s, 2 * s->hcap);
  return 1;
}

static int PushNode(WSat* s, int c, int g){       /* Schwoon's state q_{c,g} */
  int key = c * s->na + g, i, c2;
  for(i = 0; i < s->npush; i++) if(s->pushKey[i] == key) return s->pushNode[i];
  c2 = s->capPush;
  s->pushKey = (int*) WGrow(s->pushKey, &c2, s->npush + 1, sizeof(int)); c2 = s->capPush;
  s->pushNode = (int*) WGrow(s->pushNode, &c2, s->npush + 1, sizeof(int));
  s->capPush = c2;
  s->pushKey[s->npush] = key;
  s->pushNode[s->npush] = NewNode(s, false);
  return s->pushNode[s->npush++];
}

/* ---------- the run ---------- */

/* builds the window controls, the letters and the initial pushdown automaton (controls, then one LOW
 * node per state of fa); for pre*, the target windows are closed under shorter suffixes (deep) */
void WBuild(WSat* s, Fa* fa, Upds* upds, int k, int post, State*** pfst, int* pnf){
  State** fst = NULL;
  int nf = 0, capF = 0, i, j, a, p0;
  ListTransition* l;
  ListRule* r;
  bool* seen;
  int* queue;
  int qh = 0, qt = 0;

  memset(s, 0, sizeof(WSat));
  if(k < 0) k = MaxReadOffset(upds);          /* WINDOW_AUTO: as large as the largest read offset */
  if(k > 60) k = 60;
  s->k = k;
  for(i = 0; i < upds->nalphabet; i++) WLetter(s, upds->alphabet[i]);
  for(r = upds->rules; r; r = r->next){
    WLetter(s, r->rule->from_stack);
    if(r->rule->to_stack1) WLetter(s, r->rule->to_stack1);
    if(r->rule->to_stack2) WLetter(s, r->rule->to_stack2);
  }
  for(r = upds->rules; r; r = r->next) if(r->rule->read_letter) WLetter(s, r->rule->read_letter);
  for(l = fa->listTransition; l; l = l->next) WLetter(s, l->transition->label);
  BuildWindows(s);
  s->hcap = 256;
  s->htab = (int*) calloc((size_t) s->hcap, sizeof(int));
  for(i = 0; i < s->C; i++) NewNode(s, false);

  /* states of the input automaton (LOW node of state f = C + f) */
  {
    State* all[2];
    fst = (State**) WGrow(fst, &capF, 1, sizeof(State*));
    fst[nf++] = fa->initial;
    for(l = fa->listTransition; l; l = l->next){
      all[0] = l->transition->from; all[1] = l->transition->to;
      for(a = 0; a < 2; a++){
        for(j = 0; j < nf; j++) if(IsTheSameState(fst[j], all[a])) break;
        if(j == nf){ fst = (State**) WGrow(fst, &capF, nf + 1, sizeof(State*)); fst[nf++] = all[a]; }
      }
    }
  }
  p0 = 0;
  for(i = 0; i < nf; i++) NewNode(s, fst[i]->final);

  /* initial configurations: read the upper part of fa while tracking the window */
  seen = (bool*) calloc((size_t) nf * (size_t) s->C, sizeof(bool));
  queue = (int*) malloc((size_t) nf * (size_t) s->C * sizeof(int));
  {
    int empty[1];
    int c0 = Ctrl(s, empty, 0, 0);
    seen[p0 * s->C + c0] = true;
    queue[qt++] = p0 * s->C + c0;
  }
  while(qh < qt){
    int q = queue[qh] / s->C, c = queue[qh] % s->C;
    int variants[64], nv = 0, v;
    qh++;
    /* pre*: a target upper stack u is also represented by (any suffix of its window, deep): those
     * abstract configurations can reach u too, so they belong to the target set */
    variants[nv++] = c;
    if(!post){
      int w[64], len = s->wlen[c], d;
      for(d = 0; d < len; d++) w[d] = s->wlet[c * s->k + d];
      for(d = 0; d <= len; d++) variants[nv++] = Ctrl(s, w + d, len - d, 1);
    }
    for(v = 0; v < nv; v++) if(fst[q]->final) s->final[variants[v]] = true;
    for(l = fa->listTransition; l; l = l->next){
      Transition* t = l->transition;
      if(!IsTheSameState(t->from, fst[q])) continue;
      for(j = 0; j < nf; j++) if(IsTheSameState(fst[j], t->to)) break;
      if(t->lower){ for(v = 0; v < nv; v++) WAdd(s, variants[v], WLetter(s, t->label), s->C + j); }
      else{
        int c2 = PopCtrl(s, c, WLetter(s, t->label)), key = j * s->C + c2;
        if(!seen[key]){ seen[key] = true; queue[qt++] = key; }
      }
    }
  }
  free(seen); free(queue);
  for(l = fa->listTransition; l; l = l->next){
    Transition* t = l->transition;
    int f, to;
    if(!t->lower) continue;
    for(f = 0; f < nf; f++) if(IsTheSameState(fst[f], t->from)) break;
    for(to = 0; to < nf; to++) if(IsTheSameState(fst[to], t->to)) break;
    WAdd(s, s->C + f, WLetter(s, t->label), s->C + to);
  }

  *pfst = fst;
  *pnf = nf;
}

static Fa* WRun(Fa* fa, Upds* upds, int k, int post){
  WSat S;
  WSat* s = &S;
  State** fst;
  int nf, i, j, a, p0 = 0, changed;
  ListRule* r;

  WBuild(s, fa, upds, k, post, &fst, &nf);
  /* saturation of the pushdown system whose control states are the windows */
  changed = 1;
  while(changed){
    int nt = s->nt;
    changed = 0;
    if(post){
      for(i = 0; i < nt; i++){
        WTr x = s->tr[i];
        if(x.f >= s->C) continue;
        if(x.l == EPS){                                  /* epsilon closure */
          for(j = 0; j < s->nt; j++){
            WTr y = s->tr[j];
            if(y.f == x.t && y.l != EPS) changed |= WAdd(s, x.f, y.l, y.t);
          }
          if(s->final[x.t] && !s->final[x.f]){ s->final[x.f] = true; changed = 1; }
          continue;
        }
        for(r = upds->rules; r; r = r->next){
          Rule* rule = r->rule;
          if(WLetter(s, rule->from_stack) != x.l || !ReadOk(s, x.f, rule)) continue;
          if(IsPopRule(rule))
            changed |= WAdd(s, PopCtrl(s, x.f, x.l), EPS, x.t);
          else if(isSwitchRule(rule))
            changed |= WAdd(s, x.f, WLetter(s, rule->to_stack1), x.t);
          else if(isPushRule(rule)){
            int c2 = EraseCtrl(s, x.f), g1 = WLetter(s, rule->to_stack1), g2 = WLetter(s, rule->to_stack2);
            int m = PushNode(s, c2, g1);
            changed |= WAdd(s, c2, g1, m);
            changed |= WAdd(s, m, g2, x.t);
          }
        }
      }
    }else{
      int c;
      for(c = 0; c < s->C; c++){
        for(r = upds->rules; r; r = r->next){
          Rule* rule = r->rule;
          int g = WLetter(s, rule->from_stack);
          if(!ReadOk(s, c, rule)) continue;
          if(IsPopRule(rule))
            changed |= WAdd(s, c, g, PopCtrl(s, c, g));
          else if(isSwitchRule(rule)){
            int g1 = WLetter(s, rule->to_stack1);
            for(i = 0; i < s->nt; i++)
              if(s->tr[i].f == c && s->tr[i].l == g1) changed |= WAdd(s, c, g, s->tr[i].t);
          }else if(isPushRule(rule)){
            int c2 = EraseCtrl(s, c), g1 = WLetter(s, rule->to_stack1), g2 = WLetter(s, rule->to_stack2);
            for(i = 0; i < s->nt; i++){
              if(s->tr[i].f != c2 || s->tr[i].l != g1) continue;
              for(j = 0; j < s->nt; j++)
                if(s->tr[j].f == s->tr[i].t && s->tr[j].l == g2) changed |= WAdd(s, c, g, s->tr[j].t);
            }
          }
        }
      }
    }
  }

  /* ---------- output: a P-automaton of the UPDS ---------- */
  {
    State** node = (State**) calloc((size_t) s->nn, sizeof(State*));
    State** twin = (State**) calloc((size_t) s->C, sizeof(State*));   /* lower copy of a control */
    bool* used = (bool*) calloc((size_t) s->C, sizeof(bool));
    bool* entered = (bool*) calloc((size_t) s->C, sizeof(bool));
    ListTransition *head = NULL, *tail = NULL;
    char buf[64];
    int c, n;

#define EMIT(F, T, LAB, LOW) do { ListTransition* nd_ = CreateListTransition(CreateTransition((F), (T), (LAB), (LOW))); \
      if(!head) head = nd_; else tail->next = nd_; tail = nd_; } while(0)

    for(i = 0; i < s->nt; i++){
      if(s->tr[i].f < s->C) used[s->tr[i].f] = true;
      if(s->tr[i].t < s->C) entered[s->tr[i].t] = true;
    }
    for(n = s->C; n < s->nn; n++){
      int f = n - s->C;
      if(f < nf && f != p0) node[n] = fst[f];
      else{
        sprintf(buf, f < nf ? "L%d" : "q%d", n);
        node[n] = CreateState(CreateStateName(strcpy((char*) malloc(strlen(buf) + 1), buf), ""), false, false, false);
        SetFinal(node[n], s->final[n]);
      }
    }
    for(c = 0; c < s->C; c++) if(entered[c]){
      sprintf(buf, "C%d", c);
      twin[c] = CreateState(CreateStateName(strcpy((char*) malloc(strlen(buf) + 1), buf), ""), false, false, false);
      SetFinal(twin[c], s->final[c]);
    }
#define OUT(T) ((T) < s->C ? twin[T] : node[T])

    /* lower part: transitions of the non-control nodes and of the control twins */
    for(i = 0; i < s->nt; i++){
      WTr x = s->tr[i];
      if(x.l == EPS) continue;
      if(x.f >= s->C) EMIT(node[x.f], OUT(x.t), s->let[x.l], true);
      else if(twin[x.f]) EMIT(twin[x.f], OUT(x.t), s->let[x.l], true);
    }
    /* upper part: for each window (w, deep), a path from p reading Gamma* (if deep) then w */
    for(c = 0; c < s->C; c++){
      State* entries[2];
      State* tops[2];
      int ne = 0, ntop, len = s->wlen[c];
      if(!used[c] && !s->final[c]) continue;
      entries[ne++] = fa->initial;
      if(c >= s->W){
        sprintf(buf, "D%d", c);
        entries[ne] = CreateState(CreateStateName(strcpy((char*) malloc(strlen(buf) + 1), buf), ""), false, false, false);
        for(a = 0; a < s->na; a++){
          EMIT(fa->initial, entries[ne], s->let[a], false);
          EMIT(entries[ne], entries[ne], s->let[a], false);
        }
        ne++;
      }
      if(len == 0){ ntop = ne; for(i = 0; i < ne; i++) tops[i] = entries[i]; }
      else{
        State* prev = NULL;
        for(i = 0; i < len; i++){
          State* cur;
          sprintf(buf, "W%d_%d", c, i + 1);
          cur = CreateState(CreateStateName(strcpy((char*) malloc(strlen(buf) + 1), buf), ""), false, false, false);
          if(i == 0) for(j = 0; j < ne; j++) EMIT(entries[j], cur, s->let[s->wlet[c * s->k]], false);
          else EMIT(prev, cur, s->let[s->wlet[c * s->k + i]], false);
          prev = cur;
        }
        ntop = 1; tops[0] = prev;
      }
      for(j = 0; j < ntop; j++){
        if(s->final[c]) SetFinal(tops[j], true);
        for(i = 0; i < s->nt; i++)
          if(s->tr[i].f == c && s->tr[i].l != EPS) EMIT(tops[j], OUT(s->tr[i].t), s->let[s->tr[i].l], true);
      }
    }
    free(node); free(twin); free(used); free(entered);
    free(fst); free(s->let); free(s->off); free(s->wlen); free(s->wlet); free(s->final);
    free(s->tr); free(s->htab); free(s->pushKey); free(s->pushNode);
    if(!head){ /* nothing accepted: keep a valid (empty) list */
      State* dead = CreateState(CreateStateName("dead", ""), false, false, false);
      head = CreateListTransition(CreateTransition(dead, dead, "", true));
    }
    return CreateFa(head, fa->initial);
  }
}

Fa* WindowPostStar(Fa* fa, Upds* upds, int k){ return WRun(fa, upds, k, 1); }
Fa* WindowPreStar(Fa* fa, Upds* upds, int k){ return WRun(fa, upds, k, 0); }
