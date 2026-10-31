"""post* saturation written to follow the report's rules (Chapter 5, "Computing post*") as literally as
possible, independently of the C code. Used to find out whether a wrong result comes from the rules or
from the C implementation.

Usage: python3 tests/report_post.py N_CASES SEED [--alg]     (random cases, compared with the oracle)

Notation: transitions (from, 'U'|'L', letter, to): 'U' = (letter, eps) upper, 'L' = (eps, letter) lower.
A rule applies to a lower transition t = (q', (eps,g), q) when q' is reachable from p by upper
transitions only (p -(w,eps)->* q'). The upper stack is empty when q' = p (p has no incoming transition).

Places where the text has to be interpreted (marked READING in the code):
 R1 (ii)(alpha) the report writes (p,(g',eps),q); a switch keeps the letter in the lower stack, so we use
    (p,(eps,g'),q) as in Algorithm 1.2, line 2.
 R2 (iii)(alpha) the report writes (p,(eps,g'),q) then (p,(eps,g''),q); we use Algorithm 1.3, lines 2-3:
    (p,(eps,g'),q_{p,g'}) and (q_{p,g'},(eps,g''),q).
 R3 (ii)(beta1) "pr is a duplicated state of q and a pop rule for g' exists": the table adds
    (pr,(g',eps),pr); Algorithm 1.2 line 7 adds (pr,(g,eps),pr) and (pr,(eps,g'),q). Default: the table;
    --alg uses Algorithm 1.2.
 R4 duplicated states: D_q for a state q of A or a push state q = q_{s,a} (then written D_{s,a}, member
    of P_Dpush). A duplicated state is not final.
"""
import sys, os, random
from collections import Counter
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fuzz

