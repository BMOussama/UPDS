/* Shared machinery of the LTL / CTL model checkers: see McInternal.h. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "McInternal.h"

static void* Grow(void* p, int* cap, int need, size_t size){
  if(need <= *cap) return p;
  while(*cap < need) *cap = *cap ? 2 * *cap : 16;
  return realloc(p, (size_t)(*cap) * size);
}

static char* Dup(const char* s, size_t n){
  char* c = (char*) malloc(n + 1);
  memcpy(c, s, n); c[n] = 0;
  return c;
}

/* ====================== hash map ====================== */

static unsigned MHash(int a, int b, int c){
  unsigned h = (unsigned) a * 2654435761u;
  h ^= (unsigned) b * 2246822519u + (h >> 13);
  h ^= (unsigned) c * 3266489917u + (h >> 11);
  h *= 668265263u;
  return h ^ (h >> 15);
}

void McMapInit(McMap* m){
  m->cap = 64; m->n = 0;
  m->k1 = (int*) malloc(64 * sizeof(int)); m->k2 = (int*) malloc(64 * sizeof(int));
  m->k3 = (int*) malloc(64 * sizeof(int)); m->val = (int*) malloc(64 * sizeof(int));
  memset(m->val, -1, 64 * sizeof(int));
}

void McMapFree(McMap* m){ free(m->k1); free(m->k2); free(m->k3); free(m->val); memset(m, 0, sizeof(McMap)); }

static int MSlot(McMap* m, int a, int b, int c){
  unsigned j = MHash(a, b, c) & (unsigned)(m->cap - 1);
  while(m->val[j] != -1 && (m->k1[j] != a || m->k2[j] != b || m->k3[j] != c)) j = (j + 1) & (unsigned)(m->cap - 1);
  return (int) j;
}

int McMapGet(McMap* m, int a, int b, int c){ return m->val[MSlot(m, a, b, c)]; }

void McMapPut(McMap* m, int a, int b, int c, int val){
  int j = MSlot(m, a, b, c);
  if(m->val[j] == -1){
    m->n++;
    m->k1[j] = a; m->k2[j] = b; m->k3[j] = c;
  }
  m->val[j] = val;
  if(2 * m->n > m->cap){
    McMap o = *m;
    int i;
    m->cap = 2 * o.cap; m->n = 0;
    m->k1 = (int*) malloc((size_t) m->cap * sizeof(int)); m->k2 = (int*) malloc((size_t) m->cap * sizeof(int));
    m->k3 = (int*) malloc((size_t) m->cap * sizeof(int)); m->val = (int*) malloc((size_t) m->cap * sizeof(int));
    memset(m->val, -1, (size_t) m->cap * sizeof(int));
    for(i = 0; i < o.cap; i++) if(o.val[i] != -1) McMapPut(m, o.k1[i], o.k2[i], o.k3[i], o.val[i]);
    McMapFree(&o);
  }
}

/* ====================== the window pushdown system ====================== */

int McFindLetter(McModel* m, const char* name){
  int i;
  for(i = 0; i < m->s.na; i++) if(!strcmp(m->s.let[i], name)) return i;
  return -1;
}

void McBuild(McModel* m, Fa* fa, Upds* upds, int k){
  WSat* s = &m->s;
  int c, g, cap = 0, n = 0;
  ListRule* r;
  WBuild(s, fa, upds, k, 1, &m->fst, &m->nf);
  m->C = s->C; m->G = s->na + 1; m->BOT = s->na;
  m->rstart = (int*) malloc(((size_t) m->C * (size_t) m->G + 1) * sizeof(int));
  m->rules = NULL;
  for(c = 0; c < m->C; c++) for(g = 0; g < m->G; g++){
    int sure = 0;
    m->rstart[c * m->G + g] = n;
    if(g != m->BOT) for(r = upds->rules; r; r = r->next){
      Rule* rule = r->rule;
      McRule x;
      if(WLetter(s, rule->from_stack) != g || !ReadOk(s, c, rule)) continue;
      if(rule->read_offset <= s->wlen[c]) sure = 1;     /* the read is inside the window (or no read) */
      x.g1 = x.g2 = -1;
      if(IsPopRule(rule)){ x.kind = MC_POP; x.c2 = PopCtrl(s, c, g); }
      else if(isSwitchRule(rule)){ x.kind = MC_SW; x.c2 = c; x.g1 = WLetter(s, rule->to_stack1); }
      else{ x.kind = MC_PUSH; x.c2 = EraseCtrl(s, c); x.g1 = WLetter(s, rule->to_stack1); x.g2 = WLetter(s, rule->to_stack2); }
      m->rules = (McRule*) Grow(m->rules, &cap, n + 1, sizeof(McRule));
      m->rules[n++] = x;
    }
    if(!sure){                                           /* the run may block here: it stutters */
      m->rules = (McRule*) Grow(m->rules, &cap, n + 1, sizeof(McRule));
      m->rules[n].kind = MC_SW; m->rules[n].c2 = c; m->rules[n].g1 = g; m->rules[n].g2 = -1;
      n++;
    }
  }
  m->rstart[m->C * m->G] = n;
}

