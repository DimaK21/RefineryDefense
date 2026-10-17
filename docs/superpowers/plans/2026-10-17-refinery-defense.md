# Refinery Defense Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Tower-defense игра для Pebble Time 2: днём расставляешь ПВО вокруг НПЗ, ночью смотришь, как оно отбивает налёт дронов; бесконечно, на рекорд.

**Architecture:** Вся игровая логика (состояние, экономика, волны, симуляция ночи, сохранение, жесты, камера) — чистый C без `pebble.h` в `src/c/logic/`, тестируется на хосте через `clang` + `make`. Слой Pebble в `src/c/app/` — окна, рендер, тач, вибро/звук, persist — только вызывает логику и рисует `Game`. Координаты — фиксированная точка Q8 (1 мировая единица = 256), детерминированный ГПСЧ, всё в пулах фиксированного размера.

**Tech Stack:** C99, Pebble SDK 4.33.1 (`pebble build`, эмулятор `emery`), хостовые тесты: `clang` + `make` + свой минимальный `test.h`.

**Spec:** `docs/superpowers/specs/2026-10-17-drone-defence-design.md`

## Global Constraints

- Платформа v1: только `emery` (Pebble Time 2, 200×228, 64 цвета, тач, динамик). `targetPlatforms: ["emery"]`.
- Логика в `src/c/logic/` не включает `pebble.h`, не использует `malloc` и `float`.
- Весь текст в UI — на английском.
- Названия: MG Nest, Gepard, Tor, Patriot, EW Jammer, Radar; Switchblade, Shahed-136, Gerbera, Harpy, Shahed-238; Distillation Column, Tank Farm A, Tank Farm B, Loading Terminal, Cracking Unit; Air Support.
- Мир 200×228 единиц; клетка 12; сетка 16×19; днём зум ×2, ночью ×1.
- Persist: не больше 256 байт на ключ (`PERSIST_DATA_MAX_LENGTH`).
- Все игровые числа — только в `src/c/logic/config.c`/`config.h`.
- Коммиты: одна строка ≤ 80 символов, без тела и без `Co-Authored-By` (глобальное правило пользователя).

## Review Focus

1. Двойной тап по «▶ Night» / повторный вызов старта ночи → ночь стартует один раз (`night_begin` возвращает false не в фазе DAY) — тест в Task 7.
2. Поздние ночи (N = 40), волна больше пула дронов → лишние спавны откладываются, не теряются и не портят память — тест в Task 5.
3. Денег меньше стоимости (покупка, ремонт, перезарядка, постройка, Air Support) → действие отклоняется целиком, деньги не уходят в минус — тесты в Task 3 и Task 7.
4. Повреждённое/старое/отсутствующее сохранение → `save_read` возвращает false и не трогает `Game`, меню предлагает только New Game — тест в Task 8.
5. Выход посреди ночи → после Continue та же ночь с тем же налётом (тот же seed) — тест в Task 8.

---

## File Structure

```
package.json, wscript                 — проект Pebble (из pebble new-project)
src/c/logic/types.h                   — константы мира, enum'ы, структуры Game
src/c/logic/fx.h, fx.c                — Q8-математика: sin-таблица, isqrt, dist
src/c/logic/rng.h, rng.c              — xorshift32
src/c/logic/config.h, config.c        — таблицы TowerSpec/DroneSpec/ObjectSpec и константы баланса
src/c/logic/economy.h, economy.c      — game_new, покупка/продажа/ремонт/постройка, доход
src/c/logic/waves.h, waves.c          — генерация волны ночи N
src/c/logic/drones.h, drones.c        — спавн, движение, виляние, РЭБ, попадание в цель
src/c/logic/combat.h, combat.c        — радар/видимость, наведение, стрельба, ракеты, эффекты
src/c/logic/sim.h, sim.c              — жизненный цикл ночи, тик, Air Support, события
src/c/logic/save.h, save.c            — сериализация, рекорд
src/c/logic/gesture.h, gesture.c      — тап/пан из сырого тача
src/c/logic/camera.h, camera.c        — зум, скролл, экран↔мир↔клетка
src/c/app/main.c                      — init/deinit, глобальный Game, навигация
src/c/app/storage.h, storage.c        — persist-обёртка над save.*
src/c/app/render.h, render.c          — отрисовка карты/объектов/сущностей через Camera
src/c/app/feedback.h, feedback.c      — вибро и звук
src/c/app/menu_window.*, day_window.*, night_window.*, report_window.*, gameover_window.*, touch_off_window.*
tests/test.h, tests/Makefile, tests/test_*.c
```

---

### Task 1: Каркас проекта, хостовые тесты, fx и rng

**Files:**
- Create: `package.json`, `wscript`, `src/c/app/main.c` (из `pebble new-project`), `src/c/logic/fx.{h,c}`, `src/c/logic/rng.{h,c}`, `tests/test.h`, `tests/Makefile`, `tests/test_fx_rng.c`

**Interfaces:**
- Produces:
  - `#define FX 256`; `int32_t fx_sin(uint16_t angle)` — угол 0..65535 = полный круг, результат в Q8 (−256..256); `int32_t fx_cos(uint16_t angle)`
  - `uint32_t isqrt32(uint32_t v)`; `int32_t fx_dist(int32_t dx, int32_t dy)` — Q8 вход, Q8 выход (через 64-битный квадрат)
  - `uint32_t rng_next(uint32_t *state)` (xorshift32, state≠0); `int32_t rng_range(uint32_t *state, int32_t lo, int32_t hi)` — включительно
  - `tests/test.h`: `CHECK(cond)`, `CHECK_EQ(a,b)`, `RUN(test_fn)`, итог «N passed, M failed», ненулевой exit при провале
  - `tests/Makefile`: цель `test` собирает каждый `test_*.c` в отдельный бинарник вместе со всеми `../src/c/logic/*.c` (`-std=c99 -Wall -Werror -I../src/c/logic`) и запускает все по очереди; провал любого → ненулевой exit

