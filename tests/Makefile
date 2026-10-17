CC ?= clang
CFLAGS = -std=c99 -Wall -Werror -I../src/c/logic -I.
LOGIC = $(wildcard ../src/c/logic/*.c)
TESTS = $(wildcard test_*.c)
BINS = $(TESTS:.c=.bin)

.PHONY: test bench clean
test: $(BINS)
	@set -e; for b in $(BINS); do echo "== $$b"; ./$$b; done

%.bin: %.c $(LOGIC) $(wildcard ../src/c/logic/*.h) test.h
	$(CC) $(CFLAGS) -o $@ $< $(LOGIC)

bench: bench.c $(LOGIC) $(wildcard ../src/c/logic/*.h)
	$(CC) $(CFLAGS) -O2 -o bench.out bench.c $(LOGIC)
	./bench.out

clean:
	rm -f *.bin bench.out
