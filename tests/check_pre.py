"""Brute-force check of pre* for tests/pre_example.c.
Usage: ./pre_example N | python3 tests/check_pre.py N
Builds every configuration up to size MAX, applies the UPDS rules forward, and computes
exactly which ones can reach C. The automaton must accept exactly those (final state: f).
Configurations it finds are really in pre*; a run into C may pass through configurations bigger
than MAX, so an "extra" reported here should be re-checked with a larger MAX."""
import re, sys, itertools
ex = int(sys.argv[1]) if len(sys.argv) > 1 else 1
GAMMA = 'abx'
RULES = [('x', ['a']), ('a', []), ('b', []), ('a', ['a', 'b'])]
MAX = 7

def in_C(up, low):
    if ex == 1: return up == '' and low == 'abb'
    if ex == 2: return re.fullmatch(r'ab*', up) is not None and low == 'x'
    return up == 'b' and re.fullmatch(r'ax*', low) is not None

T = []
lines = sys.stdin.read().splitlines()
FINAL = {l.split()[1] for l in lines if l.startswith('FINAL ')}
for line in lines:
    m = re.match(r'^\((.*?),\((.*?),(.*?)\),(.*)\)$', line.strip())
    if not m: continue
    f, u, l, t = m.groups()
    T.append((f, '' if u == '.' else u, '' if l == '.' else l, t))

def accepts(up, low):
    cur = {'p'}
    for kind, c in [('u', c) for c in up] + [('l', c) for c in low]:
        cur = {t for (f, u, l, t) in T if f in cur and ((kind == 'u' and u == c and not l) or (kind == 'l' and l == c and not u))}
    return bool(cur & FINAL)

def succ(c):
    up, low = c
    if not low: return
    g, rest = low[0], low[1:]
    for lhs, rhs in RULES:
        if lhs != g: continue
        if not rhs: yield (up + g, rest)
        elif len(rhs) == 1: yield (up, rhs[0] + rest)
        elif up == '': yield ('', rhs[0] + rhs[1] + rest)
        else: yield (up[:-1], rhs[0] + rhs[1] + rest)

confs = [(''.join(w[:k]), ''.join(w[k:])) for n in range(MAX + 1)
         for w in itertools.product(GAMMA, repeat=n) for k in range(n + 1)]
pred = {}
for c in confs:
    for d in succ(c):
        pred.setdefault(d, []).append(c)
pre = {c for c in confs if in_C(*c)}
stack = list(pre)
while stack:
    for c in pred.get(stack.pop(), []):
        if c not in pre:
            pre.add(c); stack.append(c)
missing = sorted((c for c in pre if not accepts(*c)), key=lambda c: (len(c[0]) + len(c[1]), c))
extra = sorted((c for c in confs if c not in pre and accepts(*c)), key=lambda c: (len(c[0]) + len(c[1]), c))
print("example %d: pre* configurations (size <= %d): %d, not accepted: %d, accepted but not in pre*: %d"
      % (ex, MAX, len(pre), len(missing), len(extra)))
for c in missing[:8]: print("  missing <p, %s, %s>" % (c[0] or 'ε', c[1] or 'ε'))
for c in extra[:8]: print("  extra   <p, %s, %s>" % (c[0] or 'ε', c[1] or 'ε'))
sys.exit(1 if missing or extra else 0)
