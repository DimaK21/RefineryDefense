#include <string.h>
#include "test.h"
#include "board.h"

static void test_name_cycle(void) {
  CHECK_EQ(name_cycle('A', 1), 'B');
  CHECK_EQ(name_cycle('Z', 1), '0');
  CHECK_EQ(name_cycle('9', 1), ' ');
  CHECK_EQ(name_cycle(' ', 1), 'A');
  CHECK_EQ(name_cycle('A', -1), ' ');
  CHECK_EQ(name_cycle('0', -1), 'Z');
  CHECK_EQ(name_cycle('a', 1), 'A');
}

static void test_name_trim_valid(void) {
  char s[] = "  DIM ";
  name_trim(s);
  CHECK(strcmp(s, "DIM") == 0);
  char blank[] = "   ";
  name_trim(blank);
  CHECK(strcmp(blank, "") == 0);
  CHECK(name_valid("DIM"));
  CHECK(name_valid("A1 B2"));
  CHECK(name_valid(" DIM "));
  CHECK(!name_valid("      "));
  CHECK(!name_valid(""));
  CHECK(!name_valid("ABCDEFG"));
  CHECK(!name_valid("ab"));
}

static void test_parse_row(void) {
  BoardRow r;
  const char *a = "14|DIM|12|3400";
  CHECK(board_parse_row(a, strlen(a), &r));
  CHECK_EQ(r.rank, 14); CHECK(strcmp(r.name, "DIM") == 0);
  CHECK_EQ(r.nights, 12); CHECK_EQ(r.score, 3400);
  const char *b = "3||5|10";
  CHECK(board_parse_row(b, strlen(b), &r));
  CHECK(strcmp(r.name, "") == 0); CHECK_EQ(r.rank, 3);
  const char *c = "1|ABCDEFGH|2|3";
  CHECK(board_parse_row(c, strlen(c), &r));
  CHECK(strcmp(r.name, "ABCDEF") == 0); CHECK_EQ(r.score, 3);
  const char *bad1 = "x|A|1|1", *bad2 = "1|A|1", *bad3 = "1|A|1|", *bad4 = "1|A|1|2|3";
  CHECK(!board_parse_row(bad1, strlen(bad1), &r));
  CHECK(!board_parse_row(bad2, strlen(bad2), &r));
  CHECK(!board_parse_row(bad3, strlen(bad3), &r));
  CHECK(!board_parse_row(bad4, strlen(bad4), &r));
  CHECK(!board_parse_row("", 0, &r));
}

static void test_parse_rows(void) {
  BoardRow rows[BOARD_MAX_ROWS];
  CHECK_EQ(board_parse_rows("1|A|9|900\nbad\n2|B|8|800", rows, BOARD_MAX_ROWS), 2);
  CHECK_EQ(rows[1].rank, 2); CHECK(strcmp(rows[1].name, "B") == 0);
  CHECK_EQ(board_parse_rows("1|A|9|900\n2|B|8|800", rows, 1), 1);
  CHECK_EQ(board_parse_rows("", rows, BOARD_MAX_ROWS), 0);
  CHECK_EQ(board_parse_rows(NULL, rows, BOARD_MAX_ROWS), 0);
}

static void test_best_beats(void) {
  CHECK(best_beats((Best){12, 3000}, (Best){11, 9000}));
  CHECK(best_beats((Best){12, 3001}, (Best){12, 3000}));
  CHECK(!best_beats((Best){12, 3000}, (Best){12, 3000}));
  CHECK(!best_beats((Best){11, 9000}, (Best){12, 3000}));
  CHECK(!best_beats((Best){0, 0}, (Best){0, 0}));
}

int main(void) {
  RUN(test_name_cycle);
  RUN(test_name_trim_valid);
  RUN(test_parse_row);
  RUN(test_parse_rows);
  RUN(test_best_beats);
  return TEST_SUMMARY();
}
