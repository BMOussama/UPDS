# UPDS — reachability for pushdown systems with an upper stack

A small C library that computes the sets of **successors (post\*)** and **predecessors (pre\*)** of a
regular set of configurations of a *pushdown system with an upper stack* (UPDS).

It was written during a Master's internship (2019) at the LIPN laboratory (Université Paris 13 / Sorbonne
Paris Nord), supervised by Tayssir Touili.

---

## Why an upper stack?

Pushdown systems (PDS) are the classic model for the call stack of a sequential program, and they are
widely used to model-check binaries (for example, to detect malware). But they do not behave like a real
machine stack:

- in a **PDS**, a `pop` destroys the top symbol;
- on a **real processor**, a `pop` only moves the stack pointer. The value stays in memory just *above*
  the stack pointer until a later `push` overwrites it.

Programs can still read that memory (`mov eax, [esp-4]`), and malware authors use it to hide their payload
from static analysis. A PDS cannot see it.

A **UPDS** fixes this by keeping two stacks that meet at the stack pointer:

- the **lower stack**: the usual stack contents;
- the **upper stack**: the symbols that were popped and not yet overwritten.

A configuration is `<p, w_u, w_l>`: a control state, the upper stack and the lower stack. The rules are
the same as for a PDS, but their meaning changes:

| Rule | Effect |
|---|---|
| pop `<p,γ> → <p',ε>` | `γ` leaves the lower stack and is added to the bottom of the upper stack |
| switch `<p,γ> → <p',γ'>` | the top of the lower stack `γ` is replaced by `γ'`, the upper stack is unchanged |
| push `<p,γ> → <p',γ'γ''>` | `γ` is replaced by `γ'γ''`; the push overwrites the last letter of the upper stack (if any) |

## The problem

Model checking needs the reachable sets (post\*, pre\*) to be **regular**, so they can be represented by
finite automata and intersected with sets of "bad" configurations.

For ordinary PDS this is always the case. For UPDS it is not: Pommellet, Diaz and Touili showed that,
starting from a regular set, post\* and pre\* of a UPDS can be non-regular (the forward set is
context-sensitive). Intuitively, the push rule lets the upper stack reorder letters like the
semi-commutation `ba → ab`, and applied to a set such as `(ab)*` this produces `{aⁿbⁿ}`.

## The approach

The idea explored here is to restrict the initial set to **Alphabetic Pattern Constraints (APC)**: unions
of products of single letters and star expressions `(a₁+…+aₙ)*`. Their automata only contain self-loops,
and APC are closed under semi-commutation rewriting. The conjecture is that for an APC set of
configurations, post\* and pre\* of a UPDS are regular and computable.

The library represents sets of configurations with **P-automata** whose transitions read either the upper
stack `(γ, ε)` or the lower stack `(ε, γ)`; an accepted path reads the whole upper stack first, then the
lower stack. post\* and pre\* are computed by **saturation**: transitions (and a few new states) are added
until nothing changes.

## Repository layout

| File | Content |
|---|---|
| `Saturation.c/.h` | **the corrected post\* / pre\* saturation** (`SaturatePost`, `SaturatePre`) |
| `Window.c/.h` | **bounded-window model**, always regular and computable (`WindowPostStar`, `WindowPreStar`) |
| `Post.c/.h`, `Pre.c/.h` | `PostStar(fa, upds)`, `PreStar(fa, upds)`: same, with a warning when widening was used |
| `State.c/.h` | automaton states (`SetFinal` marks final states) |
| `Transition.c/.h` | transitions; `lower = true` means `(ε,γ)`, `false` means `(γ,ε)` |
| `Fa.c/.h` | P-automaton (list of transitions + initial state) |
| `Upds.c/.h` | UPDS rules (pop / switch / push), `UpdsSetAlphabet` |
| `main.c` | small demo |
| `tests/` | examples, brute-force checkers, random tester, Python reference implementations |
| `legacy/` | the 2019 implementation of the report's rules (`Post_2019.c`), kept for comparison |

## Build and run

Requirements: `gcc`, `make`, and `python3` for the tests.

```bash
make            # builds the demo ./main
./main          # prints the post* automaton of the demo
make test       # the report's examples, checked by brute force
make fuzz       # random testing (N=100 SEED=2 MODE=post|pre|both)
make wfuzz      # random testing of the window model (K=2 = window size)
make clean      # removes object files
```

## Using the library

