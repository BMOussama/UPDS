"""Counterexample to the conjecture "post* of an APC set is regular" (single control state p).

UPDS rules:  c -> db,  d -> ca  (pushes),  c -> eps, a -> eps, b -> eps  (pops),  a -> ab  (push)
Initial set: C = { <p, eps, c x> }   (a single configuration, hence an APC set; x is never popped)

While the upper stack is empty, the pushes c -> db, d -> ca build the lower stack c (ab)^n x.
After popping c, the report's own non-regularity example applies (push a -> ab with pops of a and b).

This script lists every reachable configuration up to a given size (exact: no step makes a configuration
smaller) and prints the pairs (i, j) such that <p, c a^i b^j, x> is reachable. Up to size 16 they are
exactly { (i, j) : j >= i, j - i even } (plus (0,0)), which is not the set of a regular language:
post*(C) intersected with the regular set <p, c a* b*, x> would then not be regular.
(A pen-and-paper proof of j >= i for all sizes is still to be written.)

Usage: python3 tests/counterexample.py [MAX_SIZE]   (default 14)"""
import sys, os, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fuzz

rules = [('c', 'db'), ('d', 'ca'), ('c', ''), ('a', ''), ('b', ''), ('a', 'ab')]
MAX = int(sys.argv[1]) if len(sys.argv) > 1 else 14
start = ('', 'cx')
res = {start}; st = [start]
while st:
    for d in fuzz.succ(rules, st.pop()):
        if len(d[0]) + len(d[1]) <= MAX and d not in res: res.add(d); st.append(d)
pairs = sorted({(len(m.group(1)), len(m.group(2))) for u, l in res if l == 'x'
                for m in [re.fullmatch(r'c(a*)(b*)', u)] if m})
print('reachable <p, c a^i b^j, x>, (i, j) =', pairs)
bad = [(i, j) for i, j in pairs if (i, j) != (0, 0) and not (j >= i and (j - i) % 2 == 0)]
print('pairs outside { j >= i, j - i even }:', bad)
