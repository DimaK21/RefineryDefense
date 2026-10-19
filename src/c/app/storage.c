#include "storage.h"
#include "persist_keys.h"

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
  int size = persist_get_size(KEY_SAVE);
  return size == SAVE_LEN || size == SAVE_LEN_V1;
}

Difficulty storage_saved_difficulty(void) {
  uint8_t hdr[SAVE_HDR_LEN];
  if (persist_get_size(KEY_SAVE) != SAVE_LEN) return DIFF_NORMAL;   /* v1 saves are Normal */
  if (persist_read_data(KEY_SAVE, hdr, sizeof(hdr)) != (int)sizeof(hdr)) return DIFF_NORMAL;
  return hdr[SAVE_HDR_LEN - 1] < DIFF_COUNT ? (Difficulty)hdr[SAVE_HDR_LEN - 1] : DIFF_NORMAL;
}

bool storage_load(Game *g, bool *pending_night) {
  if (!storage_exists()) return false;
  uint8_t buf[SAVE_LEN];
  int len = persist_read_data(KEY_SAVE, buf, sizeof(buf));
  if (len != persist_get_size(KEY_SAVE)) return false;
  return save_read_blob(g, pending_night, buf, (size_t)len);
}

void storage_clear(void) {
  persist_delete(KEY_SAVE);
  if (persist_exists(KEY_OLD_HDR)) persist_delete(KEY_OLD_HDR);
  if (persist_exists(KEY_OLD_TOWERS)) persist_delete(KEY_OLD_TOWERS);
}

static void prv_read_bests(Best out[DIFF_COUNT]) {
  memset(out, 0, sizeof(Best) * DIFF_COUNT);
  if (persist_get_size(KEY_BESTS) == (int)(sizeof(Best) * DIFF_COUNT)) {
    persist_read_data(KEY_BESTS, out, sizeof(Best) * DIFF_COUNT);
  } else if (persist_get_size(KEY_OLD_BEST) == (int)sizeof(Best)) {
    persist_read_data(KEY_OLD_BEST, &out[DIFF_NORMAL], sizeof(Best));
  }
}

Best storage_best(Difficulty d) {
  Best all[DIFF_COUNT];
  prv_read_bests(all);
  return all[d < DIFF_COUNT ? d : DIFF_NORMAL];
}

void storage_set_best(Difficulty d, Best b) {
  if (d >= DIFF_COUNT) return;
  Best all[DIFF_COUNT];
  prv_read_bests(all);
  all[d] = b;
  int written = persist_write_data(KEY_BESTS, all, sizeof(all));
  if (written != (int)sizeof(all)) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "best write failed: %d", written);
    return;
  }
  if (persist_exists(KEY_OLD_BEST)) persist_delete(KEY_OLD_BEST);
}
