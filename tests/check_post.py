"""Brute-force check of post* for the report's example.
Usage: ./paper_example | python3 tests/check_post.py
Enumerates every configuration reachable from c0=<p,eps,x x*> (up to size MAX) using the
UPDS semantics, and checks the computed automaton accepts exactly those (final state: q)."""
import re,sys
# parse transitions (from,(u,l),to)
T=[]
lines = sys.stdin.read().splitlines()
FINAL = {l.split()[1] for l in lines if l.startswith('FINAL ')}
for line in lines:
    if not line.startswith('('): continue
    m=re.match(r'^\((.*?),\((.*?),(.*?)\),(.*)\)$',line.strip())
    f,u,l,t=m.groups(); T.append((f,u if u!='.' else '',l if l!='.' else '',t))
def accepts(up,low):
    word=[('u',c) for c in up]+[('l',c) for c in low]
    cur={'p'}
    for kind,c in word:
        nxt=set()
        for (f,u,l,t) in T:
            if f in cur and ((kind=='u' and u==c and l=='') or (kind=='l' and l==c and u=='')): nxt.add(t)
        cur=nxt
    return bool(cur & FINAL)
rules=[('x',['a']),('a',[]),('b',[]),('a',['a','b'])]
def succ(c):
    up,low=c
    if not low: return
    g,rest=low[0],low[1:]
    for lhs,rhs in rules:
        if lhs!=g: continue
        if len(rhs)==0: yield (up+g,rest)
        elif len(rhs)==1: yield (up,rhs[0]+rest)
        else:
            if up=='': yield ('',rhs[0]+rhs[1]+rest)
            else: yield (up[:-1],rhs[0]+rhs[1]+rest)
MAX=7
reach=set(); frontier=[('', 'x'*n) for n in range(1,MAX+1)]
reach|=set(frontier)
while frontier:
    c=frontier.pop()
    for d in succ(c):
        if len(d[0])+len(d[1])<=MAX and d not in reach:
            reach.add(d); frontier.append(d)
missing=sorted([c for c in reach if not accepts(*c)],key=lambda c:(len(c[0])+len(c[1]),c))
print("reachable:",len(reach),"not accepted:",len(missing))
for c in missing[:12]: print("  <p, %s, %s>"%(c[0] or 'ε', c[1] or 'ε'))
# soundness: accepted but unreachable (small words)
import itertools
extra=[]
for n in range(0,7):
  for w in itertools.product('abx',repeat=n):
    for k in range(n+1):
      up,low=''.join(w[:k]),''.join(w[k:])
      if accepts(up,low) and (up,low) not in reach and True: extra.append((up,low))
print("accepted but not reachable (bounded):",len(extra))
for c in extra[:8]: print("  <p, %s, %s>"%(c[0] or 'ε', c[1] or 'ε'))

sys.exit(1 if missing or extra else 0)
