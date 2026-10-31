"""Random testing of PostStar / PreStar against a brute-force oracle.
Usage: python3 tests/fuzz.py BINARY [N_CASES] [SEED] [post|pre|both]
BINARY is tests/run_case.c compiled with the library.
Each case: a random APC initial automaton (upper-stack part then lower-stack part, self-loops allowed
on both stacks) and random pop/switch/push rules over the alphabet {a,b,x}.
The oracle enumerates every configuration up to size MAX and applies the UPDS rules directly.
post*: exact up to size MAX (no step makes a configuration smaller, so a run to a configuration of size <= MAX
only goes through configurations of size <= MAX).
pre*: a run from a small configuration into C may go through bigger configurations, so configurations the
library accepts but the bounded oracle does not find are re-checked with a larger bound (MAX+5)."""
import itertools, os, random, re, subprocess, sys
ENV = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
from collections import Counter

GAMMA = 'abx'
MAX = 6
CONFS = [(''.join(w[:k]), ''.join(w[k:])) for n in range(MAX + 1)
         for w in itertools.product(GAMMA, repeat=n) for k in range(n + 1)]

def accepts(T, final, up, low, start='p'):
    cur = {start}
    for kind, c in [('U', c) for c in up] + [('L', c) for c in low]:
        cur = {t for (f, k, l, t) in T if f in cur and k == kind and l == c}
        if not cur: return False
    return bool(cur & set(final))

def succ(rules, c):
    up, low = c
    if not low: return
    g, rest = low[0], low[1:]
    for lhs, rhs in rules:
        if lhs != g: continue
        if not rhs: yield (up + g, rest)
        elif len(rhs) == 1: yield (up, rhs + rest)
        elif up == '': yield ('', rhs + rest)
        else: yield (up[:-1], rhs + rest)

def gen_case(rng):
    T, upper_states = [], ['p']
    cur, idx = 'p', 0
    for _ in range(rng.randint(0, 2)):                      # upper part
        idx += 1; nxt = 'u%d' % idx
        T.append((cur, 'U', rng.choice(GAMMA), nxt)); cur = nxt; upper_states.append(cur)
        for x in rng.sample(GAMMA, rng.choice([0, 0, 1, 2])): T.append((cur, 'U', x, cur))
    for _ in range(rng.randint(1, 3)):                      # lower part
        idx += 1; nxt = 'l%d' % idx
        T.append((cur, 'L', rng.choice(GAMMA), nxt)); cur = nxt
        for x in rng.sample(GAMMA, rng.choice([0, 0, 1, 2])): T.append((cur, 'L', x, cur))
    rules = set()
    for _ in range(rng.randint(2, 5)):
        kind = rng.choice(['pop', 'switch', 'push'])
        lhs = rng.choice(GAMMA)
        if kind == 'pop': rules.add((lhs, ''))
        elif kind == 'switch': rules.add((lhs, rng.choice([g for g in GAMMA if g != lhs])))
        else: rules.add((lhs, rng.choice(GAMMA) + rng.choice(GAMMA)))
    return T, {cur}, upper_states, sorted(rules)

def case_text(mode, T, upper_states, rules, final=()):
    lines = ['MODE ' + mode, 'INIT p', 'A ' + ' '.join(GAMMA)]
    lines += ['T %s %s %s %s' % t for t in T]
    lines += ['R ' + ' '.join([lhs] + list(rhs)) for lhs, rhs in rules]
    lines += ['F ' + s for s in sorted(final)]
    return '\n'.join(lines) + '\n'

def parse_finals(out):
    return {line.split()[1] for line in out.splitlines() if line.startswith('FINAL ')}

def parse_status(out):
    for line in out.splitlines():
        if line.startswith('STATUS '): return line.split()[1]
    return '?'

def parse_output(out):
    R = []
    for line in out.splitlines():
        m = re.match(r'^\((.*?),\((.*?),(.*?)\),(.*)\)$', line.strip())
        if m:
            f, u, l, t = m.groups()
            R.append((f, 'U', u, t) if u != '.' else (f, 'L', l, t))
    return R

def reaches(rules, c, T, final, bound):
    """True if configuration c can reach the initial set C through configurations of size <= bound."""
    seen, st = {c}, [c]
    while st:
        d = st.pop()
        if accepts(T, final, *d): return True
        for e in succ(rules, d):
            if len(e[0]) + len(e[1]) <= bound and e not in seen: seen.add(e); st.append(e)
    return False

def oracle(mode, T, final, rules):
    init = {c for c in CONFS if accepts(T, final, *c)}
    if mode == 'post':
        res, st = set(init), list(init)
        while st:
            for d in succ(rules, st.pop()):
                if len(d[0]) + len(d[1]) <= MAX and d not in res: res.add(d); st.append(d)
        return res
    pred = {}
    for c in CONFS:
        for d in succ(rules, c): pred.setdefault(d, []).append(c)
    res, st = set(init), list(init)
    while st:
        for c in pred.get(st.pop(), []):
            if c not in res: res.add(c); st.append(c)
    return res

def run(binary, n, seed, modes):
    rng = random.Random(seed)
    stats = {m: Counter() for m in modes}
    examples = {m: {} for m in modes}
    for i in range(n):
        T, final, upper_states, rules = gen_case(rng)
        upper_loop = any(k == 'U' and f == t for (f, k, l, t) in T)
        for mode in modes:
            txt = case_text(mode, T, upper_states, rules, final)
            try:
                p = subprocess.run([binary], input=txt, capture_output=True, text=True, timeout=10, env=ENV)
            except subprocess.TimeoutExpired:
                verdict = 'timeout'
            else:
                if p.returncode != 0: verdict = 'crash'
                else:
                    R = parse_output(p.stdout)
                    fin = parse_finals(p.stdout)
                    good = oracle(mode, T, final, rules)
                    acc = {c for c in CONFS if accepts(R, fin, *c)}
                    missing, extra = good - acc, acc - good
                    if mode == 'pre':   # a run into C may pass through bigger configurations: re-check extras
                        extra = {c for c in extra if not reaches(rules, c, T, final, MAX + 5)}
                    status = parse_status(p.stdout)
                    verdict = status + ': ' + ('correct' if not (missing or extra) else
                              ('extra' if not missing else ('missing' if not extra else 'extra+missing')))
            key = verdict + (' (upper loop)' if upper_loop else '')
            stats[mode][key] += 1
            wrong = (verdict.startswith('exact') and not verdict.endswith('correct')) or 'missing' in verdict \
                    or verdict in ('timeout', 'crash')
            if wrong and verdict not in examples[mode]:
                examples[mode][verdict] = txt
    for mode in modes:
        print('== %s* : %d cases' % (mode, n))
        for k, v in sorted(stats[mode].items()): print('   %-32s %d' % (k, v))
        for v, txt in examples[mode].items():
            print('   first "%s" case:\n      ' % v + txt.strip().replace('\n', '\n      '))
    return stats

if __name__ == '__main__':
    binary = sys.argv[1]
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 100
    seed = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    which = sys.argv[4] if len(sys.argv) > 4 else 'both'
    run(binary, n, seed, ['post', 'pre'] if which == 'both' else [which])
