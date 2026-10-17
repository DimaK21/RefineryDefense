#include "storage.h"

#define KEY_HDR 1
#define KEY_TOWERS 2
#define KEY_BEST 3

void storage_save(const Game *g, bool pending_night) {
  uint8_t hdr[SAVE_HDR_LEN];
  uint8_t tw[SAVE_TOWERS_LEN];
  if (!save_write_header(g, pending_night, hdr, sizeof(hdr))) return;
  if (!save_write_towers(g, tw, sizeof(tw))) return;
  persist_write_data(KEY_HDR, hdr, sizeof(hdr));
  persist_write_data(KEY_TOWERS, tw, sizeof(tw));
}

bool storage_exists(void) {
  return persist_exists(KEY_HDR) && persist_exists(KEY_TOWERS) &&
         persist_get_size(KEY_HDR) == SAVE_HDR_LEN &&
         persist_get_size(KEY_TOWERS) == SAVE_TOWERS_LEN;
}

bool storage_load(Game *g, bool *pending_night) {
  if (!storage_exists()) return false;
  uint8_t hdr[SAVE_HDR_LEN];
  uint8_t tw[SAVE_TOWERS_LEN];
  if (persist_read_data(KEY_HDR, hdr, sizeof(hdr)) != SAVE_HDR_LEN) return false;
  if (persist_read_data(KEY_TOWERS, tw, sizeof(tw)) != SAVE_TOWERS_LEN) return false;
  return save_read(g, pending_night, hdr, sizeof(hdr), tw, sizeof(tw));
}

void storage_clear(void) {
  persist_delete(KEY_HDR);
  persist_delete(KEY_TOWERS);
}

Best storage_best(void) {
  Best b = {0, 0};
  if (persist_get_size(KEY_BEST) == (int)sizeof(Best)) {
    persist_read_data(KEY_BEST, &b, sizeof(b));
  }
  return b;
}

void storage_set_best(Best b) { persist_write_data(KEY_BEST, &b, sizeof(b)); }