def post_star_report(T, final, rules, start='p', alg=False, cap=60):
    trans = set(T)
    kind = {}            # state -> 'push' (q_{s,a}), 'dup' (D_q of a state of A), 'duppush' (D_{s,a})
    pushinfo = {}        # push state -> (s, a)
    created = [0]
    origin = {}          # transition -> rule case that added it first
    post_star_report.origin = origin

    def push_state(s, a):                        # q_{s,a}
        name = 'q(%s,%s)' % (s, a)
        if name not in kind:
            kind[name] = 'push'; pushinfo[name] = (s, a); created[0] += 1
        return name

    def dup(q):                                  # D_q (D_{s,a} if q = q_{s,a})
        if q in pushinfo:
            name = 'D(%s,%s)' % pushinfo[q]
            if name not in kind: kind[name] = 'duppush'; pushinfo.setdefault(name, pushinfo[q]); created[0] += 1
        else:
            name = 'D' + q
            if name not in kind: kind[name] = 'dup'; created[0] += 1
        return name

    def is_dup(q): return kind.get(q) in ('dup', 'duppush')
    def is_duppush(q): return kind.get(q) == 'duppush'
    def is_dup_of(pr, q): return is_dup(pr) and pr == dup(q) if (q in pushinfo or q not in kind) else False
    def upper_preds(q): return [(pr, a) for (pr, k, a, t) in trans if k == 'U' and t == q]
    def has_selfloop(q): return any(f == q and t == q for (f, k, l, t) in trans)
    def pop_exists(g): return any(lhs == g and rhs == '' for lhs, rhs in rules)

    changed = True
    while changed:
        changed = False
        if created[0] > cap: return trans, False
        top = {start}; ch = True
        while ch:
            ch = False
            for (f, k, l, t) in trans:
                if k == 'U' and f in top and t not in top: top.add(t); ch = True
        new = set()
        cur = ['']
        def add(f, k, l, t):
            new.add((f, k, l, t))
            if (f, k, l, t) not in trans: origin.setdefault((f, k, l, t), cur[0])
        for (q1, k, g, q) in list(trans):        # t = (q', (eps,g), q)
            if k != 'L' or q1 not in top: continue
            loop = (q1 == q)
            empty = (q1 == start)
            for lhs, rhs in rules:
                if lhs != g: continue
                # ---------------- (i) pop <p,g> -> <p,eps> ----------------
                if rhs == '':
                    if empty: cur[0]='(i) pop alpha'; add(start, 'U', g, q)
                    elif loop:                                                       # (beta1)
                        for pr, a in upper_preds(q1):
                            if is_dup(pr) and pr == dup(q): cur[0]='(i) pop beta1, pr = D_q (self-loop)'; add(pr, 'U', g, pr)
                            else:
                                cur[0]="(i) pop beta1, via D_q'"; d = dup(q1); add(pr, 'U', a, d); add(d, 'U', g, q)
                    else:                                                            # (beta2)
                        if is_dup(q1): cur[0]="(i) pop beta2, q' duplicated"; add(q1, 'U', g, q)
                        else:
                            cur[0]="(i) pop beta2, via D_q'"; d = dup(q1)
                            for pr, a in upper_preds(q1): add(pr, 'U', a, d); add(d, 'U', g, q)
                # ---------------- (ii) switch <p,g> -> <p,g'> ----------------
                elif len(rhs) == 1:
                    g2 = rhs
                    if empty: cur[0]='(ii) switch alpha'; add(start, 'L', g2, q)                                 # (alpha), READING R1
                    elif loop:                                                       # (beta1)
                        for pr, a in upper_preds(q1):
                            if is_dup(pr) and pr == dup(q) and pop_exists(g2):
                                cur[0]='(ii) switch beta1, pr = D_q'
                                if alg: add(pr, 'U', g, pr); add(pr, 'L', g2, q)    # READING R3 (--alg)
                                else: add(pr, 'U', g2, pr)                           # READING R3 (table)
                            elif kind.get(q1) == 'push': cur[0]="(ii) switch beta1, q' pushed"; add(q1, 'L', g2, q1)
                            else:
                                cur[0]="(ii) switch beta1, via D_q'"; d = dup(q1); add(pr, 'U', a, d); add(d, 'L', g2, q)
                    else:                                                            # (beta2)
                        if is_dup(q1): cur[0]="(ii) switch beta2, q' duplicated"; add(q1, 'L', g2, q)
                        else:
                            cur[0]="(ii) switch beta2, via D_q'"; d = dup(q1)
                            for pr, a in upper_preds(q1): add(pr, 'U', a, d); add(d, 'L', g2, q)
                # ---------------- (iii) push <p,g> -> <p,g'g''> ----------------
                else:
                    g1, g2 = rhs[0], rhs[1]
                    if empty:                                                        # (alpha), READING R2
                        cur[0]='(iii) push alpha'; m = push_state(start, g1); add(start, 'L', g1, m); add(m, 'L', g2, q)
                    elif loop:                                                       # (beta1)
                        for pr, a in upper_preds(q1):
                            cur[0]='(iii) push beta1'
                            if is_duppush(pr):
                                m = push_state(pushinfo[pr][0], g1)
                            else:
                                m = push_state(pr, g1)
                            add(pr, 'L', g1, m); add(m, 'L', g2, q)
                    else:                                                            # (beta2)
                        if is_duppush(q1):                                           # (beta2-1)
                            m = push_state(pushinfo[q1][0], g1)
                            cur[0]="(iii) push beta2-1, first transition from q'"; add(q1, 'L', g1, m)
                            cur[0]='(iii) push beta2-1'
                            for pr, a in upper_preds(q1): add(pr, 'L', g1, m)
                            add(m, 'L', g2, q)
                        else:                                                        # (beta2-2)
                            for pr, a in upper_preds(q1):
                                if pr == q1: continue
                                m = push_state(pushinfo[pr][0], g1) if is_duppush(pr) else push_state(pr, g1)
                                cur[0]='(iii) push beta2-2'; add(pr, 'L', g1, m); add(m, 'L', g2, q)
                                if is_dup(q1) and has_selfloop(q1):
                                    cur[0]="(iii) push beta2-2, extra transition from q' (self-loop)"; add(q1, 'L', g1, m)
        if not new <= trans:
            trans |= new; changed = True
    return trans, True

def check(n, seed, alg=False, show=3):
    rng = random.Random(seed); stats = Counter(); shown = Counter()
    for i in range(n):
        T, final, ups, rules = fuzz.gen_case(rng)
        R, conv = post_star_report(T, final, rules, alg=alg)
        good = fuzz.oracle('post', T, final, rules)
        acc = {c for c in fuzz.CONFS if fuzz.accepts(list(R), final, *c)}
        extra, missing = acc - good, good - acc
        v = 'exact' if not (extra or missing) else ('extra+missing' if extra and missing else ('extra' if extra else 'missing'))
        if not conv: v += '+nofixpoint'
        stats[v] += 1
        if v != 'exact' and shown[v] < show:
            shown[v] += 1
            k = lambda c: (len(c[0]) + len(c[1]), c)
            print('%s: T=%s rules=%s extra=%s missing=%s' % (v, T, rules, sorted(extra, key=k)[:3], sorted(missing, key=k)[:3]))
    print(dict(stats))
    return stats

if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 50
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    check(n, seed, alg='--alg' in sys.argv)
