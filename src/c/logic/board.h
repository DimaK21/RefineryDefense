#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "save.h"

/* Online leaderboard rules shared by the screens: player name, table rows from the phone. */
#define NAME_LEN 6
#define BOARD_MAX_ROWS 10

typedef struct { uint32_t rank; char name[NAME_LEN + 1]; uint16_t nights; uint32_t score; } BoardRow;

/* Next/previous name character: A..Z, 0..9, space, wrapping. Anything else becomes 'A'. */
char name_cycle(char c, int dir);
void name_trim(char *s);   /* strips spaces at both ends, in place */
/* Non-empty after trimming, at most NAME_LEN, only characters name_cycle walks through. */
bool name_valid(const char *s);

/* One "rank|name|nights|score" line (len bytes, no terminator needed). Long names are cut. */
bool board_parse_row(const char *line, size_t len, BoardRow *out);
/* Lines joined by '\n'; broken ones are skipped. Returns how many landed in out. */
int board_parse_rows(const char *packed, BoardRow *out, int max);

/* a ranks above b: more nights, or as many nights and more points. */
bool best_beats(Best a, Best b);
