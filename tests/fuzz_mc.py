"""Random testing of LtlCheck / CtlCheck (Ltl.c, Ctl.c) against a brute-force oracle.
Usage: python3 tests/fuzz_mc.py BINARY N_CASES SEED      (BINARY = tests/mc_case.c compiled)

Each case: a random UPDS over {a,b,x} with read rules, a random initial automaton, a window size k, and
random LTL and CTL formulas. The oracle is independent of the library:
 - the window model is explored explicitly (configurations (w, deep, lower stack)); cases whose lower stack
   grows beyond a bound are skipped, so the explored graph is the whole model;
 - every run is infinite: <w, d, empty> stutters, and so does a configuration where no rule surely applies
   (a read below the window may fail);
 - LTL: Vardi-Wolper automaton of the negation (states = truth values of the atoms and of the X / until
   obligations), product with the graph, accepting strongly connected component. An upper letter below the
   window may be any letter at each step (the graph state also carries those letters).
 - CTL: fixpoints on the graph; an atom upJ.x whose letter is below the window is false."""
import itertools, os, random, subprocess, sys
from collections import Counter
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fuzz_window as fw

GAMMA = 'abx'
LB = 6
ENV = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')

# ---------------- the window model ----------------
def push_w(w, d, g, k): return fw.push_w(w, d, g, k)

def succ(rules, c, k):
    w, d, l = c
    if not l: return [c]
    g, rest, out, sure = l[0], l[1:], [], False
    for lhs, rhs, j, x in rules:
        if lhs != g or not fw.read_ok_abstract(w, d, j, x): continue
        if j == 0 or j <= len(w): sure = True
        if rhs == '': nw, nd = push_w(w, d, g, k); out.append((nw, nd, rest))
        elif len(rhs) == 1: out.append((w, d, rhs + rest))
        else: out.append((w[:-1] if w else '', d, rhs + rest))
    if not sure: out.append(c)
    return out

def explore(init, rules, k):
    seen, st, E = set(init), list(init), {}
    while st:
        c = st.pop()
        E[c] = succ(rules, c, k)
        for e in E[c]:
            if len(e[2]) > LB: return None
            if e not in seen: seen.add(e); st.append(e)
    return E

def atom_val(a, c, asg=None):
    w, d, l = c
    if a[0] == 'true': return True
    if a[0] == 'end': return l == ''
    if a[0] == 'at': return l != '' and l[0] == a[1]
    j, x = a[1], a[2]
    if j <= len(w): return w[-j] == x
    if not d: return False
    return asg is not None and asg.get(j) == x       # CTL: unknown -> false

# ---------------- formulas ----------------
ATOMS = [('at', 'a'), ('at', 'b'), ('at', 'x'), ('end',), ('up', 1, 'a'), ('up', 1, 'x'), ('up', 2, 'b'), ('true',)]
def atom_str(a):
    return {'at': lambda: 'at.' + a[1], 'end': lambda: 'at.end', 'true': lambda: 'true',
            'up': lambda: 'up%d.%s' % (a[1], a[2])}[a[0]]()

def gen(rng, depth, ctl):
    if depth == 0 or rng.random() < 0.25: return ('atom', rng.choice(ATOMS))
    un = ['EX', 'AX', 'EF', 'AF', 'EG', 'AG'] if ctl else ['X', 'F', 'G']
    bi = ['EU', 'AU'] if ctl else ['U', 'R']
    op = rng.choice(['not', 'and', 'or', 'imp'] + un + un + bi + bi)
    if op in ['not'] + un: return (op, gen(rng, depth - 1, ctl))
    return (op, gen(rng, depth - 1, ctl), gen(rng, depth - 1, ctl))

