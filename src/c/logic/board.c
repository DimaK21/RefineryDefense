#include <string.h>
#include "board.h"

static const char NAME_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";
#define NAME_CHARS_N ((int)sizeof(NAME_CHARS) - 1)

static int prv_char_index(char c) {
  for (int i = 0; i < NAME_CHARS_N; i++) if (NAME_CHARS[i] == c) return i;
  return -1;
}

char name_cycle(char c, int dir) {
  int i = prv_char_index(c);
  if (i < 0) return 'A';
  return NAME_CHARS[(i + NAME_CHARS_N + (dir < 0 ? -1 : 1)) % NAME_CHARS_N];
}

void name_trim(char *s) {
  size_t n = strlen(s), start = 0;
  while (start < n && s[start] == ' ') start++;
  while (n > start && s[n - 1] == ' ') n--;
  memmove(s, s + start, n - start);
  s[n - start] = '\0';
}

bool name_valid(const char *s) {
  size_t n = strlen(s), start = 0;
  while (start < n && s[start] == ' ') start++;
  while (n > start && s[n - 1] == ' ') n--;
  if (n == start || n - start > NAME_LEN) return false;
  for (size_t i = start; i < n; i++) if (prv_char_index(s[i]) < 0) return false;
  return true;
}

/* Decimal digits in [p, end): at least one, no sign, fits uint32. */
static bool prv_parse_u32(const char *p, const char *end, uint32_t *out) {
  if (p == end || end - p > 10) return false;
  uint64_t v = 0;
  for (; p < end; p++) {
    if (*p < '0' || *p > '9') return false;
    v = v * 10 + (uint64_t)(*p - '0');
  }
  if (v > UINT32_MAX) return false;
  *out = (uint32_t)v;
  return true;
}

bool board_parse_row(const char *line, size_t len, BoardRow *out) {
  const char *f[4], *e[4], *p = line, *end = line + len;
  for (int i = 0; i < 4; i++) {
    f[i] = p;
    while (p < end && *p != '|') p++;
    e[i] = p;
    if (i < 3) {
      if (p == end) return false;
      p++;
    }
  }
  if (p != end) return false;   /* a fifth field */
  uint32_t rank, nights, score;
  if (!prv_parse_u32(f[0], e[0], &rank) || !prv_parse_u32(f[2], e[2], &nights) ||
      !prv_parse_u32(f[3], e[3], &score) || nights > UINT16_MAX)
    return false;
  size_t name_len = (size_t)(e[1] - f[1]);
  if (name_len > NAME_LEN) name_len = NAME_LEN;
  memcpy(out->name, f[1], name_len);
  out->name[name_len] = '\0';
  out->rank = rank;
  out->nights = (uint16_t)nights;
  out->score = score;
  return true;
}

int board_parse_rows(const char *packed, BoardRow *out, int max) {
  if (!packed) return 0;
  int n = 0;
  const char *p = packed;
  while (*p && n < max) {
    const char *nl = strchr(p, '\n');
    size_t len = nl ? (size_t)(nl - p) : strlen(p);
    if (board_parse_row(p, len, &out[n])) n++;
    if (!nl) break;
    p = nl + 1;
  }
  return n;
}

bool best_beats(Best a, Best b) {
  return a.nights > b.nights || (a.nights == b.nights && a.score > b.score);
}