- [ ] **Step 1:** Сгенерировать проект: `cd <scratchpad> && pebble new-project --c refinery_defense`, перенести `package.json`, `wscript`, `src/` в корень репозитория; в `package.json`: `"displayName": "Refinery Defense"`, `"targetPlatforms": ["emery"]`, `"watchapp": {"watchface": false}`; переместить `src/c/refinery_defense.c` в `src/c/app/main.c` (wscript по умолчанию собирает `src/c/**/*.c`).
- [ ] **Step 2: Failing tests** в `tests/test_fx_rng.c`:

```c
CHECK_EQ(fx_sin(0), 0);
CHECK_EQ(fx_sin(16384), 256);
CHECK_EQ(fx_sin(49152), -256);
CHECK_EQ(fx_cos(0), 256);
CHECK_EQ(isqrt32(144), 12);
CHECK_EQ(isqrt32(145), 12);
CHECK_EQ(fx_dist(3*FX, 4*FX), 5*FX);
uint32_t a = 42, b = 42;
for (int i = 0; i < 100; i++) CHECK_EQ(rng_next(&a), rng_next(&b));
uint32_t s = 7; for (int i = 0; i < 1000; i++) { int32_t v = rng_range(&s, -3, 3); CHECK(v >= -3 && v <= 3); }
```

- [ ] **Step 3:** `make -C tests test` → FAIL (не компилируется).
- [ ] **Step 4:** Реализовать fx (таблица четверти периода, 65 значений Q8, симметрия для остальных) и rng.
- [ ] **Step 5:** `make -C tests test` → `passed`, exit 0. `pebble build` → успешная сборка `build/*.pbw`.
- [ ] **Step 6:** Добавить в `.gitignore` строку `build/`. Commit: `Scaffold Pebble project with host test harness`

---

### Task 2: Типы, конфиг баланса, новая игра

**Files:**
- Create: `src/c/logic/types.h`, `src/c/logic/config.{h,c}`, `src/c/logic/economy.{h,c}` (пока только `game_new`, `alive_objects`), `tests/test_config.c`

**Interfaces:**
- Produces (`types.h`):

```c
#define WORLD_W 200
#define WORLD_H 228
#define CELL 12
#define GRID_COLS 16
#define GRID_ROWS 19
#define MAX_TOWERS 40
#define MAX_DRONES 64
#define MAX_MISSILES 24
#define MAX_EFFECTS 24
#define MAX_WAVE 200
#define OBJ_COUNT 5
typedef enum { TW_MG, TW_GEPARD, TW_TOR, TW_PATRIOT, TW_JAMMER, TW_RADAR, TW_COUNT } TowerType;
typedef enum { DR_SWITCHBLADE, DR_SHAHED136, DR_GERBERA, DR_HARPY, DR_SHAHED238, DR_COUNT } DroneType;
typedef enum { OBJ_DISTILLATION, OBJ_TANK_A, OBJ_TANK_B, OBJ_TERMINAL, OBJ_CRACKER } ObjectId;
typedef enum { OBJ_NOT_BUILT, OBJ_ALIVE, OBJ_DESTROYED } ObjectStatus;
typedef enum { PHASE_DAY, PHASE_NIGHT, PHASE_REPORT, PHASE_GAME_OVER } Phase;
typedef enum { TGT_OBJECT, TGT_TOWER } TargetKind;
typedef enum { FX_EXPLOSION, FX_TRACER, FX_AIRSTRIKE } EffectKind;
typedef struct { int32_t x, y; } Vec;                          // Q8 world units
typedef struct { uint8_t active, type, col, row, hp, ammo; int16_t cooldown_ms; } Tower;
typedef struct { uint8_t active, type, hp, target_kind, target_idx, wp_idx, amp;
                 uint16_t uid, phase; Vec track, pos; Vec wps[3]; } Drone;
typedef struct { uint8_t active, tower_type, drone_idx; uint16_t drone_uid; Vec pos; } Missile;
typedef struct { uint8_t active, kind; int16_t ttl_ms; Vec a, b; } Effect;
typedef struct { uint8_t status, hp; } RefObject;
typedef struct { uint16_t time_ms; uint8_t type, side; } WaveEntry;   // side: 0 top,1 right,2 bottom,3 left
typedef struct { uint16_t kills; int32_t bounty, income; uint8_t objects_hit, towers_lost; } NightStats;
typedef struct {
  Phase phase; uint16_t night; int32_t money; uint32_t score, seed, rng;
  RefObject objects[OBJ_COUNT];
  Tower towers[MAX_TOWERS]; Drone drones[MAX_DRONES];
  Missile missiles[MAX_MISSILES]; Effect effects[MAX_EFFECTS];
  WaveEntry wave[MAX_WAVE]; uint16_t wave_len, wave_next, next_uid; uint32_t night_ms;
  uint8_t air_used, air_active; Vec air_point; int16_t air_timer_ms;
  NightStats stats; uint32_t events;
} Game;
```

- Produces (`config.h`): `TowerSpec {const char *name; uint16_t cost; uint8_t range; uint16_t period_ms; uint8_t damage, ammo /*0=∞*/, hp, ammo_price;}`, `DroneSpec {const char *name; uint8_t speed, hp, damage, tower_damage, bounty, first_night, threat;}`, `ObjectSpec {const char *name; uint8_t income; uint16_t build_cost; uint8_t col, row;}` (объект занимает 2×2 клетки от `col,row`); `extern const TowerSpec TOWER_SPECS[TW_COUNT]; extern const DroneSpec DRONE_SPECS[DR_COUNT]; extern const ObjectSpec OBJECT_SPECS[OBJ_COUNT];` и константы:
  `START_MONEY 150, SELL_REFUND_PCT 50, TOWER_REPAIR_PCT 30, REBUILD_PCT 150, DETECT_PCT 60, JAMMER_SLOW_PCT 40, AIR_COST 60, AIR_DELAY_MS 1500, AIR_RADIUS 25, MISSILE_SPEED 60, SPAWN_WINDOW_MS 40000, SWITCHBLADE_TOWER_PCT 20, HIT_RADIUS 4, WAVE_BASE 20, WAVE_PER_NIGHT 12, WAVE_EXTRA_OBJ_PCT 10, EFFECT_MS 400, TRACER_MS 120`
