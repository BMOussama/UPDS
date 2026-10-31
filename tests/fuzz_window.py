"""Random testing of the bounded-window model (WindowPostStar / WindowPreStar), with read rules.
Usage: python3 tests/fuzz_window.py BINARY N_CASES SEED K [post|pre|both]
K = window size, or "auto" (largest read offset). About a third of the rules get a read condition
"only if the j-th upper letter is x" (j = 1..3).

Two checks per case, both computed here independently of the library:
 1. EXACT for the window model: the abstract semantics (window of K letters + "deep" flag) is explored by
    brute force, and the automaton must accept a concrete configuration <u, l> iff some reachable abstract
    configuration (w, deep, l) represents it (u = w, or u ends with w when deep).
 2. SOUND for the full UPDS: every configuration that is really reachable (fuzz.oracle) is accepted."""
import itertools, os, random, subprocess, sys
from collections import Counter
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fuzz

ENV = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
LB = 8                      # bound on the lower stack in the abstract exploration

def push_w(w, d, g, k):
    if k == 0: return '', True
    return ((w + g), d) if len(w) < k else ((w + g)[1:], True)

def read_ok_abstract(w, d, j, x):
    if j == 0: return True
    if j <= len(w): return w[-j] == x
    return d

def read_ok(u, j, x): return j == 0 or (len(u) >= j and u[-j] == x)

def csucc(rules, c):
    """full UPDS semantics with read conditions"""
    up, low = c
    if not low: return
    g, rest = low[0], low[1:]
    for lhs, rhs, j, x in rules:
        if lhs != g or not read_ok(up, j, x): continue
        if not rhs: yield (up + g, rest)
        elif len(rhs) == 1: yield (up, rhs + rest)
        elif up == '': yield ('', rhs + rest)
        else: yield (up[:-1], rhs + rest)

def coracle(mode, T, final, rules):
    init = {c for c in fuzz.CONFS if fuzz.accepts(T, final, *c)}
    if mode == 'post':
        res, st = set(init), list(init)
        while st:
            for d in csucc(rules, st.pop()):
                if len(d[0]) + len(d[1]) <= fuzz.MAX and d not in res: res.add(d); st.append(d)
        return res
    pred = {}
    for c in fuzz.CONFS:
        for d in csucc(rules, c): pred.setdefault(d, []).append(c)
    res, st = set(init), list(init)
    while st:
        for c in pred.get(st.pop(), []):
            if c not in res: res.add(c); st.append(c)
    return res

def asucc(rules, c, k):
    w, d, l = c
    if not l: return
    g, rest = l[0], l[1:]
    for lhs, rhs, j, x in rules:
        if lhs != g or not read_ok_abstract(w, d, j, x): continue
        if rhs == '': nw, nd = push_w(w, d, g, k); yield (nw, nd, rest)
        elif len(rhs) == 1: yield (w, d, rhs + rest)
        else: yield (w[:-1] if w else '', d, rhs + rest)

def abstract_init(T, final, k, mode='post'):
    """abstract configurations (w, d, l) of the initial automaton, lower stacks up to LB letters"""
    start = ('p', '', False); seen = {start}; todo = [start]
    while todo:
        q, w, d = todo.pop()
        for (f, kk, x, t) in T:
            if f == q and kk == 'U':
                nw, nd = push_w(w, d, x, k)
                if (t, nw, nd) not in seen: seen.add((t, nw, nd)); todo.append((t, nw, nd))
    if mode == 'pre':   # a target upper stack is also represented by any suffix of its window with deep = True
        seen |= {(q, w[i:], True) for (q, w, d) in seen for i in range(len(w) + 1)}
    out = set()
    for q, w, d in seen:
        cur = [(q, '')]
        for n in range(LB + 1):
            for s, l in cur:
                if s in final: out.add((w, d, l))
            cur = [(t, l + x) for (s, l) in cur for (f, kk, x, t) in T if f == s and kk == 'L']
    return out

def abstract_reach(T, final, rules, k, mode, lb=None):
    global LB
    old = LB
    if lb: LB = lb
    try: return _abstract_reach(T, final, rules, k, mode)
    finally: LB = old

