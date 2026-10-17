#include <string.h>
#include "test.h"
#include "config.h"
#include "economy.h"
#include "sim.h"
#include "save.h"

static void test_sizes(void) {
  CHECK(SAVE_HDR_LEN <= 256 && SAVE_TOWERS_LEN <= 256);
  CHECK_EQ(SAVE_TOWERS_LEN, 1 + 5 * MAX_TOWERS);
}

static void setup(Game *g) {
  game_new(g, 7);
  g->money = 1000;
  tower_buy(g, TW_MG, 4, 8);
  tower_buy(g, TW_GEPARD, 7, 8);
  tower_buy(g, TW_TOR, 10, 8);
  g->towers[1].hp -= 3;
  g->towers[2].ammo -= 1;
  g->objects[OBJ_TANK_A].hp = 40;
  g->objects[OBJ_DISTILLATION].status = OBJ_DESTROYED;
  g->objects[OBJ_DISTILLATION].hp = 0;
  g->night = 5; g->score = 4321; g->money = -17;
}

static void test_roundtrip(void) {
  Game a, b; memset(&b, 0, sizeof b); setup(&a);
  uint8_t h[SAVE_HDR_LEN], t[SAVE_TOWERS_LEN];
  CHECK_EQ(save_write_header(&a, true, h, sizeof h), SAVE_HDR_LEN);
  CHECK_EQ(save_write_towers(&a, t, sizeof t), SAVE_TOWERS_LEN);
  CHECK_EQ(h[0], SAVE_VERSION);
  CHECK_EQ(h[2], 5); CHECK_EQ(h[3], 0);
  memset(&b, 0xAA, sizeof b);
  bool pending = false;
  CHECK(save_read(&b, &pending, h, sizeof h, t, sizeof t));
  CHECK(pending);
  CHECK(b.phase == PHASE_DAY);
  CHECK_EQ(b.night, 5); CHECK_EQ(b.money, -17); CHECK_EQ(b.score, 4321);
  CHECK_EQ(b.seed, a.seed); CHECK_EQ(b.rng, a.rng);
  for (int i = 0; i < OBJ_COUNT; i++) {
    CHECK_EQ(b.objects[i].status, a.objects[i].status);
    CHECK_EQ(b.objects[i].hp, a.objects[i].hp);
  }
  for (int i = 0; i < MAX_TOWERS; i++) {
    CHECK_EQ(b.towers[i].active, a.towers[i].active);
    if (!a.towers[i].active) continue;
    CHECK_EQ(b.towers[i].type, a.towers[i].type);
    CHECK_EQ(b.towers[i].col, a.towers[i].col);
    CHECK_EQ(b.towers[i].row, a.towers[i].row);
    CHECK_EQ(b.towers[i].hp, a.towers[i].hp);
    CHECK_EQ(b.towers[i].ammo, a.towers[i].ammo);
    CHECK_EQ(b.towers[i].cooldown_ms, 0);
  }
  CHECK_EQ(b.wave_len, 0);
  CHECK(!b.drones[0].active);
  /* pending=false is preserved */
  save_write_header(&a, false, h, sizeof h);
  CHECK(save_read(&b, &pending, h, sizeof h, t, sizeof t));
  CHECK(!pending);
  CHECK(save_read(&b, NULL, h, sizeof h, t, sizeof t));
}