void McFree(McModel* m){
  WSat* s = &m->s;
  free(m->fst); free(s->let); free(s->off); free(s->wlen); free(s->wlet); free(s->final);
  free(s->tr); free(s->htab); free(s->pushKey); free(s->pushNode);
  free(m->rstart); free(m->rules);
}

/* ====================== formulas ====================== */

typedef struct { const char* p; int ctl; int err; } Lex;
typedef struct { int kind; const char* s; int n; } Tok;   /* kind: 0 end, 'i' identifier, or the char, '>' for -> */

static int IdChar(int c){ return isalnum(c) || c == '_' || c == '.' || c == '$' || c == '@' || c == ':' || c == '#'; }

static Tok Peek(Lex* L){
  Tok t;
  const char* p = L->p;
  while(*p && isspace((unsigned char) *p)) p++;
  t.s = p; t.n = 1;
  if(!*p){ t.kind = 0; t.n = 0; return t; }
  if(p[0] == '-' && p[1] == '>'){ t.kind = '>'; t.n = 2; return t; }
  if((p[0] == '&' && p[1] == '&') || (p[0] == '|' && p[1] == '|')){ t.kind = p[0]; t.n = 2; return t; }
  if(strchr("()[]!&|~", *p)){ t.kind = *p == '~' ? '!' : *p; return t; }
  if(IdChar((unsigned char) *p)){
    t.kind = 'i'; t.n = 0;
    while(IdChar((unsigned char) p[t.n])) t.n++;
    return t;
  }
  t.kind = '?';
  return t;
}
static void Take(Lex* L, Tok t){ L->p = t.s + t.n; }
static int Is(Tok t, const char* w){ return t.kind == 'i' && (int) strlen(w) == t.n && !strncmp(t.s, w, (size_t) t.n); }

static McF* Node(int op, McF* a, McF* b){
  McF* f = (McF*) calloc(1, sizeof(McF));
  f->op = op; f->a = a; f->b = b;
  return f;
}

void McFreeF(McF* f){
  if(!f) return;
  McFreeF(f->a); McFreeF(f->b);
  free(f->atom.name);
  free(f);
}

static McF* Fail(Lex* L, const char* msg, Tok t){
  if(!L->err) fprintf(stderr, "formula: %s at \"%.20s\"\n", msg, t.kind ? t.s : "<end>");
  L->err = 1;
  return NULL;
}

static McF* PImp(Lex* L);

static McF* PAtom(Lex* L){
  Tok t = Peek(L);
  McF* f;
  if(t.kind == '('){
    Take(L, t);
    f = PImp(L);
    t = Peek(L);
    if(!f) return NULL;
    if(t.kind != ')'){ McFreeF(f); return Fail(L, "expected )", t); }
    Take(L, t);
    return f;
  }
  if(t.kind != 'i') return Fail(L, "expected an atom", t);
  Take(L, t);
  f = Node(F_ATOM, NULL, NULL);
  f->atom.letter = -1;
  if(Is(t, "true")) f->atom.kind = MA_TRUE;
  else if(Is(t, "false")) f->atom.kind = MA_FALSE;
  else if(Is(t, "at.end")) f->atom.kind = MA_END;
  else if(t.n > 3 && !strncmp(t.s, "at.", 3)){ f->atom.kind = MA_AT; f->atom.name = Dup(t.s + 3, (size_t)(t.n - 3)); }
  else if(t.n > 2 && !strncmp(t.s, "up", 2) && isdigit((unsigned char) t.s[2])){
    int i = 2, j = 0;
    while(i < t.n && isdigit((unsigned char) t.s[i])) j = 10 * j + (t.s[i++] - '0');
    if(i >= t.n - 1 || t.s[i] != '.' || j < 1 || j > 60){ McFreeF(f); return Fail(L, "bad atom upJ.x", t); }
    f->atom.kind = MA_UP; f->atom.j = j; f->atom.name = Dup(t.s + i + 1, (size_t)(t.n - i - 1));
  }else{ McFreeF(f); return Fail(L, "unknown atom (use at.X, at.end, upJ.x, true, false)", t); }
  return f;
}

