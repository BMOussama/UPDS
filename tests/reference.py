"""Reference implementation (Python) of the corrected post* / pre* saturation for UPDS.

Configuration <p, u, l>: u upper stack, l lower stack. One control state p.
Automaton: transitions (from, 'U'|'L', letter, to); an accepted path reads u with 'U' transitions, then l with
'L' transitions, from p to a final state.

Corrected saturation (see README, "Corrected algorithm"):
  * Two roles of states. TOP states are reachable from p by U transitions only (the stack pointer can be
    there). A rule is applied only at a top state, and an L transition never enters a top state: it enters
    the state's lower twin W(m) instead (same L transitions as m, kept in sync, final iff m is final).
    So a transition added "at the top of the lower stack" can never be used in its middle.
  post*:
  (i)   pop    <p,g> -> <p,eps>  : top q' -(eps,g)-> q        ==> (q',(g,eps),T(q)),  T(q) = top copy of q
  (ii)  switch <p,g> -> <p,g'>   : top q' -(eps,g)-> q        ==> (q',(eps,g'),q)
  (iii) push   <p,g> -> <p,g'g''>: top q' -(eps,g)-> q, and r -(x,eps)-> q' (the letter x is overwritten),
                                   or r = q' = p (empty upper stack)
                                   ==> (r,(eps,g'),M(r,g')) and (M(r,g'),(eps,g''),q)
  pre*:
  (i)   pop    : top q' -(g,eps)-> q''                         ==> (q',(eps,g),q'')
  (ii)  switch : top q' -(eps,g')-> q                          ==> (q',(eps,g),q)
  (iii) push   : top q' -(eps,g')-> q1 -(eps,g'')-> q          ==> (q',(x,eps),N(q,g)) for every letter x,
                                   (N(q,g),(eps,g),q); and (p,(eps,g),q) if q' = p
  Every added transition only adds configurations that are successors (predecessors) of accepted ones, and at
  the fixpoint the language is closed under one step: the result is EXACT whenever the saturation stops.

Termination: new states can be created forever. When `cap` new states have been created, WIDENING starts: every new state of a given kind (twin, top copy, push
state) for a given letter is one shared summary state. There are then finitely many states, so the
saturation always stops. The result is an OVER-approximation closed under one step (an inductive invariant):
sound to prove that bad configurations are NOT reachable.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

class Saturation:
    def __init__(self, T, final, rules, start='p', alphabet=''):
        self.trans = set(T); self.final = set(final); self.rules = rules; self.start = start
        self.alpha = sorted(set(alphabet) | {l for (f, k, l, t) in T} | {c for lhs, rhs in rules for c in lhs + rhs})
        self.n = 0; self.twin = {}; self.copy = {}; self.mid = {}; self.info = {}
        self.widen = False      # widening: new states are shared summary states (finitely many)

    def fresh(self, kind, key, g=None, final_like=None):
        self.n += 1
        s = '%s%d' % (kind, self.n)
        self.info[s] = (kind, g, key)
        if final_like is not None and final_like in self.final: self.final.add(s)
        return s

    def top(self):
        top = {self.start}; ch = True
        while ch:
            ch = False
            for (f, k, l, t) in self.trans:
                if k == 'U' and f in top and t not in top: top.add(t); ch = True
        return top

    def summary(self, kind, g, like):
        key = ('*' + kind, g)
        if key not in self.mid: self.mid[key] = self.fresh('S' + kind, None, g)
        s = self.mid[key]
        if like in self.final: self.final.add(s)
        return s

    def get_twin(self, m):
        if m not in self.twin:
            self.twin[m] = self.summary('W', None, m) if self.widen else self.fresh('W', m, final_like=m)
        return self.twin[m]

    def get_copy(self, q):
        if q not in self.copy:
            self.copy[q] = self.summary('T', None, q) if self.widen else self.fresh('T', q, final_like=q)
        return self.copy[q]

    def get_mid(self, r, g, tag='M'):
        if (r, g) not in self.mid:
            self.mid[(r, g)] = self.summary(tag, g, None) if self.widen else self.fresh(tag, r, g)
        return self.mid[(r, g)]

    def normalize(self):
        top = self.top()
        for tr in list(self.trans):
            f, k, l, t = tr
            if k == 'L' and t in top:
                self.trans.discard(tr); self.trans.add((f, k, l, self.get_twin(t)))

    def rules_post(self, top, new):
        for (f, k, l, t) in self.trans:
            if k != 'L' or f not in top: continue
            for lhs, rhs in self.rules:
                if lhs != l: continue
                if rhs == '': new.add((f, 'U', l, self.get_copy(t)))
                elif len(rhs) == 1: new.add((f, 'L', rhs, t))
                else:
                    srcs = [r for (r, k2, x, t2) in self.trans if k2 == 'U' and t2 == f]
                    if f == self.start: srcs.append(self.start)
                    for r in srcs:
                        m = self.get_mid(r, rhs[0]); new.add((r, 'L', rhs[0], m)); new.add((m, 'L', rhs[1], t))

    def rules_pre(self, top, new):
        for (f, k, l, t) in self.trans:
            if f not in top: continue
            for lhs, rhs in self.rules:
                if rhs == '':
                    if k == 'U' and l == lhs: new.add((f, 'L', lhs, t))
                elif len(rhs) == 1:
                    if k == 'L' and l == rhs: new.add((f, 'L', lhs, t))
                elif k == 'L' and l == rhs[0]:
                    for (f2, k2, l2, t2) in self.trans:
                        if f2 == t and k2 == 'L' and l2 == rhs[1]:
                            nq = self.get_mid(t2, lhs, 'N')
                            for x in self.alpha: new.add((f, 'U', x, nq))
                            new.add((nq, 'L', lhs, t2))
                            if f == self.start: new.add((self.start, 'L', lhs, t2))

    def run(self, mode, cap=40, widen_after_cap=True):
        """returns 'exact', 'over' (widened, over-approximation) or 'under' (stopped, under-approximation)"""
        self.normalize()
        widen = False
        while True:
            if self.n > cap and not widen:
                if not widen_after_cap: return 'under'
                widen = self.widen = True
            top = self.top(); new = set()
            (self.rules_post if mode == 'post' else self.rules_pre)(top, new)
            top2 = top | {t for (f, k, l, t) in new if k == 'U'}
            fixed = {(f, k, l, self.get_twin(t) if (k == 'L' and t in top2) else t) for (f, k, l, t) in new}
            # twins and top copies follow the L transitions of their state
            for (f, k, l, t) in self.trans | fixed:
                if k == 'L':
                    for d in (self.twin, self.copy):
                        if f in d: fixed.add((d[f], 'L', l, self.get_twin(t) if t in top2 else t))
            if fixed <= self.trans:
                return 'over' if widen else 'exact'
            self.trans |= fixed

def post_star(T, final, rules, cap=40, widen=True, alphabet='abx'):
    s = Saturation(T, final, rules, alphabet=alphabet); status = s.run('post', cap, widen); return s.trans, s.final, status

def pre_star(T, final, rules, cap=40, widen=True, alphabet='abx'):
    s = Saturation(T, final, rules, alphabet=alphabet); status = s.run('pre', cap, widen); return s.trans, s.final, status

if __name__ == '__main__':
    import random, fuzz
    from collections import Counter
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 50
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    modes = sys.argv[3:] or ['post', 'pre']
    k = lambda c: (len(c[0]) + len(c[1]), c)
    for mode in modes:
        rng = random.Random(seed); stats = Counter(); shown = 0
        for i in range(n):
            T, final, ups, rules = fuzz.gen_case(rng)
            R, F, status = (post_star if mode == 'post' else pre_star)(T, final, rules)
            good = fuzz.oracle(mode, T, final, rules)
            acc = {c for c in fuzz.CONFS if fuzz.accepts(list(R), F, *c)}
            extra, missing = acc - good, good - acc
            if mode == 'pre':
                extra = {c for c in extra if not fuzz.reaches(rules, c, T, final, fuzz.MAX + 5)}
            v = 'correct' if not (extra or missing) else ('extra' if not missing else ('missing' if not extra else 'both'))
            stats[(status, v)] += 1
            if (status == 'exact' and v != 'correct') or (status == 'over' and missing) or (status == 'under' and extra):
                if shown < 3:
                    shown += 1; print('UNEXPECTED', mode, status, v, T, rules, sorted(extra, key=k)[:3], sorted(missing, key=k)[:3])
        print(mode, dict(stats))