def fstr(f):
    op = f[0]
    if op == 'atom': return atom_str(f[1])
    if op == 'not': return '!(%s)' % fstr(f[1])
    if op in ('and', 'or', 'imp', 'U', 'R'):
        return '(%s) %s (%s)' % (fstr(f[1]), {'and': '&', 'or': '|', 'imp': '->', 'U': 'U', 'R': 'R'}[op], fstr(f[2]))
    if op in ('EU', 'AU'): return '%s[(%s) U (%s)]' % (op[0], fstr(f[1]), fstr(f[2]))
    return '%s (%s)' % (op, fstr(f[1]))

# ---------------- CTL oracle ----------------
def ctl_sat(f, E):
    S = set(E)
    def ex(Z): return {c for c in S if any(e in Z for e in E[c])}
    def eu(A, B):
        Z = set(B)
        while True:
            N = Z | (A & ex(Z))
            if N == Z: return Z
            Z = N
    def eg(A):
        Z = set(A)
        while True:
            N = A & ex(Z)
            if N == Z: return Z
            Z = N
    op = f[0]
    if op == 'atom': return {c for c in S if atom_val(f[1], c)}
    if op == 'not': return S - ctl_sat(f[1], E)
    if op == 'and': return ctl_sat(f[1], E) & ctl_sat(f[2], E)
    if op == 'or': return ctl_sat(f[1], E) | ctl_sat(f[2], E)
    if op == 'imp': return (S - ctl_sat(f[1], E)) | ctl_sat(f[2], E)
    if op == 'EX': return ex(ctl_sat(f[1], E))
    if op == 'AX': return S - ex(S - ctl_sat(f[1], E))
    if op == 'EF': return eu(S, ctl_sat(f[1], E))
    if op == 'AG': return S - eu(S, S - ctl_sat(f[1], E))
    if op == 'EG': return eg(ctl_sat(f[1], E))
    if op == 'AF': return S - eg(S - ctl_sat(f[1], E))
    A, B = ctl_sat(f[1], E), ctl_sat(f[2], E)
    if op == 'EU': return eu(A, B)
    return S - (eu(S - B, (S - A) & (S - B)) | eg(S - B))           # AU

# ---------------- LTL oracle (Vardi-Wolper) ----------------
def core(f):
    op = f[0]
    if op == 'atom': return f
    if op == 'not': return ('not', core(f[1]))
    if op in ('and', 'or'): return (op, core(f[1]), core(f[2]))
    if op == 'imp': return ('or', ('not', core(f[1])), core(f[2]))
    if op == 'X': return ('X', core(f[1]))
    if op == 'F': return ('U', ('atom', ('true',)), core(f[1]))
    if op == 'G': return ('not', ('U', ('atom', ('true',)), ('not', core(f[1]))))
    if op == 'U': return ('U', core(f[1]), core(f[2]))
    return ('not', ('U', ('not', core(f[1])), ('not', core(f[2]))))   # R

def subs(f, out):
    out.add(f)
    for g in f[1:]:
        if isinstance(g, tuple) and g and isinstance(g[0], str) and g[0] in ('atom', 'not', 'and', 'or', 'X', 'U'): subs(g, out)
    return out

