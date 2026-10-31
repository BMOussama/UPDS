"""Reference (Python) version of a corrected post* saturation, checked against the fuzz oracle.
Usage: python3 tests/post_reference.py N_CASES SEED

Differences with the rules of the report:
 * states have two roles: "top" states, where the upper stack can end (reachable from p by upper
   transitions only), and "lower" states. A lower transition never enters a top state (it enters the
   state's lower twin q' instead), so a rule applied at a top state can never be used in the middle of
   the lower stack.
 * (i)   pop    at a top state q' with (q',(eps,g),q): add (q',(g,eps),T(q)), where T(q) is a top copy of q
         (same lower transitions as q, kept in sync). This plays the role of the duplicated states D_q.
 * (ii)  switch at a top state q' with (q',(eps,g),q): add (q',(eps,g'),q).
 * (iii) push   at a top state q' with (q',(eps,g),q): the push overwrites the last upper letter, so the
         new lower transitions start at every upper predecessor r of q' ((r,(x,eps),q')), not at q':
         add (r,(eps,g'),M(r,g')) and (M(r,g'),(eps,g''),q); if q' = p (empty upper stack) also from p.
Termination is not guaranteed: the saturation stops after `cap` new states."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fuzz, random
from collections import Counter

def post_star(T, final, rules, start="p", cap=60):
    # T: list of (from, 'U'|'L', label, to)
    trans = set(T)
    finals = set(final)
    def upper_reach():
        top = {start}; ch = True
        while ch:
            ch = False
            for (f, k, l, t) in list(trans):
                if k == 'U' and f in top and t not in top: top.add(t); ch = True
        return top
    top = upper_reach()
    # split: lower transitions into a top state go to its lower twin
    twin = {}
    def get_twin(m):
        if m not in twin:
            twin[m] = m + "'"
            if m in finals: finals.add(m + "'")
        return twin[m]
    for tr in list(trans):
        f, k, l, t = tr
        if k == 'L' and t in top:
            trans.discard(tr); trans.add((f, k, l, get_twin(t)))
    topcopy = {}   # lower-role state q -> top copy T(q)
    def get_top(q):
        if q not in topcopy:
            topcopy[q] = 'T(%s)' % q
            if q in finals: finals.add(topcopy[q])
        return topcopy[q]
    mid = {}
    def get_mid(r, g):
        if (r, g) not in mid: mid[(r, g)] = 'M(%s,%s)' % (r, g)
        return mid[(r, g)]
    changed = True
    while changed:
        changed = False
        if len(mid) + len(topcopy) > cap: return trans, finals, False
        top = upper_reach()
        new = set()
        for (f, k, l, t) in trans:
            if k != 'L': continue
            # twins and top copies follow the lower transitions of their state
            if f in twin: new.add((twin[f], 'L', l, t))
            if f in topcopy: new.add((topcopy[f], 'L', l, t))
            if f not in top: continue
            q1, g, q = f, l, t
            for lhs, rhs in rules:
                if lhs != g: continue
                if rhs == '':                      # pop: <u, g l> -> <u g, l>
                    new.add((q1, 'U', g, get_top(q)))
                elif len(rhs) == 1:                # switch
                    new.add((q1, 'L', rhs, q))
                else:                              # push <u x, g l> -> <u, g'g'' l>, <eps, g l> -> <eps, g'g'' l>
                    srcs = [r for (r, k2, x, t2) in trans if k2 == 'U' and t2 == q1]
                    if q1 == start: srcs.append(start)
                    for r in srcs:
                        m = get_mid(r, rhs[0])
                        new.add((r, 'L', rhs[0], m)); new.add((m, 'L', rhs[1], q))
        # keep the discipline: no lower transition into a top state
        top2 = top | {t for (f, k, l, t) in new if k == 'U'}
        fixed = set()
        for (f, k, l, t) in new:
            if k == 'L' and t in top2: t = get_twin(t)
            fixed.add((f, k, l, t))
        if not fixed <= trans:
            trans |= fixed; changed = True
    return trans, finals, True

def check(n, seed):
    rng = random.Random(seed); stats = Counter(); ex = {}
    for i in range(n):
        T, final, ups, rules = fuzz.gen_case(rng)
        R, fin, conv = post_star(T, final, rules)
        good = fuzz.oracle('post', T, final, rules)
        acc = {c for c in fuzz.CONFS if fuzz.accepts(list(R), fin, *c)}
        v = 'exact' if acc == good else ('extra' if acc - good else 'missing')
        if not conv: v += '+nofixpoint'
        stats[v] += 1
        if v != 'exact' and v not in ex: ex[v] = (T, rules, sorted(acc - good)[:4], sorted(good - acc)[:4])
    print(stats)
    for v, e in ex.items(): print(v, e)

if __name__ == '__main__':
    check(int(sys.argv[1]) if len(sys.argv) > 1 else 20, int(sys.argv[2]) if len(sys.argv) > 2 else 1)