- Значения таблиц — из спеки (раздел «Установки», «Дроны», «Завод»). Дополнительно: `tower_damage` = Switchblade 1, Harpy 4, остальные 0; Radar: `range 70, period 0, damage 0`; Jammer: `range 36, period 0, damage 0`. Позиции объектов (col,row): Distillation (7,8), Tank A (3,12), Tank B (11,4), Terminal (11,13), Cracker (3,4).
- Produces (`economy.h`): `void game_new(Game *g, uint32_t seed)`; `int alive_objects(const Game *g)`

- [ ] **Step 1: Failing tests** `tests/test_config.c`:

```c
CHECK_EQ(TOWER_SPECS[TW_PATRIOT].cost, 250); CHECK_EQ(TOWER_SPECS[TW_PATRIOT].ammo, 3);
CHECK_EQ(DRONE_SPECS[DR_SHAHED238].first_night, 6); CHECK_EQ(DRONE_SPECS[DR_GERBERA].damage, 0);
CHECK_EQ(OBJECT_SPECS[OBJ_CRACKER].income, 40);
// объекты не пересекаются и влезают в сетку
for each pair i<j: CHECK(2x2 rects of OBJECT_SPECS[i], [j] do not overlap);
for each i: CHECK(col+1 < GRID_COLS && row+1 < GRID_ROWS);
Game g; game_new(&g, 1);
CHECK_EQ(g.phase, PHASE_DAY); CHECK_EQ(g.night, 1); CHECK_EQ(g.money, 150);
CHECK_EQ(g.objects[OBJ_DISTILLATION].status, OBJ_ALIVE); CHECK_EQ(g.objects[OBJ_DISTILLATION].hp, 100);
CHECK_EQ(g.objects[OBJ_TANK_A].status, OBJ_ALIVE);
CHECK_EQ(g.objects[OBJ_CRACKER].status, OBJ_NOT_BUILT);
CHECK_EQ(alive_objects(&g), 2);
```

- [ ] **Step 2:** `make -C tests test` → FAIL.
- [ ] **Step 3:** Реализовать `types.h`, `config.c`, `game_new` (обнуляет всё через `memset`, `rng = seed ? seed : 1`, `seed = rng_next(&rng)`), `alive_objects`.
- [ ] **Step 4:** `make -C tests test` → PASS; `pebble build` → OK.
- [ ] **Step 5:** Commit: `Add game state types and balance config`

---

### Task 3: Экономика дня

**Files:**
- Modify: `src/c/logic/economy.{h,c}`
- Test: `tests/test_economy.c`

**Interfaces:**
- Consumes: `Game`, `*_SPECS`, константы из Task 2.
- Produces:

```c
typedef enum { ACT_OK, ACT_NO_MONEY, ACT_BLOCKED, ACT_INVALID } ActResult;
int  tower_at(const Game *g, int col, int row);      // индекс или -1
int  object_at(const Game *g, int col, int row);     // ObjectId или -1 (любой статус)
bool cell_free(const Game *g, int col, int row);     // в сетке, не объект, не установка
ActResult tower_buy(Game *g, TowerType t, int col, int row);   // полный HP и боезапас
ActResult tower_move(Game *g, int idx, int col, int row);      // бесплатно
ActResult tower_sell(Game *g, int idx);                        // +cost*50/100
int32_t   tower_repair_cost(const Game *g, int idx);  // ceil(cost*30/100 * missing_hp / max_hp)
ActResult tower_repair(Game *g, int idx);
int32_t   tower_reload_cost(const Game *g, int idx);  // missing_ammo * ammo_price; 0 для ∞
ActResult tower_reload(Game *g, int idx);
int32_t   object_action_cost(const Game *g, ObjectId o); // NOT_BUILT: build_cost; ALIVE: build_cost*(100-hp)/100; DESTROYED: build_cost*150/100
ActResult object_action(Game *g, ObjectId o);            // build/repair/rebuild → ALIVE, hp 100
int32_t   morning_income(const Game *g);                 // Σ income*hp/100 по ALIVE
```

- Правила: вне `PHASE_DAY` любое действие → `ACT_INVALID`; занятая/вне сетки клетка → `ACT_BLOCKED`; денег < стоимости → `ACT_NO_MONEY` и состояние не меняется; нулевая стоимость ремонта/перезарядки/действия → `ACT_INVALID`; пул установок полон → `ACT_BLOCKED`.

- [ ] **Step 1: Failing tests** `tests/test_economy.c`:

```c
Game g; game_new(&g, 1);
CHECK_EQ(tower_buy(&g, TW_MG, 0, 0), ACT_OK); CHECK_EQ(g.money, 120);
CHECK_EQ(tower_buy(&g, TW_MG, 0, 0), ACT_BLOCKED);
CHECK_EQ(tower_buy(&g, TW_MG, 7, 8), ACT_BLOCKED);           // клетка Distillation
CHECK_EQ(tower_buy(&g, TW_MG, 16, 0), ACT_BLOCKED);          // вне сетки
CHECK_EQ(tower_buy(&g, TW_PATRIOT, 1, 1), ACT_NO_MONEY); CHECK_EQ(g.money, 120);
int t = tower_at(&g, 0, 0);
CHECK_EQ(tower_move(&g, t, 2, 2), ACT_OK); CHECK_EQ(tower_at(&g, 2, 2), t); CHECK_EQ(g.money, 120);
g.towers[t].hp = 1;                                          // MG: hp 3, cost 30
CHECK_EQ(tower_repair_cost(&g, t), 6);                       // ceil(9 * 2/3)
CHECK_EQ(tower_sell(&g, t), ACT_OK); CHECK_EQ(g.money, 135); CHECK_EQ(tower_at(&g, 2, 2), -1);
g.money = 1000; tower_buy(&g, TW_TOR, 5, 5); int tor = tower_at(&g, 5, 5);
CHECK_EQ(tower_reload_cost(&g, tor), 0); CHECK_EQ(tower_reload(&g, tor), ACT_INVALID);
g.towers[tor].ammo = 2; CHECK_EQ(tower_reload_cost(&g, tor), 20);
g.objects[OBJ_TANK_A].hp = 40; CHECK_EQ(object_action_cost(&g, OBJ_TANK_A), 72);
g.objects[OBJ_TANK_B].status = OBJ_DESTROYED; CHECK_EQ(object_action_cost(&g, OBJ_TANK_B), 180);
CHECK_EQ(object_action_cost(&g, OBJ_CRACKER), 250);
CHECK_EQ(morning_income(&g), 30 + 8);                        // Distillation 100% + Tank A 40%
g.money = 10; CHECK_EQ(object_action(&g, OBJ_CRACKER), ACT_NO_MONEY); CHECK_EQ(g.money, 10);
g.phase = PHASE_NIGHT; CHECK_EQ(tower_buy(&g, TW_MG, 9, 0), ACT_INVALID);
```

- [ ] **Step 2:** `make -C tests test` → FAIL.
- [ ] **Step 3:** Реализовать функции из Interfaces.
- [ ] **Step 4:** `make -C tests test` → PASS.
- [ ] **Step 5:** Commit: `Add day economy actions`

---

### Task 4: Генерация волн

**Files:**
- Create: `src/c/logic/waves.{h,c}`; Test: `tests/test_waves.c`

**Interfaces:**
- Produces: `int32_t wave_budget(uint16_t night, int alive_objects)` = `(20 + 12*night) * (100 + 10*max(0, alive-2)) / 100`; `uint16_t wave_generate(WaveEntry *out, uint16_t max, uint16_t night, int alive_objects, uint32_t seed)` — записи отсортированы по `time_ms`, все `time_ms < SPAWN_WINDOW_MS`.
- Алгоритм: пока остаток бюджета ≥ 1 и есть место — случайный тип среди разблокированных (`first_night ≤ night`) с `threat ≤ остатка`; Switchblade добавляется стаей 3–5 (каждый тратит свой threat, одна сторона, интервал 300 мс). Стороны: из случайно выбранного набора — ночи 1–2: 1 сторона, 3–5: 2 разные, 6+: все 4. Затем сортировка вставками по времени.

- [ ] **Step 1: Failing tests:**

```c
CHECK_EQ(wave_budget(1, 2), 32); CHECK_EQ(wave_budget(1, 4), 38); CHECK_EQ(wave_budget(10, 1), 140);
WaveEntry w[MAX_WAVE], w2[MAX_WAVE];
uint16_t n = wave_generate(w, MAX_WAVE, 1, 2, 123);
CHECK(n > 0); int sum = 0; for i<n: { CHECK_EQ(w[i].type, DR_SWITCHBLADE); sum += threat(w[i]); CHECK(w[i].side == w[0].side); }
CHECK(sum <= 32);
for i>0: CHECK(w[i-1].time_ms <= w[i].time_ms);
CHECK_EQ(wave_generate(w2, MAX_WAVE, 1, 2, 123), n); CHECK(memcmp(w, w2, n*sizeof *w) == 0);   // детерминизм
n = wave_generate(w, MAX_WAVE, 3, 2, 9); for i<n: CHECK(w[i].type != DR_HARPY && w[i].type != DR_SHAHED238);
n = wave_generate(w, MAX_WAVE, 8, 2, 9); // все 4 стороны встречаются
n = wave_generate(w, MAX_WAVE, 200, 5, 9); CHECK(n <= MAX_WAVE);   // огромный бюджет обрезается пулом
```

- [ ] **Step 2:** FAIL → **Step 3:** реализовать → **Step 4:** PASS.
- [ ] **Step 5:** Commit: `Add night wave generation`

---

### Task 5: Дроны — спавн, движение, удар по цели

**Files:**
- Create: `src/c/logic/drones.{h,c}`; Test: `tests/test_drones.c`

**Interfaces:**
- Consumes: `Game`, `fx_*`, `rng_*`, specs.
- Produces:

```c
bool drone_spawn(Game *g, const WaveEntry *e);     // false, если пул полон
void drones_update(Game *g, uint16_t dt_ms);       // движение + удары
bool drone_jammed(const Game *g, int d);           // в радиусе любого EW Jammer
Vec  tower_center(const Tower *t);  Vec object_center(ObjectId o);   // Q8
void drone_retarget(Game *g, int d);
#define EV_OBJECT_HIT 1u
#define EV_OBJECT_DESTROYED 2u
#define EV_MISSILE 4u
#define EV_EXPLOSION 8u
#define EV_TOWER_LOST 16u
```