def ltl_holds(f, E, init):
    phi = core(f)
    S = subs(phi, set())
    props = sorted({g[1] for g in S if g[0] == 'atom' and g[1][0] != 'true'})
    xs = sorted({g for g in S if g[0] == 'X'}, key=repr)
    us = sorted({g for g in S if g[0] == 'U'}, key=repr)
    el = [('p', a) for a in props] + [('x', g) for g in xs] + [('u', g) for g in us]
    if len(el) > 14: return None
    def sat(g, s):
        op = g[0]
        if op == 'atom': return True if g[1][0] == 'true' else s[('p', g[1])]
        if op == 'not': return not sat(g[1], s)
        if op == 'and': return sat(g[1], s) and sat(g[2], s)
        if op == 'or': return sat(g[1], s) or sat(g[2], s)
        if op == 'X': return s[('x', g)]
        return sat(g[2], s) or (sat(g[1], s) and s[('u', g)])
    states = [dict(zip(el, v)) for v in itertools.product([False, True], repeat=len(el))]
    obligations = [(('x', g), g[1]) for g in xs] + [(('u', g), g) for g in us]
    nexts = {i: [j for j, t in enumerate(states) if all(states[i][e] == sat(h, t) for e, h in obligations)]
             for i in range(len(states))}
    accs = [{i for i, s in enumerate(states) if sat(u[2], s) or not sat(u, s)} for u in us]
    # graph states: (configuration, letters below the window at the read positions of the formula)
    pos = sorted({a[1] for a in props if a[0] == 'up'})
    vals = {j: sorted({a[2] for a in props if a[0] == 'up' and a[1] == j}) + [None] for j in pos}
    def asgs(c):
        w, d, l = c
        free = [j for j in pos if j > len(w) and d]
        for v in itertools.product(*[vals[j] for j in free]): yield dict(zip(free, v))
    def match(c, asg, i):
        s = states[i]
        return all(s[('p', a)] == atom_val(a, c, asg) for a in props)
    start = [(c, tuple(sorted(a.items())), i) for c in init for a in asgs(c) for i in range(len(states))
             if not sat(phi, states[i]) and match(c, a, i)]
    seen, st, adj = set(start), list(start), {}
    while st:
        node = st.pop(); c, a, i = node
        adj[node] = []
        for e in E[c]:
            for a2 in asgs(e):
                for j in nexts[i]:
                    if match(e, a2, j):
                        n2 = (e, tuple(sorted(a2.items())), j)
                        adj[node].append(n2)
                        if n2 not in seen: seen.add(n2); st.append(n2)
    for comp in sccs(adj):
        cs = set(comp)
        if len(comp) == 1 and comp[0] not in adj[comp[0]]: continue
        if all(any(n[2] in A for n in comp) for A in accs): return False
    return True

def sccs(adj):
    index, low, on, stack, out, idx = {}, {}, set(), [], [], [0]
    for root in adj:
        if root in index: continue
        work = [(root, iter(adj[root]))]; index[root] = low[root] = idx[0]; idx[0] += 1; stack.append(root); on.add(root)
        while work:
            v, it = work[-1]
            nxt = next(it, None)
            if nxt is not None:
                if nxt not in index:
                    index[nxt] = low[nxt] = idx[0]; idx[0] += 1; stack.append(nxt); on.add(nxt)
                    work.append((nxt, iter(adj[nxt])))
                elif nxt in on: low[v] = min(low[v], index[nxt])
            else:
                work.pop()
                if work: low[work[-1][0]] = min(low[work[-1][0]], low[v])
                if low[v] == index[v]:
                    comp = []
                    while True:
                        x = stack.pop(); on.discard(x); comp.append(x)
                        if x == v: break
                    out.append(comp)
    return out

# ---------------- cases ----------------
def gen_case(rng):
    T, cur, idx = [], 'p', 0
    for _ in range(rng.randint(0, 2)):
        idx += 1; nxt = 'u%d' % idx
        T.append((cur, 'U', rng.choice(GAMMA), nxt)); cur = nxt
        for x in rng.sample(GAMMA, rng.choice([0, 0, 1, 3])): T.append((cur, 'U', x, cur))
    ends = []
    for branch in range(rng.choice([1, 1, 2])):
        c2 = cur
        for _ in range(rng.randint(0, 2)):
            idx += 1; nxt = 'l%d' % idx
            T.append((c2, 'L', rng.choice(GAMMA), nxt)); c2 = nxt
        ends.append(c2)
    rules = set()
    for _ in range(rng.randint(2, 6)):
        kind = rng.choice(['pop', 'pop', 'switch', 'switch', 'push'])
        lhs = rng.choice(GAMMA)
        rhs = '' if kind == 'pop' else rng.choice(GAMMA) if kind == 'switch' else rng.choice(GAMMA) + rng.choice(GAMMA)
        j = rng.randint(1, 2) if rng.random() < 0.35 else 0
        rules.add((lhs, rhs, j, rng.choice(GAMMA) if j else ''))
    if rng.random() < 0.5:                 # a call that returns: y -> z y', z -> eps (loops with a bounded stack)
        y, z = rng.choice(GAMMA), rng.choice(GAMMA)
        rules.add((y, z + rng.choice(GAMMA), 0, '')); rules.add((z, '', 0, ''))
    return T, set(ends), sorted(rules)