```c
#include "Upds.h"
#include "Fa.h"
#include "Post.h"
#include "Pre.h"

/* states of the initial automaton; final states are marked with SetFinal */
State* p = CreateState(CreateStateName("p",""), false, false, false);
State* q = CreateState(CreateStateName("q",""), false, false, false);
SetFinal(q, true);

/* C0 = <p, ε, x x*> : lower-stack transitions (last argument true) */
ListTransition* l = CreateListTransition(CreateTransition(p, q, "x", true));
AddToListTransition(l, CreateTransition(q, q, "x", true));
Fa* fa = CreateFa(l, p);

/* rules: <p,x>-><p,a>  <p,a>-><p,ε>  <p,b>-><p,ε>  <p,a>-><p,ab> */
ListRule* r = CreateListRule(CreateRule(p, "x", p, "a", NULL));   /* switch */
AddRule(r, CreateRule(p, "a", p, NULL, NULL));                    /* pop    */
AddRule(r, CreateRule(p, "b", p, NULL, NULL));                    /* pop    */
AddRule(r, CreateRule(p, "a", p, "a", "b"));                      /* push   */
Upds* upds = CreateUpds(r);

char* gamma[] = { "a", "b", "x" };
UpdsSetAlphabet(upds, gamma, 3);         /* stack alphabet (needed by pre*) */

UpdsStatus status;
Fa* post = SaturatePost(fa, upds, UPDS_DEFAULT_CAP, &status);   /* or PostStar(fa, upds) */
PrintFa(post);
if (status == UPDS_OVER) { /* widening was used: over-approximation */ }
```

Transitions are printed as `(from,(upper,lower),to)`, where `.` means ε, followed by the final states
(`FINAL s`). The input automaton is not modified.

Assumptions: a single control state `p`, which is the initial state of the automaton, and no transition
leading into `p`. For pre\*, declare the stack alphabet with `UpdsSetAlphabet`: a push overwrites *any*
letter of Γ, including letters that appear in no rule.

## Checking the examples

`make test` runs four examples, all with the rules
`<p,x>→<p,a>`, `<p,a>→<p,ε>`, `<p,b>→<p,ε>`, `<p,a>→<p,ab>`:

| Command | What it checks |
|---|---|
| `./paper_example \| python3 tests/check_post.py` | post\* of `<p, ε, x x*>` |
| `./pre_example 1 \| python3 tests/check_pre.py 1` | pre\* of `<p, ε, abb>` |
| `./pre_example 2 \| python3 tests/check_pre.py 2` | pre\* of `<p, ab*, x>` |
| `./pre_example 3 \| python3 tests/check_pre.py 3` | pre\* of `<p, b, a x*>` |

The Python checkers do not trust the library: they apply the UPDS rules directly to every configuration up
to a fixed size (7), compute the exact reachable set, and compare it with what the automaton accepts.
Since no UPDS step makes a configuration smaller, this is exact for post\* up to that size. For pre\*, a
run into C can pass through bigger configurations, so the random tester re-checks reported extras with a
larger bound.

## Bounded-window model (always regular)

Because of the counterexample below, post\* of a UPDS cannot always be regular. But a program only reads
above the stack pointer at bounded offsets (`[esp-4]`, `[esp-8]`, …). `Window.c` keeps the **last k
letters** of the upper stack exactly and forgets what lies below them:

- an abstract upper stack is `(w, deep)`, with `|w| ≤ k`; it stands for `w` (deep = false) or `Γ* w`
  (deep = true);
- pop `γ`: `w := last k letters of w·γ` (deep becomes true when a letter leaves the window);
- push: `w := w` without its last letter (when `w` is empty and deep, an unknown letter is erased).

There are finitely many pairs `(w, deep)`, so they become control states of an ordinary pushdown system,
and post\* / pre\* are computed with the classic saturation (Schwoon). The result is **always regular and
always computed**, it is **exact for the window model** (exact on the last k letters above the stack
pointer), and it **contains every configuration that is really reachable** in the full UPDS. When no
reachable upper stack has more than k letters, it is exact for the full UPDS too.

```c
Rule* rule = CreateRule(p, "x", p, "a", NULL);
RuleSetRead(rule, 2, "x");                 /* applies only if [esp-8] holds x */

Fa* reach = WindowPostStar(fa, upds, 2);   /* window of 2 letters: [esp-4] and [esp-8] are exact */
Fa* back  = WindowPreStar(fa, upds, WINDOW_AUTO);   /* k = largest read offset of the rules */
```