- Спавн: точка на стороне `e->side` со случайной координатой вдоль края; `uid = ++next_uid`; HP из spec; цель: Harpy → ближайший Radar, иначе ближайшая установка, иначе случайный живой объект; Switchblade → с вероятностью 20% ближайшая установка (если есть), иначе объект; остальные → случайный живой объект. `wps[0]`, `wps[1]` — точки на 1/3 и 2/3 пути с перпендикулярным смещением `rng_range(-30, 30)`; `wps[2]` — центр цели. `amp = rng_range(3, 8)`, `phase` случайная.
- Движение: `track` идёт к `wps[wp_idx]` со скоростью spec (×(100−40)/100 и `amp`×2, если `drone_jammed`); `phase += dt_ms * 44` (период ≈1.5 с); `pos = track + perp(dir) * amp * fx_sin(phase) / FX`. В радиусе `HIT_RADIUS` от точки — `wp_idx++`; на `wps[2]` — удар: объект `hp -= damage` (до 0 → `OBJ_DESTROYED`, `EV_OBJECT_DESTROYED`, иначе `EV_OBJECT_HIT`, `stats.objects_hit++`); установка `hp -= tower_damage` (до 0 → `active=0`, `EV_TOWER_LOST`, `stats.towers_lost++`); эффект `FX_EXPLOSION`; дрон `active=0`. Gerbera просто исчезает.
- Если цель исчезла (установка снята/объект разрушен) — `drone_retarget` по тем же правилам, обновляет только `wps[2]` (и `wp_idx = 2`, если уже прошёл); нет живых объектов → дрон исчезает.

- [ ] **Step 1: Failing tests:**

```c
// попадание: Shahed-136 сверху по Distillation снимает 35 HP
Game g; game_new(&g, 5); WaveEntry e = {0, DR_SHAHED136, 0};
CHECK(drone_spawn(&g, &e));
for (int i = 0; i < 2000 && g.drones[0].active; i++) drones_update(&g, 40);
CHECK(!g.drones[0].active);
CHECK(g.objects[OBJ_DISTILLATION].hp == 65 || g.objects[OBJ_TANK_A].hp == 65);
CHECK(g.events & EV_OBJECT_HIT);
// виляние: pos отличается от track хотя бы в одном тике
// РЭБ: дрон в зоне Jammer проходит за 1 с ≈ 60% пути обычного (±1 единица)
// Harpy выбирает радар, а не ближайшую пушку
// пул: 64 успешных спавна, 65-й возвращает false и не портит соседние поля Game
for (int i = 0; i < MAX_DRONES; i++) CHECK(drone_spawn(&g2, &e)); CHECK(!drone_spawn(&g2, &e));
// ретаргет: цель-установку продали → wps[2] указывает на центр живого объекта
// установка с hp 1 после удара Switchblade исчезает, EV_TOWER_LOST
```

- [ ] **Step 2:** FAIL → **Step 3:** реализовать → **Step 4:** PASS.
- [ ] **Step 5:** Commit: `Add drone spawning and movement`

---

### Task 6: Бой — радар, видимость, наведение, ракеты

**Files:**
- Create: `src/c/logic/combat.{h,c}`; Test: `tests/test_combat.c`

**Interfaces:**
- Consumes: Task 5 (`tower_center`, `EV_*`), specs.
- Produces:

```c
bool    point_in_radar(const Game *g, Vec p);
int32_t tower_engage_range(const Game *g, int t);  // Q8: range*FX, если центр установки в зоне радара, иначе (range*60/100)*FX (целочисленно в единицах, потом в Q8)
bool    drone_visible(const Game *g, int d);        // в зоне радара или в engage-радиусе любой стреляющей установки
bool    drone_identified(const Game *g, int d);     // в зоне радара (Gerbera показывается как Gerbera)
void    combat_update(Game *g, uint16_t dt_ms);
void    effect_add(Game *g, EffectKind k, Vec a, Vec b, int16_t ttl_ms);  // пул полон → тихо пропустить
void    kill_drone(Game *g, int d);                  // active=0, money/score/stats += bounty, kills++, EV_EXPLOSION, FX_EXPLOSION
```

- Каждая стреляющая установка (MG, Gepard, Tor, Patriot): `cooldown_ms -= dt`; если ≤ 0 и есть боезапас (ammo spec 0 = бесконечно) — цель среди активных дронов в `tower_engage_range`; Tor/Patriot пропускают Gerbera, если `drone_identified`; выбирается дрон с минимальным оставшимся расстоянием `track → wps[2]`. MG/Gepard: мгновенный урон, `FX_TRACER`. Tor/Patriot: ракета из центра установки, `ammo--`, `EV_MISSILE`. `cooldown_ms = period_ms`.
- Ракеты летят `MISSILE_SPEED` к `pos` цели; если `drones[idx].uid != drone_uid` или неактивен — ракета исчезает; в `HIT_RADIUS` — урон spec установки. HP ≤ 0 → `kill_drone`.
- Эффекты: `ttl_ms -= dt`, ≤ 0 → неактивен (тик эффектов делает sim, Task 7).

- [ ] **Step 1: Failing tests:**

```c
// MG без радара: engage = 22*60/100 = 13 единиц; с радаром рядом — 22
CHECK_EQ(tower_engage_range(&g, mg), 13*FX);  /* добавили Radar рядом */ CHECK_EQ(tower_engage_range(&g, mg), 22*FX);
// MG убивает Switchblade (hp 2) в радиусе за 2 выстрела: после 250 мс + 1 тика деньги += 3
// дрон за пределами всех радиусов не виден; в зоне радара виден и опознан
// Tor под радаром не стреляет по Gerbera (ammo остаётся 6), без радара — стреляет
// Patriot: ammo 3 → после трёх пусков стрельба прекращается
// ракета к дрону, который умер раньше, исчезает без урона другому дрону, занявшему слот (другой uid)
// выбор цели: из двух дронов в радиусе стреляет в того, кто ближе к своей цели
```

- [ ] **Step 2:** FAIL → **Step 3:** реализовать → **Step 4:** PASS.
- [ ] **Step 5:** Commit: `Add tower combat, radar and missiles`

---

### Task 7: Жизненный цикл ночи и Air Support

