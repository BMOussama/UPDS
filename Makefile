CC=gcc
CFLAGS=-W -Wall -ansi -pedantic
LDFLAGS=
EXEC=main
LIB=Ltl.o Ctl.o Mc.o Check.o Window.o Saturation.o Post.o Pre.o Upds.o Fa.o Transition.o State.o

all: $(EXEC)

main: $(LIB) main.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

main.o: main.c Fa.h Upds.h Post.h
	$(CC) -o $@ -g -c $< $(CFLAGS)

# examples of the report and generic driver used by the random tests
paper_example: $(LIB) tests/paper_example.c
	$(CC) -I. -o $@ $^ $(LDFLAGS)

pre_example: $(LIB) tests/pre_example.c
	$(CC) -I. -o $@ $^ $(LDFLAGS)

stale_example: $(LIB) tests/stale_example.c
	$(CC) -I. -o $@ $^ $(LDFLAGS)

run_case: $(LIB) tests/run_case.c
	$(CC) -I. -o $@ $^ $(LDFLAGS)

mc_case: $(LIB) tests/mc_case.c
	$(CC) -I. -o $@ $^ $(LDFLAGS)

Ltl.o Ctl.o Mc.o: McInternal.h WindowInternal.h
Window.o: WindowInternal.h

test: paper_example pre_example
	./paper_example | python3 tests/check_post.py
	./pre_example 1 | python3 tests/check_pre.py 1
	./pre_example 2 | python3 tests/check_pre.py 2
	./pre_example 3 | python3 tests/check_pre.py 3

# random testing against a brute-force oracle: make fuzz N=100 SEED=1 MODE=post|pre|both
N ?= 50
SEED ?= 1
MODE ?= both
fuzz: run_case
	python3 tests/fuzz.py ./run_case $(N) $(SEED) $(MODE)

# random testing of the bounded-window model: make wfuzz N=100 SEED=1 K=2 MODE=post|pre|both
K ?= 2
wfuzz: run_case
	python3 tests/fuzz_window.py ./run_case $(N) $(SEED) $(K) $(MODE)

# LTL / CTL on the window model against a brute-force oracle: make mcfuzz N=200 SEED=1
mcfuzz: mc_case
	python3 tests/fuzz_mc.py ./mc_case $(N) $(SEED)

clean:
	@rm -rf *.o

mrproper: clean
	@rm -rf $(EXEC) paper_example pre_example run_case stale_example mc_case