static void test_blob(void) {
  CHECK_EQ(SAVE_LEN, SAVE_HDR_LEN + SAVE_TOWERS_LEN);
  CHECK(SAVE_LEN <= 256);
  static Game a, b;
  setup(&a);
  uint8_t buf[SAVE_LEN + 4];
  CHECK_EQ(save_write(&a, true, buf, SAVE_LEN - 1), 0);    /* too small */
  CHECK_EQ(save_write(&a, true, buf, sizeof buf), SAVE_LEN);
  uint8_t h[SAVE_HDR_LEN], t[SAVE_TOWERS_LEN];               /* layout: header then towers */
  save_write_header(&a, true, h, sizeof h);
  save_write_towers(&a, t, sizeof t);
  CHECK(memcmp(buf, h, SAVE_HDR_LEN) == 0);
  CHECK(memcmp(buf + SAVE_HDR_LEN, t, SAVE_TOWERS_LEN) == 0);
  memset(&b, 0xAA, sizeof b);
  bool pending = false;
  CHECK(save_read_blob(&b, &pending, buf, SAVE_LEN));
  CHECK(pending);
  CHECK(b.phase == PHASE_DAY);
  CHECK_EQ(b.night, 5); CHECK_EQ(b.money, -17); CHECK_EQ(b.score, 4321);
  CHECK_EQ(b.seed, a.seed); CHECK_EQ(b.rng, a.rng);
  for (int i = 0; i < OBJ_COUNT; i++) {
    CHECK_EQ(b.objects[i].status, a.objects[i].status);
    CHECK_EQ(b.objects[i].hp, a.objects[i].hp);
  }
  for (int i = 0; i < MAX_TOWERS; i++) {
    CHECK_EQ(b.towers[i].active, a.towers[i].active);
    if (!a.towers[i].active) continue;
    CHECK_EQ(b.towers[i].type, a.towers[i].type);
    CHECK_EQ(b.towers[i].col, a.towers[i].col);
    CHECK_EQ(b.towers[i].row, a.towers[i].row);
    CHECK_EQ(b.towers[i].hp, a.towers[i].hp);
    CHECK_EQ(b.towers[i].ammo, a.towers[i].ammo);
  }
  CHECK(!save_read_blob(&b, &pending, buf, SAVE_LEN - 1));  /* wrong length */
  CHECK(!save_read_blob(&b, &pending, NULL, SAVE_LEN));
  buf[0] = SAVE_VERSION + 1;
  CHECK(!save_read_blob(&b, &pending, buf, SAVE_LEN));      /* validation still applies */
}

/* A save taken at REPORT (before the report is dismissed) loads as the next DAY, and the
   save written after report_ack is byte-identical: no double income on Continue. */
static void test_save_at_report(void) {
  static Game g, b;
  game_new(&g, 3);
  CHECK(night_begin(&g));
  for (int i = 0; i < 20000 && g.phase == PHASE_NIGHT; i++) sim_tick(&g, 40);
  CHECK(g.phase == PHASE_REPORT);
  uint8_t at_report[SAVE_LEN], after_ack[SAVE_LEN];
  CHECK_EQ(save_write(&g, false, at_report, sizeof at_report), SAVE_LEN);
  int32_t money = g.money;
  report_ack(&g);
  CHECK_EQ(g.money, money);
  CHECK_EQ(save_write(&g, false, after_ack, sizeof after_ack), SAVE_LEN);
  CHECK(memcmp(at_report, after_ack, SAVE_LEN) == 0);
  bool pending = true;
  CHECK(save_read_blob(&b, &pending, at_report, SAVE_LEN));
  CHECK(!pending);
  CHECK(b.phase == PHASE_DAY);
  CHECK_EQ(b.night, 2);
  CHECK_EQ(b.money, money);
}

static void test_wave_identical(void) {
  Game a, b; memset(&b, 0, sizeof b); game_new(&a, 11);
  a.night = 3; a.objects[OBJ_TERMINAL].status = OBJ_ALIVE; a.objects[OBJ_TERMINAL].hp = 100;
  uint8_t h[SAVE_HDR_LEN], t[SAVE_TOWERS_LEN];
  save_write_header(&a, true, h, sizeof h);
  save_write_towers(&a, t, sizeof t);
  bool p;
  CHECK(save_read(&b, &p, h, sizeof h, t, sizeof t));
  CHECK(night_begin(&a)); CHECK(night_begin(&b));
  CHECK(a.wave_len > 0);
  CHECK_EQ(a.wave_len, b.wave_len);
  for (int i = 0; i < a.wave_len && i < b.wave_len; i++) {
    CHECK_EQ(a.wave[i].time_ms, b.wave[i].time_ms);
    CHECK_EQ(a.wave[i].type, b.wave[i].type);
    CHECK_EQ(a.wave[i].side, b.wave[i].side);
  }
}