/* identifiers made of unary operators: LTL "GF" = G F; CTL "AGEF" = AG EF */
static int UnaryOps(Lex* L, Tok t, int* ops){
  int n = 0, i;
  if(t.kind != 'i') return 0;
  if(!L->ctl){
    for(i = 0; i < t.n; i++){
      if(t.s[i] == 'X') ops[n++] = F_X; else if(t.s[i] == 'F') ops[n++] = F_F;
      else if(t.s[i] == 'G') ops[n++] = F_G; else return 0;
    }
    return n;
  }
  if(t.n % 2) return 0;
  for(i = 0; i < t.n; i += 2){
    char q = t.s[i], o = t.s[i + 1];
    if(q != 'E' && q != 'A') return 0;
    if(o == 'X') ops[n++] = q == 'E' ? F_EX : F_AX;
    else if(o == 'F') ops[n++] = q == 'E' ? F_EF : F_AF;
    else if(o == 'G') ops[n++] = q == 'E' ? F_EG : F_AG;
    else return 0;
  }
  return n;
}

static McF* PUnary(Lex* L){
  Tok t = Peek(L);
  int ops[64], n, i;
  McF* f;
  if(t.kind == '!'){ Take(L, t); f = PUnary(L); return f ? Node(F_NOT, f, NULL) : NULL; }
  if(t.n < 64 && (n = UnaryOps(L, t, ops)) > 0){
    Take(L, t);
    f = PUnary(L);
    if(!f) return NULL;
    for(i = n - 1; i >= 0; i--) f = Node(ops[i], f, NULL);
    return f;
  }
  if(L->ctl && (Is(t, "E") || Is(t, "A"))){       /* E[f U g], A[f U g] */
    int q = Is(t, "E");
    Tok o, u, c;
    McF *a, *b;
    Take(L, t);
    o = Peek(L);
    if(o.kind != '[' && o.kind != '(') return Fail(L, "expected [ after E / A", o);
    Take(L, o);
    a = PImp(L);
    if(!a) return NULL;
    u = Peek(L);
    if(!Is(u, "U")){ McFreeF(a); return Fail(L, "expected U", u); }
    Take(L, u);
    b = PImp(L);
    if(!b){ McFreeF(a); return NULL; }
    c = Peek(L);
    if(c.kind != (o.kind == '[' ? ']' : ')')){ McFreeF(a); McFreeF(b); return Fail(L, "expected ] or )", c); }
    Take(L, c);
    return Node(q ? F_EU : F_AU, a, b);
  }
  return PAtom(L);
}

static McF* PUntil(Lex* L){
  McF* a = PUnary(L);
  Tok t;
  if(!a || L->ctl) return a;
  t = Peek(L);
  if(Is(t, "U") || Is(t, "R")){
    McF* b;
    Take(L, t);
    b = PUntil(L);
    if(!b){ McFreeF(a); return NULL; }
    return Node(Is(t, "U") ? F_U : F_R, a, b);
  }
  return a;
}

static McF* PAnd(Lex* L){
  McF* a = PUntil(L);
  Tok t;
  while(a && (t = Peek(L)).kind == '&'){
    McF* b;
    Take(L, t);
    b = PUntil(L);
    if(!b){ McFreeF(a); return NULL; }
    a = Node(F_AND, a, b);
  }
  return a;
}

static McF* POr(Lex* L){
  McF* a = PAnd(L);
  Tok t;
  while(a && (t = Peek(L)).kind == '|'){
    McF* b;
    Take(L, t);
    b = PAnd(L);
    if(!b){ McFreeF(a); return NULL; }
    a = Node(F_OR, a, b);
  }
  return a;
}

static McF* PImp(Lex* L){
  McF* a = POr(L);
  Tok t;
  if(a && (t = Peek(L)).kind == '>'){
    McF* b;
    Take(L, t);
    b = PImp(L);
    if(!b){ McFreeF(a); return NULL; }
    a = Node(F_IMP, a, b);
  }
  return a;
}