def gen_ltl(rng):
    if rng.random() < 0.5: return gen(rng, 3, False)
    lit = lambda: ('atom', rng.choice(ATOMS)) if rng.random() < 0.6 else ('not', ('atom', rng.choice(ATOMS)))
    p, q = lit(), lit()
    return rng.choice([('G', ('F', p)), ('F', ('G', p)), ('G', ('imp', p, ('F', q))), ('imp', ('G', ('F', p)), ('G', ('F', q))),
                       ('G', ('imp', p, ('X', q))), ('U', p, q), ('F', ('and', p, ('X', q)))])

def case_text(T, rules, final, karg, queries):
    lines = ['K %s' % karg, 'INIT p', 'A ' + ' '.join(GAMMA)]
    lines += ['T %s %s %s %s' % t for t in T]
    for lhs, rhs, j, x in rules:
        lines.append(('Q %d %s ' % (j, x) if j else 'R ') + ' '.join([lhs] + list(rhs)))
    lines += ['F ' + s for s in sorted(final)]
    lines += queries
    return '\n'.join(lines) + '\n'

def run(binary, n, seed):
    rng = random.Random(seed)
    st = Counter()
    for case in range(n):
        T, final, rules = gen_case(rng)
        karg = rng.choice(['auto', 0, 1, 2, 3])
        ltl = [gen_ltl(rng) for _ in range(6)]
        ctl = [gen(rng, 3, True) for _ in range(6)]
        def kof(f):
            if karg != 'auto': return karg
            return max([j for (_, _, j, _) in rules] + [0] + [a[1] for a in atoms_of(f) if a[0] == 'up'])
        models = {}
        for kk in {kof(f) for f in ltl + ctl}:
            init = fw.abstract_init(T, final, kk)
            models[kk] = (init, explore(init, rules, kk))
        if any(E is None for (_, E) in models.values()): st['skipped (unbounded)'] += 1; continue
        if not any(init for (init, _) in models.values()): st['skipped (empty)'] += 1; continue
        q = ['LTL ' + fstr(f) for f in ltl] + ['CTL ' + fstr(f) for f in ctl]
        txt = case_text(T, rules, final, karg, q)
        try:
            p = subprocess.run([binary], input=txt, capture_output=True, text=True, timeout=60, env=ENV)
        except subprocess.TimeoutExpired:
            st['timeout'] += 1; continue
        if p.returncode != 0: st['crash'] += 1; print(p.stderr[-800:]); print(txt); continue
        got = [int(line.split()[1]) for line in p.stdout.splitlines()]
        S = None
        for f, g, line in zip(ltl + ctl, got, q):
            init, E = models[kof(f)]
            want = ltl_holds(f, E, init) if line.startswith('LTL') else int(init <= ctl_sat(f, E))
            if want is None: st['formula skipped'] += 1; continue
            kind = line[:3]
            st[kind + (' holds' if want else ' violated')] += 1
            if int(want) != g:
                st[kind + ' WRONG'] += 1
                if st[kind + ' WRONG'] <= 3:
                    print('--- case %d: %s  library %d, oracle %d' % (case, line, g, int(want))); print(txt)
    print(dict(st))
    return st

def atoms_of(f):
    if f[0] == 'atom': return [f[1]]
    return [a for g in f[1:] for a in atoms_of(g)]

if __name__ == '__main__':
    b = sys.argv[1]; n = int(sys.argv[2]) if len(sys.argv) > 2 else 100; seed = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    st = run(b, n, seed)
    sys.exit(1 if any('WRONG' in key or key in ('crash', 'timeout') for key in st) else 0)