Cost: up to `2 · (1 + |Γ| + … + |Γ|^k)` control states, fine for small windows.

**Read rules.** A program's behaviour can depend on a value read above the stack pointer
(`mov eax,[esp-8]; cmp eax,0; je …`). `RuleSetRead(rule, j, x)` makes a rule apply only when the j-th
upper letter (j = 1 is `[esp-4]`) is `x`. The window model applies it exactly when that letter is inside the
window, and allows it when the letter is below the window (unknown), which keeps the result sound.
`SaturatePost` / `SaturatePre` ignore read conditions and report `UPDS_OVER`.

**Choosing k.** `WindowPostStar(fa, upds, WINDOW_AUTO)` uses the largest read offset of the rules, so every
read is inside the window. A read can still hit an unknown letter when that letter left the window earlier
(more pops than k, then pushes back): to cover such patterns, take k a few letters larger. In the random
tests, k = largest offset gave the same reachable lower stacks as the full UPDS in 199 of 200 cases, and
k = largest offset + 3 in all 200.


Tests (`make wfuzz`, `tests/fuzz_window.py`, under AddressSanitizer): an independent brute-force version of
the window semantics, and the brute-force oracle of the full UPDS. On 600 post\* cases (k = 0 to 4) and 110
pre\* cases (k = 1 to 3), then 400 post\* and 30 pre\* cases with read rules (k = auto and 1 to 3): exact
for the window model and sound for the full UPDS in **every** case.

## What was wrong with the 2019 rules

`tests/report_post.py` implements the post\* rules of the report literally, independently of the C code,
and `tests/report_post_blame.py` finds which rule causes each wrong result. On 280 random cases the rules
always terminate and are exact in 81% of the cases. The other cases come from four places:

1. **Push states shared between different upper-stack contexts.** (iii) β2-2 names the new state
   `q_{s,γ'}` after `s` when the predecessor is `D_{s,α}`, so a state created for one context is reused
   for another. Example: `C = <p, x, bb>`, rules `b→xx`, `x→ε`: `<p, ε, xx>` is accepted but not reachable.
2. **Self-loop in (ii) β1, "q' is a pushed state".** The loop `(q',(ε,γ'),q')` lets a switch change a
   letter below the top of the lower stack. Example: `C = <p, ε, b>`, rules `a→b`, `b→x`, `x→ε`, `x→ab`:
   `<p, ε, axb>` is accepted but not reachable.
3. **(iii) β2-1, first transition `(q',(ε,γ'),q_{s,γ'})`.** It starts after the upper letter that the push
   overwrites, so that letter is kept. Example: `C = <p, ε, a>`, rules `a→bx`, `b→ε`, `x→a`:
   `<p, b, bx>` is accepted but not reachable.
4. **Repeated pushes with a loop on the upper stack are not covered.** Example: `C = <p, a⁺, xb>`, rules
   `b→bb`, `x→b`: `<p, a, bbbb>` is reachable but not accepted.

The 2019 C code (`legacy/Post_2019.c`) also had implementation bugs of its own (memory errors, missing
transitions, wrong state names).

## Corrected algorithm

The corrected saturation (`Saturation.c`; same algorithm in Python in `tests/reference.py`) keeps the idea
of the report (saturation, duplicated states) and fixes the four points above with one invariant.

**Stack-pointer discipline.** A *top* state is a state reachable from `p` by upper transitions only: the
stack pointer can be there. Rules are applied only at top states, and a lower transition never enters a
top state: it enters the state's **lower twin** `W(m)` (same lower transitions as `m`, kept in sync, final
iff `m` is). So a transition added at the top of the lower stack can never be used in its middle
(this is what the duplicated states `D_q` were meant to guarantee; here it holds for every state).

post\* (for a top state `q'` and a lower transition `q' –(ε,γ)→ q`):

| Rule | Added transitions |
|---|---|
| (i) pop `<p,γ>→<p,ε>` | `(q',(γ,ε),T(q))`, where `T(q)` is a top copy of `q` (same lower transitions) |
| (ii) switch `<p,γ>→<p,γ'>` | `(q',(ε,γ'),q)` |
| (iii) push `<p,γ>→<p,γ'γ''>` | for every upper transition `r –(x,ε)→ q'` (x is overwritten), and for `r = p` if `q' = p` (empty upper stack): `(r,(ε,γ'),M(r,γ'))` and `(M(r,γ'),(ε,γ''),q)` |