McF* McParse(const char* text, int ctl){
  Lex L;
  McF* f;
  Tok t;
  L.p = text; L.ctl = ctl; L.err = 0;
  f = PImp(&L);
  if(!f) return NULL;
  t = Peek(&L);
  if(t.kind){ McFreeF(f); return Fail(&L, "unexpected text", t); }
  return f;
}

int McMaxOffset(McF* f){
  int a, b;
  if(!f) return 0;
  a = McMaxOffset(f->a); b = McMaxOffset(f->b);
  if(b > a) a = b;
  if(f->op == F_ATOM && f->atom.kind == MA_UP && f->atom.j > a) a = f->atom.j;
  return a;
}

void McResolve(McModel* m, McF* f){
  if(!f) return;
  McResolve(m, f->a); McResolve(m, f->b);
  if(f->op == F_ATOM && f->atom.name) f->atom.letter = McFindLetter(m, f->atom.name);
}

void McPrintAtom(McAtom* a){
  switch(a->kind){
    case MA_TRUE: printf("true"); break;
    case MA_FALSE: printf("false"); break;
    case MA_END: printf("at.end"); break;
    case MA_AT: printf("at.%s", a->name); break;
    default: printf("up%d.%s", a->j, a->name);
  }
}

int McAtomValue(McModel* m, McAtom* a, int c, int g){
  WSat* s = &m->s;
  int len;
  switch(a->kind){
    case MA_TRUE: return 1;
    case MA_FALSE: return 0;
    case MA_END: return g == m->BOT;
    case MA_AT: return a->letter >= 0 && g == a->letter;
    default:
      len = s->wlen[c];
      if(a->j <= len) return a->letter >= 0 && s->wlet[c * s->k + len - a->j] == a->letter;
      if(c < s->W || a->letter < 0) return 0;     /* no letter there, or a letter that is not in Gamma */
      return 2;
  }
}

/* ====================== saturation engine ====================== */

void McEngInit(McEngine* e, int N, int P, int A){
  memset(e, 0, sizeof(McEngine));
  e->N = N; e->P = P; e->A = A;
  McMapInit(&e->facts); McMapInit(&e->heads); McMapInit(&e->byTgt); McMapInit(&e->derived);
}

void McEngFree(McEngine* e){
  free(e->f); free(e->r); free(e->work);
  McMapFree(&e->facts); McMapFree(&e->heads); McMapFree(&e->byTgt); McMapFree(&e->derived);
}

void McEngFact(McEngine* e, int x, int a, int y, int flag){
  int i = McMapGet(&e->facts, x, a, y);
  if(i >= 0){
    if(e->f[i].flag >= flag) return;
    e->f[i].flag = flag;
  }else{
    i = e->nf;
    e->f = (McFact*) Grow(e->f, &e->capf, e->nf + 1, sizeof(McFact));
    e->f[i].x = x; e->f[i].a = a; e->f[i].y = y; e->f[i].flag = flag;
    e->f[i].next = McMapGet(&e->heads, x, a, 0);
    McMapPut(&e->heads, x, a, 0, i);
    McMapPut(&e->facts, x, a, y, i);
    e->nf++;
  }
  e->work = (int*) Grow(e->work, &e->capw, e->nw + 1, sizeof(int));
  e->work[e->nw++] = i;
}

int McEngFirst(McEngine* e, int x, int a){ return McMapGet(&e->heads, x, a, 0); }

static int NewRule(McEngine* e, int kind, int sx, int sa, int tx, int b1, int b2, int flag){
  int i = e->nr;
  McPRule* r;
  e->r = (McPRule*) Grow(e->r, &e->capr, e->nr + 1, sizeof(McPRule));
  r = &e->r[i];
  r->kind = kind; r->sx = sx; r->sa = sa; r->tx = tx; r->b1 = b1; r->b2 = b2; r->flag = flag;
  r->next = -1;
  if(kind != MC_POP){
    r->next = McMapGet(&e->byTgt, tx, b1, 0);
    McMapPut(&e->byTgt, tx, b1, 0, i);
  }
  e->nr++;
  return i;
}

void McEngRule(McEngine* e, int kind, int sx, int sa, int tx, int b1, int b2, int flag){
  NewRule(e, kind, sx, sa, tx, b1, b2, flag);
  if(kind == MC_POP) McEngFact(e, sx, sa, tx, flag);
}

