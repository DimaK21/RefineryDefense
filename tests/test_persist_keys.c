#include "test.h"
#include "../src/c/app/persist_keys.h"

/* Settings and the save once shared key 5: the records' first byte switched the sound on. */
static void test_keys_distinct(void) {
  const int keys[] = PERSIST_KEYS;
  int n = (int)(sizeof(keys) / sizeof(keys[0]));
  for (int i = 0; i < n; i++)
    for (int j = i + 1; j < n; j++) CHECK(keys[i] != keys[j]);
  CHECK(KEY_SOUND != KEY_BESTS);
  CHECK(KEY_NAME != KEY_SENT_BESTS);
  CHECK(KEY_NAME != KEY_BESTS && KEY_SENT_BESTS != KEY_BESTS);
}

int main(void) {
  RUN(test_keys_distinct);
  return TEST_SUMMARY();
}