**Files:**
- Create: `src/c/logic/sim.{h,c}`; Test: `tests/test_sim.c`

**Interfaces:**
- Consumes: Tasks 3–6.
- Produces:

```c
bool     night_begin(Game *g);         // только из DAY: wave_generate(seed, alive), очистка дронов/ракет/эффектов, stats=0, air_used=0, night_ms=0, phase=NIGHT
void     sim_tick(Game *g, uint16_t dt_ms);
bool     air_support_call(Game *g, Vec p);  // NIGHT, !air_used, money ≥ 60: списать, air_active, таймер 1500
uint32_t sim_take_events(Game *g);     // вернуть и обнулить events
uint16_t nights_survived(const Game *g);   // night - 1
void     report_ack(Game *g);          // REPORT → DAY
```

- `sim_tick` (только в NIGHT): `night_ms += dt`; спавнить записи `wave[wave_next]` с `time_ms ≤ night_ms`, пока `drone_spawn` успешен (неудача — оставить на следующий тик); `drones_update`; `combat_update`; Air Support: по истечении таймера убить (`kill_drone`) все активные дроны в радиусе 25 от точки, `FX_AIRSTRIKE`; тик эффектов; если `alive_objects == 0` → `PHASE_GAME_OVER`; иначе если волна исчерпана и нет активных дронов → конец ночи: `stats.income = morning_income`, `money += income`, `score += 100`, `night++`, `seed = rng_next(&rng)`, `phase = REPORT`.

- [ ] **Step 1: Failing tests:**

```c
Game g; game_new(&g, 3);
CHECK(night_begin(&g)); CHECK(!night_begin(&g));                 // двойной старт
CHECK(g.wave_len > 0);
// без ПВО ночь 1 доходит до REPORT или GAME_OVER за ≤ 120 с симуляции
for (int i = 0; i < 3000 && g.phase == PHASE_NIGHT; i++) sim_tick(&g, 40);
CHECK(g.phase == PHASE_REPORT || g.phase == PHASE_GAME_OVER);
// при REPORT: night == 2, money == 150 + stats.bounty + stats.income, score ≥ 100
// report_ack → DAY; повторный night_begin генерирует другую волну (seed сменился)
// Air Support: money 59 → false и деньги не меняются; 100 → true, money 40; второй вызов → false
// через 1500 мс все дроны в радиусе 25 от точки неактивны, вне радиуса — живы
// все объекты разрушены посреди ночи → GAME_OVER сразу
// спавн при полном пуле: wave_next не растёт, после освобождения слота запись спавнится
```

- [ ] **Step 2:** FAIL → **Step 3:** реализовать → **Step 4:** PASS.
- [ ] **Step 5:** Commit: `Add night lifecycle and air support`

---

### Task 8: Сохранение и рекорд

**Files:**
- Create: `src/c/logic/save.{h,c}`; Test: `tests/test_save.c`

**Interfaces:**
- Produces:

```c
#define SAVE_VERSION 1
#define SAVE_HDR_LEN 30     // version1 pending1 night2 money4 score4 seed4 rng4 + 5×(status1,hp1); little-endian
#define SAVE_TOWERS_LEN 201 // count1 + 40×(type,col,row,hp,ammo)
size_t save_write_header(const Game *g, bool pending_night, uint8_t *buf, size_t cap);
size_t save_write_towers(const Game *g, uint8_t *buf, size_t cap);
bool   save_read(Game *g, bool *pending_night, const uint8_t *hdr, size_t hdr_len, const uint8_t *tw, size_t tw_len);
typedef struct { uint16_t nights; uint32_t score; } Best;
bool   best_update(Best *b, uint16_t nights, uint32_t score);  // true если новый рекорд: больше ночей, при равенстве больше очков
```

- `save_read` проверяет версию, длины, диапазоны (`type < TW_COUNT`, `col < GRID_COLS`, `row < GRID_ROWS`, `status ≤ OBJ_DESTROYED`, `hp ≤ 100`, count ≤ MAX_TOWERS); при любой ошибке возвращает false и `Game` не трогает (разбирать во временный `Game`, затем копировать). Успех → `phase = PHASE_DAY`, пулы дронов/ракет/эффектов пусты.
- Обе длины ≤ 256 (`PERSIST_DATA_MAX_LENGTH`) — проверить статическим assert в тесте.

- [ ] **Step 1: Failing tests:**

```c
// круговой прогон: расставить 3 установки, повредить объект, записать, прочитать в новый Game → поля совпадают
// pending_night=true сохраняется; после чтения night_begin даёт волну, идентичную исходной (memcmp wave)
// испорченная версия / короткий буфер / type=99 → false, целевой Game не изменён (memcmp с копией)
CHECK(SAVE_HDR_LEN <= 256 && SAVE_TOWERS_LEN <= 256);
Best b = {0, 0}; CHECK(best_update(&b, 3, 500)); CHECK(!best_update(&b, 3, 400)); CHECK(best_update(&b, 3, 600)); CHECK(!best_update(&b, 2, 9999));
```

- [ ] **Step 2:** FAIL → **Step 3:** реализовать → **Step 4:** PASS.
- [ ] **Step 5:** Commit: `Add save serialization and best score`

---

### Task 9: Жесты и камера

**Files:**
- Create: `src/c/logic/gesture.{h,c}`, `src/c/logic/camera.{h,c}`; Test: `tests/test_input.c`

**Interfaces:**
- Produces:

```c
typedef enum { TOUCH_DOWN, TOUCH_UP, TOUCH_MOVE } TouchKind;   // мапится из TouchEvent_*
typedef enum { GES_NONE, GES_TAP, GES_PAN_START, GES_PAN_MOVE, GES_PAN_END } GestureKind;
typedef struct { GestureKind kind; int16_t x, y, dx, dy; } Gesture;   // dx,dy — смещение с прошлого события
typedef struct { bool down, panning; int16_t x0, y0, lx, ly; uint32_t t0; } GestureTracker;
#define TAP_SLOP 6
#define TAP_MAX_MS 500
Gesture gesture_feed(GestureTracker *t, TouchKind k, int16_t x, int16_t y, uint32_t now_ms);

typedef struct { int16_t x, y; uint8_t zoom; } Camera;   // x,y — мировая точка в левом верхнем углу экрана
void camera_clamp(Camera *c, int16_t screen_w, int16_t screen_h);    // 0..WORLD-screen/zoom
void camera_pan(Camera *c, int16_t dx_px, int16_t dy_px, int16_t screen_w, int16_t screen_h); // карта едет за пальцем
void camera_to_world(const Camera *c, int16_t sx, int16_t sy, int16_t *wx, int16_t *wy);
void camera_to_screen(const Camera *c, int16_t wx, int16_t wy, int16_t *sx, int16_t *sy);
bool world_to_cell(int16_t wx, int16_t wy, int *col, int *row);       // false вне сетки
```

- [ ] **Step 1: Failing tests:**

```c
// тап: DOWN(50,50,t0) → UP(52,51,t0+200) = GES_TAP в (52,51)
// длинное удержание: UP через 800 мс → GES_NONE
// пан: DOWN, MOVE(+10,0) → GES_PAN_START, MOVE(+5,0) → GES_PAN_MOVE dx=5, UP → GES_PAN_END, тапа нет
Camera c = {0, 0, 2}; camera_pan(&c, -400, -400, 200, 228); CHECK_EQ(c.x, 100); CHECK_EQ(c.y, 114);
camera_pan(&c, 1000, 1000, 200, 228); CHECK_EQ(c.x, 0); CHECK_EQ(c.y, 0);
c = (Camera){50, 60, 2}; int16_t wx, wy; camera_to_world(&c, 24, 24, &wx, &wy); CHECK_EQ(wx, 62); CHECK_EQ(wy, 72);
int col, row; CHECK(world_to_cell(13, 25, &col, &row)); CHECK_EQ(col, 1); CHECK_EQ(row, 2);
CHECK(!world_to_cell(195, 10, &col, &row));   // 195 ≥ 16*12 — вне сетки
CHECK(!world_to_cell(-1, 10, &col, &row));
Camera n = {0, 0, 1}; camera_clamp(&n, 200, 228); CHECK_EQ(n.x, 0); CHECK_EQ(n.y, 0);
```

- [ ] **Step 2:** FAIL → **Step 3:** реализовать → **Step 4:** PASS.
- [ ] **Step 5:** Commit: `Add gesture recognizer and camera`

---

### Task 10: Каркас приложения — окна, навигация, хранилище, проверка тача

**Files:**
- Create: `src/c/app/storage.{h,c}`, `src/c/app/menu_window.{h,c}`, `src/c/app/touch_off_window.{h,c}`, `src/c/app/report_window.{h,c}`, `src/c/app/gameover_window.{h,c}`, заглушки `day_window.{h,c}`, `night_window.{h,c}` (текст фазы + кнопка Select для перехода)
- Modify: `src/c/app/main.c`

**Interfaces:**
- Consumes: `Game`, `save.*`, `sim.*`, `economy.*`.
- Produces:
  - `Game *app_game(void)` (статический глобальный в `main.c`)
  - `void app_go_menu(void); void app_go_day(void); void app_go_night(void); void app_go_report(void); void app_go_gameover(void);` — каждая заменяет стек окон (pop всех + push), чтобы стек не рос
  - `storage.h`: `void storage_save(const Game *g, bool pending_night); bool storage_load(Game *g, bool *pending_night); void storage_clear(void); Best storage_best(void); void storage_set_best(Best b);` — ключи `1` header, `2` towers, `3` best
- Поток:
  - Запуск: `touch_service_is_enabled()` false → touch-off окно («Touch is off. Enable touch in Settings to play.»).
  - Меню (MenuLayer): `Continue` (только если `storage_load` успешен), `New Game`, строка `Best: N nights · S pts`.
  - New Game → `game_new(seed = time(NULL))` → `storage_save(false)` → день.
  - Continue: `pending_night` → сразу `night_begin` → ночь; иначе день.
  - День «▶ Night» → `storage_save(true)` → `night_begin` → ночь.
  - Ночь: при `PHASE_REPORT` → отчёт; при `PHASE_GAME_OVER` → `best_update`, `storage_set_best`, `storage_clear` → game over.
  - Отчёт (Night N survived / Kills / Bounty / Income / Objects hit / Towers lost; любая кнопка или тап) → `report_ack` → `storage_save(false)` → день.
  - Game over (Nights survived, Score, Best, «NEW RECORD» если побит) → меню.
  - Back в дне → `storage_save(false)` → меню.

- [ ] **Step 1:** Реализовать окна и поток по списку выше.
- [ ] **Step 2:** `pebble build` → OK без warning'ов; `make -C tests test` → PASS.
- [ ] **Step 3:** `pebble install --emulator emery`; кнопками пройти: New Game → (Select) ночь → дождаться отчёта/game over → меню; выйти из приложения и войти снова → `Continue` присутствует. `pebble screenshot` меню и отчёта, приложить к отчёту задачи.
- [ ] **Step 4:** Commit: `Add app windows, navigation and storage`

---

### Task 11: Рендер карты и дневной экран

**Files:**
- Create: `src/c/app/render.{h,c}`, `src/c/app/ui_colors.h`
- Modify: `src/c/app/day_window.{h,c}`

