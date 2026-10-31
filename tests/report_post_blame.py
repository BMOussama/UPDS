"""For random cases where tests/report_post.py is wrong: which rule case added the transition that made a
wrong configuration accepted, or which kind of step is missing; then shrinks one example per cause.
Usage: python3 tests/report_post_blame.py"""
import sys, os, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fuzz, report_post as RP
from collections import Counter
k = lambda c: (len(c[0]) + len(c[1]), c)

def run(T, final, rules):
    R, conv = RP.post_star_report(T, final, rules)
    origin = RP.post_star_report.origin
    order = {tr: i for i, tr in enumerate(origin)}
    good = fuzz.oracle('post', T, final, rules)
    acc = {c for c in fuzz.CONFS if fuzz.accepts(list(R), final, *c)}
    return R, origin, order, good, acc

def blame_extra(R, origin, order, c, final):
    ps = [('p', [])]
    for kind, ch in [('U', x) for x in c[0]] + [('L', x) for x in c[1]]:
        ps = [(tr[3], pa + [tr]) for (s, pa) in ps for tr in R if tr[0] == s and tr[1] == kind and tr[2] == ch]
    out = set()
    for s, pa in ps:
        if s in final:
            added = [tr for tr in pa if tr in order]
            if added: out.add(origin[max(added, key=lambda tr: order[tr])])
    return out

def blame_missing(rules, good, acc, c, R):
    for d in sorted(good & acc, key=k):
        for lhs, rhs in rules:
            if d[1] and d[1][0] == lhs and c in set(fuzz.succ([(lhs, rhs)], d)):
                kind = 'pop (i)' if rhs == '' else ('switch (ii)' if len(rhs) == 1 else 'push (iii)')
                return '%s, upper stack %s' % (kind, 'empty' if not d[0] else 'not empty')
    return 'no accepted predecessor'

def bad(T, final, rules, what):
    if not any(f == 'p' for (f, kk, l, t) in T): return False
    try: R, origin, order, good, acc = run(T, final, rules)
    except Exception: return False
    if what[0] == 'extra':
        ex = acc - good
        return bool(ex) and what[1] in blame_extra(R, origin, order, min(ex, key=k), final)
    ms = good - acc
    return bool(ms) and blame_missing(rules, good, acc, min(ms, key=k), R) == what[1]

def shrink(T, final, rules, what):
    ch = True
    while ch:
        ch = False
        for i in range(len(T)):
            T2 = T[:i] + T[i + 1:]
            if bad(T2, final, rules, what): T = T2; ch = True; break
        else:
            for i in range(len(rules)):
                r2 = rules[:i] + rules[i + 1:]
                if bad(T, final, r2, what): rules = r2; ch = True; break
    return T, rules

stats = Counter(); reps = {}
for seed in (1, 2, 3, 4):
    rng = random.Random(seed)
    for i in range(70):
        T, final, ups, rules = fuzz.gen_case(rng)
        R, origin, order, good, acc = run(T, final, rules)
        ex, ms = acc - good, good - acc
        if ex:
            for b in blame_extra(R, origin, order, min(ex, key=k), final):
                stats[('extra', b)] += 1; reps.setdefault(('extra', b), (T, final, rules))
        if ms:
            b = blame_missing(rules, good, acc, min(ms, key=k), R)
            stats[('missing', b)] += 1; reps.setdefault(('missing', b), (T, final, rules))
for key, v in stats.most_common(): print('%3d %s' % (v, key))
print()
for key, (T, final, rules) in reps.items():
    T2, r2 = shrink(list(T), final, list(rules), key)
    R, origin, order, good, acc = run(T2, final, r2)
    ex = sorted(acc - good, key=k); ms = sorted(good - acc, key=k)
    print(key); print('   C automaton:', T2, 'final', sorted(final)); print('   rules:', r2)
    print('   reachable:', sorted(good, key=k)[:8]); print('   extra:', ex[:4], ' missing:', ms[:4])
    if ex:
        c = ex[0]; ps = [('p', [])]
        for kind, ch in [('U', x) for x in c[0]] + [('L', x) for x in c[1]]:
            ps = [(tr[3], pa + [tr]) for (s, pa) in ps for tr in R if tr[0] == s and tr[1] == kind and tr[2] == ch]
        for s, pa in ps:
            if s in final: print('   path:', [(tr, origin.get(tr, 'initial')) for tr in pa]); break
