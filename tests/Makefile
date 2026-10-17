CC ?= clang
CFLAGS = -std=c99 -Wall -Werror -I../src/c/logic -I.
LOGIC = $(wildcard ../src/c/logic/*.c)
TESTS = $(wildcard test_*.c)
BINS = $(TESTS:.c=.bin)

.PHONY: test clean
test: $(BINS)
	@set -e; for b in $(BINS); do echo "== $$b"; ./$$b; done

%.bin: %.c $(LOGIC) $(wildcard ../src/c/logic/*.h) test.h
	$(CC) $(CFLAGS) -o $@ $< $(LOGIC)

clean:
	rm -f *.bin
