"""Effects of the lower stack on the upper stack (research step for an exact, always-terminating post*).

When a letter g on top of the lower stack is completely consumed (g and everything produced from it is
popped), the upper stack changes by "erase k letters, then write v": a pop of c writes c, a push (with a
non-empty upper stack) erases one letter, and erasing a letter that was just written cancels it.
E(g) = set of such effects (k, v). They are the least solution of
    E(g) = {(0,g)}            if <p,g> -> <p,eps>
         u E(g')              if <p,g> -> <p,g'>
         u (1,eps).E(g').E(g'')  if <p,g> -> <p,g'g''>
with the composition (k1,v1).(k2,v2) = (k1, v1[:-k2] v2) if k2 <= |v1|, else (k1+k2-|v1|, v2).

Usage: python3 tests/effects.py "a>ab b> a>"      (rules: lhs>rhs, empty rhs = pop)
Prints E(g) for every letter, up to a bound on k and |v|."""
import sys

def compose(e1, e2):
    (k1, v1), (k2, v2) = e1, e2
    if k2 <= len(v1): return (k1, v1[:len(v1) - k2] + v2)
    return (k1 + k2 - len(v1), v2)

def effects(rules, bound=6):
    letters = sorted({c for lhs, rhs in rules for c in lhs + rhs})
    E = {g: set() for g in letters}
    ok = lambda e: e[0] <= bound and len(e[1]) <= bound
    changed = True
    while changed:
        changed = False
        for lhs, rhs in rules:
            new = set()
            if rhs == '': new.add((0, lhs))
            elif len(rhs) == 1: new |= E[rhs]
            else:
                for e1 in E[rhs[0]]:
                    for e2 in E[rhs[1]]:
                        new.add(compose(compose((1, ''), e1), e2))
            new = {e for e in new if ok(e)} - E[lhs]
            if new: E[lhs] |= new; changed = True
    return E

if __name__ == '__main__':
    rules = [tuple(r.split('>')) for r in (sys.argv[1] if len(sys.argv) > 1 else 'a>ab b> a>').split()]
    for g, es in effects(rules).items():
        print('E(%s) = %s' % (g, sorted(es, key=lambda e: (e[0], len(e[1]), e[1]))[:16]))