**Interfaces:**
- Consumes: `Camera`, `gesture_feed`, `world_to_cell`, экономика.
- Produces (`render.h`): `void render_map(GContext *ctx, const Game *g, const Camera *c, bool night);` (фон, сетка днём, объекты по статусу: живой — цвет + полоса HP, разрушенный — тёмный с крестом, не построенный — пунктирный контур); `void render_towers(GContext *ctx, const Game *g, const Camera *c, bool show_ranges);` `void render_entities(GContext *ctx, const Game *g, const Camera *c);` (дроны только при `drone_visible`; Gerbera как Shahed-136, если не `drone_identified`; ракеты, эффекты) — размеры спрайтов задаются в мировых единицах и масштабируются `zoom`.
- Дневной экран (зум ×2):
  - HUD сверху 18 px: `$money`, `DAY N`, здоровье завода (% от суммы HP построенных объектов).
  - Панель снизу 28 px: 6 иконок установок с ценой + `▶`. Выбранный тип подсвечен, при нехватке денег цена красная.
  - Миникарта 46×44 над панелью справа: объекты, установки, рамка вида.
- Тач:
  - `window_set_touch_bridge_disabled(window, true)`, `touch_service_subscribe` в `appear`, `unsubscribe` в `disappear`.
  - Пан двигает камеру.
  - Тап по панели выбирает тип / запускает ночь.
  - Тап по свободной клетке: первый ставит «призрак» с радиусом, второй по той же клетке вызывает `tower_buy`; при `ACT_NO_MONEY` короткая вибрация.
  - Тап по установке открывает панель действий вместо нижней панели: `Move`, `Sell +$x`, `Repair $y`, `Reload $z`, `X`. В режиме Move следующий тап по свободной клетке вызывает `tower_move`.
  - Тап по объекту открывает панель `Build $x` / `Repair $x` / `Rebuild $x` / `X`.
- Кнопки: Up/Down переключают тип, Back сохраняет и выходит в меню.
- Радиусы всех установок днём видны всегда (полупрозрачный контур).

- [ ] **Step 1:** Реализовать render и дневной экран.
- [ ] **Step 2:** `pebble build` → OK.
- [ ] **Step 3:** В эмуляторе `emery` мышью: прокрутить карту во все углы (не уходит за край), поставить MG двумя тапами, переместить, продать, построить Cracking Unit; одиночный тап ничего не покупает. Скриншот дня.
- [ ] **Step 4:** Commit: `Add map rendering and day screen`

---

### Task 12: Ночной экран, Air Support, отклик

**Files:**
- Create: `src/c/app/feedback.{h,c}`
- Modify: `src/c/app/night_window.{h,c}`

**Interfaces:**
- Consumes: `sim_tick`, `sim_take_events`, `air_support_call`, render из Task 11.
- Produces (`feedback.h`): `void feedback_events(uint32_t events);` — `EV_OBJECT_DESTROYED` → `vibes_long_pulse`, иначе `EV_OBJECT_HIT` → `vibes_short_pulse`; звук только на emery (`#if defined(PBL_PLATFORM_EMERY)`) и если `!speaker_is_muted()`: `EV_MISSILE` → `speaker_play_tone(880, 60, 40, SpeakerWaveformSquare)`, `EV_EXPLOSION` → `speaker_play_tone(110, 120, 60, SpeakerWaveformSawtooth)`, не чаще одного тона в 150 мс.
- Ночной экран (зум ×1, камера 0,0):
  - Тёмный фон, `render_*` с `night=true`.
  - Полупрозрачная полоса HUD 14 px: `$money`, `NIGHT N`, иконка Air Support (серая, если использована или не хватает денег), здоровье.
  - `AppTimer` 40 мс → `sim_tick(g, 40)` (дважды при ×2) → `feedback_events(sim_take_events(g))` → `layer_mark_dirty`. Смена фазы → `app_go_report` / `app_go_gameover`.
  - Тап: первый ставит метку Air Support (круг радиуса 25), повторный тап в пределах 12 px от метки вызывает `air_support_call`; метка исчезает через 3 с. При недоступности Air Support тап игнорируется.
  - Select включает и выключает ×2 (индикатор `x2` в HUD).
  - Back ставит паузу (оверлей `PAUSED — Back: quit, Select: resume`). Back на паузе — выход в меню, сохранение остаётся с `pending_night`.

- [ ] **Step 1:** Реализовать.
- [ ] **Step 2:** `pebble build` → OK; `pebble analyze-size` — приложение < 1 МБ.
- [ ] **Step 3:** В эмуляторе: ночь 1 без ПВО — видно, как дроны виляют к объектам, объекты получают урон; с MG + Radar — сбития, трассеры, взрывы; Air Support двумя тапами; ×2; пауза и выход посреди ночи → Continue стартует ту же ночь. Скриншот ночи.
- [ ] **Step 4:** Commit: `Add night screen, air support and feedback`

---

### Task 13: Проверка на часах и баланс-прогон

**Files:**
- Create: `tests/test_balance.c` (хостовый «бот» на чистой логике)
- Modify: `src/c/logic/config.c` (только если прогон покажет явный перекос)

- [ ] **Step 1:** Тест-бот: стратегия «каждое утро потратить деньги: Radar у центра, затем Gepard/Tor по кругу вокруг живых объектов, ремонт объектов при hp < 60». Прогон 20 seed'ов до game over (лимит 60 ночей). Проверки:

```c
CHECK(median_nights >= 5);    // хорошая стратегия переживает первые ночи
CHECK(median_nights < 60);    // но игра не бесконечно лёгкая
// бот «ничего не ставить» проигрывает до 4-й ночи на всех seed'ах
```

- [ ] **Step 2:** `make -C tests test` → PASS; при провале правки делать только в `config.c` и записать новые значения в отчёт задачи.
- [ ] **Step 3:** `pebble install --phone <IP>` на Pebble Time 2 пользователя (IP спросить у пользователя): плавность ночи, попадание пальцем по клеткам днём, вибрация, звук. Записать замечания.
- [ ] **Step 4:** Commit: `Add balance bot and tune config`
