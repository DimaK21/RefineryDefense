#include "storage.h"

#define KEY_OLD_HDR 1      /* pre-single-key layout: deleted on save */
#define KEY_OLD_TOWERS 2
#define KEY_BEST 3
#define KEY_SAVE 4         /* header + towers in one key: written atomically */

_Static_assert(SAVE_LEN <= PERSIST_DATA_MAX_LENGTH, "save must fit one persist key");

void storage_save(const Game *g, bool pending_night) {
  uint8_t buf[SAVE_LEN];
  if (!save_write(g, pending_night, buf, sizeof(buf))) return;
  int written = persist_write_data(KEY_SAVE, buf, sizeof(buf));
  if (written != (int)sizeof(buf)) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "save write failed: %d", written);
    return;
  }
  if (persist_exists(KEY_OLD_HDR)) persist_delete(KEY_OLD_HDR);
  if (persist_exists(KEY_OLD_TOWERS)) persist_delete(KEY_OLD_TOWERS);
}

bool storage_exists(void) {
  return persist_get_size(KEY_SAVE) == SAVE_LEN;
}

bool storage_load(Game *g, bool *pending_night) {
  if (!storage_exists()) return false;
  uint8_t buf[SAVE_LEN];
  if (persist_read_data(KEY_SAVE, buf, sizeof(buf)) != SAVE_LEN) return false;
  return save_read_blob(g, pending_night, buf, sizeof(buf));
}

void storage_clear(void) {
  persist_delete(KEY_SAVE);
  if (persist_exists(KEY_OLD_HDR)) persist_delete(KEY_OLD_HDR);
  if (persist_exists(KEY_OLD_TOWERS)) persist_delete(KEY_OLD_TOWERS);
}

Best storage_best(void) {
  Best b = {0, 0};
  if (persist_get_size(KEY_BEST) == (int)sizeof(Best)) {
    persist_read_data(KEY_BEST, &b, sizeof(b));
  }
  return b;
}

void storage_set_best(Best b) {
  int written = persist_write_data(KEY_BEST, &b, sizeof(b));
  if (written != (int)sizeof(b)) APP_LOG(APP_LOG_LEVEL_ERROR, "best write failed: %d", written);
}