def _abstract_reach(T, final, rules, k, mode):
    init = abstract_init(T, final, k, mode)
    if mode == 'post':
        res, st = set(init), list(init)
        while st:
            for e in asucc(rules, st.pop(), k):
                if len(e[2]) <= LB and e not in res: res.add(e); st.append(e)
        return res
    windows = [''.join(w) for n in range(k + 1) for w in itertools.product(fuzz.GAMMA, repeat=n)]
    confs = [(w, d, ''.join(l)) for w in windows for d in (False, True) if d or True
             for n in range(LB + 1) for l in itertools.product(fuzz.GAMMA, repeat=n)]
    pred = {}
    for c in confs:
        for e in asucc(rules, c, k): pred.setdefault(e, []).append(c)
    res, st = set(c for c in confs if c in init), [c for c in confs if c in init]
    while st:
        for c in pred.get(st.pop(), []):
            if c not in res: res.add(c); st.append(c)
    return res

def represents(w, d, u): return u.endswith(w) if d else u == w

def case_text(mode, T, rules, final, k):
    lines = ['MODE w%s %s' % (mode, k), 'INIT p', 'A ' + ' '.join(fuzz.GAMMA)]
    lines += ['T %s %s %s %s' % t for t in T]
    for lhs, rhs, j, x in rules:
        lines.append(('Q %d %s ' % (j, x) if j else 'R ') + ' '.join([lhs] + list(rhs)))
    lines += ['F ' + s for s in sorted(final)]
    return '\n'.join(lines) + '\n'

def run(binary, n, seed, karg, modes):
    rng = random.Random(seed)
    stats = {m: Counter() for m in modes}; shown = Counter()
    for i in range(n):
        T, final, ups, rules0 = fuzz.gen_case(rng)
        rules = [(lhs, rhs, rng.randint(1, 3), rng.choice(fuzz.GAMMA)) if rng.random() < 0.35 else (lhs, rhs, 0, '')
                 for lhs, rhs in rules0]
        k = max([j for (_, _, j, _) in rules] + [0]) if karg == 'auto' else int(karg)
        for mode in modes:
            txt = case_text(mode, T, rules, final, karg)
            try:
                p = subprocess.run([binary], input=txt, capture_output=True, text=True, timeout=20, env=ENV)
            except subprocess.TimeoutExpired:
                stats[mode]['timeout'] += 1; continue
            if p.returncode != 0: stats[mode]['crash'] += 1; print(p.stderr[-500:]); continue
            R = fuzz.parse_output(p.stdout); F = fuzz.parse_finals(p.stdout)
            A = abstract_reach(T, final, rules, k, mode)
            small = [c for c in fuzz.CONFS if len(c[1]) <= LB - 3]
            acc = {c for c in small if fuzz.accepts(R, F, *c)}
            want = {(u, l) for (u, l) in small if any(represents(w, d, u) for (w, d, ll) in A if ll == l)}
            real = {c for c in coracle(mode, T, final, rules) if len(c[1]) <= LB - 3}
            v = []
            extra = acc - want
            if extra and mode == 'post':   # the bounded exploration may miss runs whose lower stack grows past LB
                A2 = abstract_reach(T, final, rules, k, mode, LB + 4)
                extra = {(u, l) for (u, l) in extra if not any(represents(w, d, u) for (w, d, ll) in A2 if ll == l)}
            if extra: v.append('extra')
            if want - acc: v.append('missing')
            if real - acc: v.append('UNSOUND')
            v = '+'.join(v) or 'exact for the window model, sound'
            stats[mode][v] += 1
            if v != 'exact for the window model, sound' and shown[v] < 2:
                shown[v] += 1
                print(mode, v, T, rules, 'extra', sorted(acc - want)[:3], 'missing', sorted(want - acc)[:3], 'unsound', sorted(real - acc)[:3])
    for m in modes: print('== window %s* (k=%s): %s' % (m, karg, dict(stats[m])))

if __name__ == '__main__':
    b, n, seed, k = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    w = sys.argv[5] if len(sys.argv) > 5 else 'both'
    run(b, n, seed, k, ['post', 'pre'] if w == 'both' else [w])