/* <sx,sa> -> <y,b> obtained from a push rule and a pop summary: behaves as a switch rule */
static void Derived(McEngine* e, int sx, int sa, int y, int b, int flag){
  int key = sx * e->A + sa, i = McMapGet(&e->derived, key, y, b), j;
  if(i >= 0){
    if(e->r[i].flag >= flag) return;
    e->r[i].flag = flag;
  }else{
    i = NewRule(e, MC_SW, sx, sa, y, b, -1, flag);
    McMapPut(&e->derived, key, y, b, i);
  }
  for(j = McEngFirst(e, y, b); j >= 0; j = e->f[j].next) McEngFact(e, sx, sa, e->f[j].y, e->f[j].flag | flag);
}

void McEngRun(McEngine* e){
  while(e->nw > 0){
    int i = e->work[--e->nw], j;
    int x = e->f[i].x, a = e->f[i].a, y = e->f[i].y, fl = e->f[i].flag;
    for(j = McMapGet(&e->byTgt, x, a, 0); j >= 0; j = e->r[j].next){
      McPRule r = e->r[j];
      if(r.kind == MC_SW) McEngFact(e, r.sx, r.sa, y, fl | r.flag);
      else Derived(e, r.sx, r.sa, y, r.b2, fl | r.flag);
    }
  }
}

/* head graph: <p,g> -> <p',g'> when a run from <p,g> reaches <p',g' w> without popping g;
 * edges: switch rules, derived rules, and the first part of push rules. Repeating heads: in a strongly
 * connected component with an internal edge whose flag is set. Iterative Tarjan. */
bool* McEngRepeating(McEngine* e){
  int H = e->P * e->A, i, ne = 0, idx = 0, sp = 0, cs = 0;
  int *deg = (int*) calloc((size_t) H + 1, sizeof(int)), *adj, *aflag, *pos;
  int *index = (int*) malloc((size_t) H * sizeof(int)), *low = (int*) malloc((size_t) H * sizeof(int));
  int *comp = (int*) malloc((size_t) H * sizeof(int)), *stack = (int*) malloc((size_t) H * sizeof(int));
  int *cstack = (int*) malloc((size_t) H * sizeof(int)), *it = (int*) malloc((size_t) H * sizeof(int));
  bool *on = (bool*) calloc((size_t) H, sizeof(bool)), *good, *rep = (bool*) calloc((size_t) H, sizeof(bool));
  int ncomp = 0;
  for(i = 0; i < e->nr; i++) if(e->r[i].kind != MC_POP){ deg[e->r[i].sx * e->A + e->r[i].sa + 1]++; ne++; }
  for(i = 0; i < H; i++) deg[i + 1] += deg[i];
  adj = (int*) malloc((size_t)(ne + 1) * sizeof(int)); aflag = (int*) malloc((size_t)(ne + 1) * sizeof(int));
  pos = (int*) malloc((size_t) H * sizeof(int));
  for(i = 0; i < H; i++) pos[i] = deg[i];
  for(i = 0; i < e->nr; i++) if(e->r[i].kind != MC_POP){
    int u = e->r[i].sx * e->A + e->r[i].sa;
    adj[pos[u]] = e->r[i].tx * e->A + e->r[i].b1;
    aflag[pos[u]++] = e->r[i].flag;
  }
  for(i = 0; i < H; i++) index[i] = -1;
  for(i = 0; i < H; i++){
    if(index[i] >= 0 || deg[i] == deg[i + 1]) continue;
    cstack[cs++] = i; it[i] = deg[i]; index[i] = low[i] = idx++; stack[sp++] = i; on[i] = true;
    while(cs > 0){
      int u = cstack[cs - 1];
      if(it[u] < deg[u + 1]){
        int v = adj[it[u]++];
        if(index[v] < 0){
          index[v] = low[v] = idx++; stack[sp++] = v; on[v] = true; it[v] = deg[v];
          cstack[cs++] = v;
        }else if(on[v] && index[v] < low[u]) low[u] = index[v];
      }else{
        cs--;
        if(cs > 0 && low[u] < low[cstack[cs - 1]]) low[cstack[cs - 1]] = low[u];
        if(low[u] == index[u]){
          int w;
          do{ w = stack[--sp]; on[w] = false; comp[w] = ncomp; }while(w != u);
          ncomp++;
        }
      }
    }
  }
  good = (bool*) calloc((size_t) ncomp + 1, sizeof(bool));
  for(i = 0; i < H; i++){
    int j;
    if(index[i] < 0) continue;
    for(j = deg[i]; j < deg[i + 1]; j++) if(aflag[j] && index[adj[j]] >= 0 && comp[adj[j]] == comp[i]) good[comp[i]] = true;
  }
  for(i = 0; i < H; i++) rep[i] = index[i] >= 0 && good[comp[i]];
  free(deg); free(adj); free(aflag); free(pos); free(index); free(low); free(comp); free(stack);
  free(cstack); free(it); free(on); free(good);
  return rep;
}