static void expect_reject(const uint8_t *h, size_t hl, const uint8_t *t, size_t tl) {
  Game g, copy; setup(&g); copy = g;
  bool p = true;
  CHECK(!save_read(&g, &p, h, hl, t, tl));
  CHECK(memcmp(&g, &copy, sizeof g) == 0);
}

static void test_rejects(void) {
  Game a; setup(&a);
  uint8_t h[SAVE_HDR_LEN], t[SAVE_TOWERS_LEN], x[SAVE_HDR_LEN], y[SAVE_TOWERS_LEN];
  save_write_header(&a, false, h, sizeof h);
  save_write_towers(&a, t, sizeof t);
  CHECK_EQ(save_write_header(&a, false, h, SAVE_HDR_LEN - 1), 0);
  CHECK_EQ(save_write_towers(&a, t, SAVE_TOWERS_LEN - 1), 0);

  memcpy(x, h, sizeof h); x[0] = 99; expect_reject(x, sizeof x, t, sizeof t);
  expect_reject(h, sizeof h - 1, t, sizeof t);
  expect_reject(h, sizeof h, t, sizeof t - 1);
  memcpy(x, h, sizeof h); x[1] = 2; expect_reject(x, sizeof x, t, sizeof t);
  memcpy(x, h, sizeof h); x[2] = 0; x[3] = 0; expect_reject(x, sizeof x, t, sizeof t);
  memcpy(x, h, sizeof h); x[20] = 3; expect_reject(x, sizeof x, t, sizeof t);
  memcpy(x, h, sizeof h); x[23] = 101; expect_reject(x, sizeof x, t, sizeof t);
  memcpy(x, h, sizeof h); x[22] = OBJ_ALIVE; x[23] = 0; expect_reject(x, sizeof x, t, sizeof t);

  memcpy(y, t, sizeof t); y[1] = 99; expect_reject(h, sizeof h, y, sizeof y);
  memcpy(y, t, sizeof t); y[2] = GRID_COLS; expect_reject(h, sizeof h, y, sizeof y);
  memcpy(y, t, sizeof t); y[3] = GRID_ROWS; expect_reject(h, sizeof h, y, sizeof y);
  memcpy(y, t, sizeof t); y[0] = MAX_TOWERS + 1; expect_reject(h, sizeof h, y, sizeof y);
  memcpy(y, t, sizeof t); y[4] = 255; expect_reject(h, sizeof h, y, sizeof y);
  memcpy(y, t, sizeof t); y[5] = 255; expect_reject(h, sizeof h, y, sizeof y);
  /* duplicate cell */
  memcpy(y, t, sizeof t); y[1 + 5 + 1] = y[2]; y[1 + 5 + 2] = y[3];
  expect_reject(h, sizeof h, y, sizeof y);
  /* tower on object cell */
  memcpy(y, t, sizeof t);
  y[2] = OBJECT_SPECS[OBJ_DISTILLATION].col; y[3] = OBJECT_SPECS[OBJ_DISTILLATION].row;
  expect_reject(h, sizeof h, y, sizeof y);
}

static void test_best(void) {
  Best b = {0, 0};
  CHECK(best_update(&b, 3, 500)); CHECK(!best_update(&b, 3, 400));
  CHECK(best_update(&b, 3, 600)); CHECK(!best_update(&b, 2, 9999));
  CHECK_EQ(b.nights, 3); CHECK_EQ(b.score, 600);
}

int main(void) {
  RUN(test_sizes); RUN(test_roundtrip); RUN(test_blob); RUN(test_save_at_report); RUN(test_wave_identical);
  RUN(test_rejects); RUN(test_best);
  return TEST_SUMMARY();
}