pre\* (for a top state `q'`):

| Rule | Condition | Added transitions |
|---|---|---|
| (i) pop | `q' –(γ,ε)→ q''` | `(q',(ε,γ),q'')` |
| (ii) switch | `q' –(ε,γ')→ q` | `(q',(ε,γ),q)` |
| (iii) push | `q' –(ε,γ')→ q1 –(ε,γ'')→ q` | `(q',(x,ε),N(q,γ))` for every `x ∈ Γ`, `(N(q,γ),(ε,γ),q)`, and `(p,(ε,γ),q)` if `q' = p` |

The push state `M(r,γ')` is named after the state `r` where the upper stack ends, so it is never shared
between two contexts (point 1), and the push starts at `r`, before the overwritten letter (point 3). No
rule adds a lower self-loop on a state that can be in the middle of the lower stack (point 2), and every
upper predecessor of the top is considered (point 4).

**Why the result is right.** *Soundness:* each added transition only creates accepted paths that are one
step away from already accepted paths (the discipline guarantees that new transitions are used at the
top of the lower stack only). *Completeness:* at the fixpoint, for every accepted configuration and every
rule that applies to it, the successor (predecessor) is accepted, so the language is closed under one
step and contains C, hence contains post\* (pre\*).

**Termination and widening.** New states can be created forever when the upper stack can grow without
bound (loops). After `cap` new states (`UPDS_DEFAULT_CAP = 40`), **widening** starts: all new states of
one kind (twin, top copy, push state) for one letter become a single summary state. The number of states
is then bounded and the saturation always stops. The result is an **over-approximation** that is still
closed under one step (an inductive invariant), which is sound for proving that a bad configuration is
**not** reachable. `status` tells which case happened (`UPDS_EXACT` or `UPDS_OVER`).

## Status

Random tests of the C library (`make fuzz`, 500 post\* and 500 pre\* cases with loops on both stacks, under
AddressSanitizer):

| | exact (fixpoint) | widened, still exact up to size 6 | widened, over-approximation | wrong / missing / crash |
|---|---|---|---|---|
| post\* | 447 (89%) | 51 | 2 | **0** |
| pre\* | 481 (96%) | 18 | 1 | **0** |

- Every result that reaches a fixpoint is exact; no result ever misses a reachable configuration.
- On the report's post\* example (`<p, ε, x x*>`) and on pre\* example 2 (`<p, ab*, x>`), widening is used;
  the results are still exact up to the checked size. The 2019 rules stop by themselves on the post\*
  example thanks to their self-loops on duplicated states, but the same self-loops cause errors 1–3 in
  other cases.
- **The conjecture is false as stated.** `tests/counterexample.py`: with the rules `c→db`, `d→ca`,
  `c→ε`, `a→ε`, `b→ε`, `a→ab` and the single configuration `C = <p, ε, c x>` (an APC set), the
  reachable configurations `<p, c a^i b^j, x>` are exactly those with `j ≥ i` and `j − i` even (checked up
  to size 16), which is not regular. The reason: while the upper stack is empty, the push rules *build*
  a non-APC lower stack `c (ab)^n x` from one letter, and then the report's own counterexample applies.
  So no algorithm can be exact *and* always terminate on all APC inputs; the library's behaviour (exact
  when the saturation stops, sound over-approximation otherwise) is the best possible in general.
- **Refined conjecture (next research step):** post\* is regular when the lower stacks reachable by the
  pushdown part (post\*_PDS of C, which is regular and computable) stay in a class closed under the
  reorderings caused by the push rules (APC is such a class for the initial set, but the pushes can leave it).
  A pen-and-paper proof that `j ≥ i` for all sizes in the counterexample is still to be written.
- Only one control state is supported.

## References

- A. Pommellet, M. Diaz, T. Touili. *Reachability analysis of pushdown systems with an upper stack*.
  LATA 2017; extended version: arXiv:1811.02982.
- A. Bouajjani, J. Esparza, O. Maler. *Reachability analysis of pushdown automata: application to
  model-checking*. CONCUR 1997.
- J. Esparza, D. Hansel, P. Rossmanith, S. Schwoon. *Efficient algorithms for model checking pushdown
  systems*. CAV 2000.
- A. Bouajjani, A. Muscholl, T. Touili. *Permutation rewriting and algorithmic verification*.
  Information and Computation, 2007.