/* ====================== automata ====================== */

Aut* AutNew(int n, int G){
  Aut* A = (Aut*) calloc(1, sizeof(Aut));
  A->n = n; A->G = G;
  A->fin = (bool*) calloc((size_t)(n > 0 ? n : 1), sizeof(bool));
  McMapInit(&A->dedup);
  return A;
}

int AutNode(Aut* A, bool fin){
  A->fin = (bool*) realloc(A->fin, (size_t)(A->n + 1) * sizeof(bool));
  A->fin[A->n] = fin;
  return A->n++;
}

void AutAdd(Aut* A, int u, int a, int v){
  if(McMapGet(&A->dedup, u, a, v) >= 0) return;
  McMapPut(&A->dedup, u, a, v, A->nt);
  A->tr = (McTr*) Grow(A->tr, &A->cap, A->nt + 1, sizeof(McTr));
  A->tr[A->nt].u = u; A->tr[A->nt].a = a; A->tr[A->nt].v = v;
  A->nt++;
}

void AutIndex(Aut* A){
  int i;
  free(A->first); free(A->nxt);
  A->first = (int*) malloc((size_t)(A->n > 0 ? A->n : 1) * sizeof(int));
  A->nxt = (int*) malloc((size_t)(A->nt > 0 ? A->nt : 1) * sizeof(int));
  for(i = 0; i < A->n; i++) A->first[i] = -1;
  for(i = A->nt - 1; i >= 0; i--){ A->nxt[i] = A->first[A->tr[i].u]; A->first[A->tr[i].u] = i; }
}

void AutFree(Aut* A){
  if(!A) return;
  free(A->fin); free(A->tr); free(A->first); free(A->nxt); McMapFree(&A->dedup); free(A);
}

Aut* AutFromModel(McModel* m){
  Aut* A = AutNew(m->s.nn, m->G);
  int i;
  for(i = 0; i < m->s.nn; i++) A->fin[i] = m->s.final[i];
  for(i = 0; i < m->s.nt; i++) if(m->s.tr[i].l != EPS) AutAdd(A, m->s.tr[i].f, m->s.tr[i].l, m->s.tr[i].t);
  AutIndex(A);
  return A;
}

Aut* AutFromEngine(McEngine* e, bool* fin){
  Aut* A = AutNew(e->N, e->A);
  int i;
  for(i = 0; i < e->N; i++) A->fin[i] = fin[i];
  for(i = 0; i < e->nf; i++) AutAdd(A, e->f[i].x, e->f[i].a, e->f[i].y);
  AutIndex(A);
  return A;
}

int McInitIntersect(Aut* init, Aut* A, int c, int start, int BOT){
  int n = init->n * A->n, qh = 0, qt = 0, found = 0;
  bool* seen = (bool*) calloc((size_t) n, sizeof(bool));
  int* queue = (int*) malloc((size_t) n * sizeof(int));
  seen[c * A->n + start] = true; queue[qt++] = c * A->n + start;
  while(qh < qt && !found){
    int u = queue[qh] / A->n, v = queue[qh] % A->n, i, j;
    qh++;
    if(init->fin[u])
      for(j = A->first[v]; j >= 0; j = A->nxt[j]) if(A->tr[j].a == BOT && A->fin[A->tr[j].v]){ found = 1; break; }
    for(i = init->first[u]; i >= 0 && !found; i = init->nxt[i])
      for(j = A->first[v]; j >= 0; j = A->nxt[j]) if(A->tr[j].a == init->tr[i].a){
        int key = init->tr[i].v * A->n + A->tr[j].v;
        if(!seen[key]){ seen[key] = true; queue[qt++] = key; }
      }
  }
  free(seen); free(queue);
  return found;
}
