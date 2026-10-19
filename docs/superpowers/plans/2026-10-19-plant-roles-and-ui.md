# Роли объектов, перезарядка одной кнопкой, понятные способности — план

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** объекты завода получают роли, второй стартовый объект случайный, ракеты докупаются одной
кнопкой, способности ночью понятны.

**Architecture:** правила (скидки, роли, старт, перезарядка всех) — чистая логика в
`src/c/logic` с юнит-тестами в `tests/`; UI в `src/c/app` только читает её функции цен.
Роль объекта — строка в `ObjectSpec` плюс проверка «жив» в местах трат.

**Tech Stack:** C99, Pebble SDK 3 (`emery`), тесты — `make -C tests test` (clang).

**Spec:** [docs/superpowers/specs/2026-10-19-plant-roles-and-ui-design.md](../specs/2026-10-19-plant-roles-and-ui-design.md)

## Global Constraints

- Скидки: ракеты −50% (Tank Farm B), установки −15% покупка и ремонт (Loading Terminal),
  способности −30% цена и −30% перезарядка (Cracking Unit). Комбо — только при живой Tank Farm A.
- Цена со скидкой: вниз до доллара, не ниже $1 для ненулевой цены. Перезарядка: вниз до мс.
- Роль действует, пока объект `OBJ_ALIVE`, при любом HP.
- Sell — 50% базовой цены. Доходы, цены объектов, формула волны, формат сохранения — без изменений.
- Тексты UI — на английском, как остальная игра.
- Коммиты — одна строка до 80 символов, без тела и атрибуции; коммитить только после «ок»
  пользователя.

## Review Focus

- Tank Farm A разрушена посреди серии → следующее сбитие без множителя, COMBO гаснет
  (тест в Task 3).
- Денег меньше полной перезарядки, но хватает на часть → ракеты распределяются, ни одна установка
  не остаётся пустой, если хватает хотя бы по одной (тест в Task 2).
- Денег меньше одной ракеты → ничего не тратится, `ACT_NO_MONEY` (тест в Task 2).
- Старые сохранения (центр + A) и рестарт ночи: seed волны не меняется от выбора старта
  (тест в Task 1).
- Цены на экране совпадают со списанием: бар, панель, HUD ночью берут те же функции, что и
  списание (ревью Task 5–6, проверка в эмуляторе).

---

### Task 1: Роли в конфиге и случайный старт

**Files:**
- Modify: `src/c/logic/config.h`, `src/c/logic/config.c`, `src/c/logic/economy.h`, `src/c/logic/economy.c`
- Modify (тесты, которые опираются на «центр + Tank Farm A»): `tests/test_*.c`, `tests/bench.c`
- Test: `tests/test_economy.c`

**Interfaces:**
- Produces: `ObjectSpec.role` (`const char *`, `NULL` у центра): A `"Combo x2"`, B `"Missiles -50%"`,
  L `"Towers -15%"`, C `"Abilities -30%"`.
- Produces: `#define ROLE_MISSILE_OFF_PCT 50`, `ROLE_TOWER_OFF_PCT 15`, `ROLE_ABILITY_OFF_PCT 30`;
  `#define OBJ_ROLE_COMBO OBJ_TANK_A`, `OBJ_ROLE_MISSILES OBJ_TANK_B`, `OBJ_ROLE_TOWERS OBJ_TERMINAL`,
  `OBJ_ROLE_ABILITIES OBJ_CRACKER` в `config.h`.
- Produces: `bool object_alive(const Game *g, ObjectId o);`
- Produces: `void game_new_start(Game *g, uint32_t seed, Difficulty d, ObjectId second);` — как
  нынешний `game_new`, но второй объект задан, RNG сверх `g->seed` не трогает.
  `game_new` = `game_new_start` + выбор второго из {A, B, L, C} через `rng_range(&g->rng, ...)`
  после того, как взят `g->seed`.
- Produces: `int32_t discounted(int32_t cost, int off_pct);` — вниз, не ниже 1 при `cost > 0`.

- [ ] **Step 1: тесты** в `test_economy.c`: `test_start_object` — на сидах 1..200 центр жив
  всегда, ровно один из A/B/L/C жив, каждый из четырёх встретился; `g.seed` после `game_new` равен
  `g.seed` после `game_new_start` с тем же сидом. `test_discounted`: `discounted(5, 50) == 2`,
  `discounted(1, 50) == 1`, `discounted(0, 50) == 0`, `discounted(70, 15) == 59`.
- [ ] **Step 2:** `make -C tests test` — новые тесты падают (не собираются).
- [ ] **Step 3:** реализовать; во всех тестах `game_new(&g, s, d)` → `game_new_start(&g, s, d, OBJ_TANK_A)`,
  кроме `test_start_object` и бота в `test_balance.c` (Task 4).
- [ ] **Step 4:** `make -C tests test` — всё зелёное, числа баланса те же, что до правки.

### Task 2: Скидки на установки и ракеты, перезарядка всех

**Files:**
- Modify: `src/c/logic/economy.h`, `src/c/logic/economy.c`
- Test: `tests/test_economy.c`

**Interfaces:**
- Consumes: `object_alive`, `discounted`, константы ролей (Task 1).
- Produces: `int32_t tower_price(const Game *g, TowerType t);` — цена покупки с учётом Terminal;
  `tower_buy` списывает её.
- Produces: `tower_repair_cost` — нынешняя формула, затем `discounted(..., ROLE_TOWER_OFF_PCT)` при
  живом Terminal.
- Produces: `int32_t missile_price(const Game *g, TowerType t);` — `ammo_price`, при живой Tank
  Farm B — `discounted(..., ROLE_MISSILE_OFF_PCT)`; `tower_reload_cost = недостающие × missile_price`.
- Produces: `int32_t reload_all_cost(const Game *g);` — сумма `tower_reload_cost` по всем.
- Produces: `ActResult reload_all(Game *g);` — `ACT_INVALID` не днём или если докупать нечего;
  `ACT_NO_MONEY` если не хватает ни на одну ракету (ничего не тратит); иначе `ACT_OK`: по одной
  ракете в установку с наименьшей долей боезапаса (`ammo/max`, при равенстве — меньший индекс),
  пока хватает денег и есть кому.

- [ ] **Step 1: тесты** `test_role_discounts`: с живым Terminal `tower_price(MG) == 25`,
  `tower_price(Patriot) == 212`, покупка Gepard списывает 59; без Terminal — базовые цены; ремонт
  MG с hp 1 при Terminal — 5 (`discounted(6, 15)`). С живой B: `missile_price(Tor) == 2`,
  `missile_price(Patriot) == 15`, Tor с ammo 2 → reload 8. Разрушенная B — снова 5 и 30. Sell
  при Terminal — 15 за MG.
- [ ] **Step 2: тесты** `test_reload_all`: два Tor (ammo 0 и 4) и Patriot (ammo 3) без B →
  `reload_all_cost == 40`; при деньгах 1000 после `reload_all` все полные, списано 40. При
  деньгах 15 → `ACT_OK`, Tor с ammo 0 стал 3, второй Tor остался 4, деньги 0. Два пустых Tor
  при деньгах 10 → по одной ракете каждому. При деньгах 4 →
  `ACT_NO_MONEY`, ничего не изменилось. Все полные → `ACT_INVALID`. Ночью → `ACT_INVALID`.
- [ ] **Step 3:** `make -C tests test` — падают.
- [ ] **Step 4:** реализовать.
- [ ] **Step 5:** `make -C tests test` — зелёное (баланс может сдвинуться: бот Terminal не
  строит, но может стартовать с ним только после Task 4).

### Task 3: Способности с Cracking Unit, комбо только с Tank Farm A

**Files:**
- Modify: `src/c/logic/sim.h`, `src/c/logic/sim.c`, `src/c/logic/combat.c`
- Test: `tests/test_sim.c`, `tests/test_combat.c`

**Interfaces:**
- Produces: `int32_t ability_cost(const Game *g, Ability a);`,
  `uint16_t ability_cooldown(const Game *g, Ability a);` — со скидкой 30% при живом Cracking Unit.
  `ability_ready` и `ability_use` используют их.
- `kill_drone`: если `OBJ_ROLE_COMBO` не жив — `g->combo = 0`, множитель 100, серия не растёт,
  `best_combo` не обновляется.

- [ ] **Step 1: тесты** в `test_sim.c`: с живым Cracking Unit `ability_cost(AIR) == 42`,
  `ability_cost(EW) == 17`, `ability_cost(FLARE) == 10`, после `ability_use(AIR)` кулдаун 17500
  и списано 42; без него — 60 и 25000.
- [ ] **Step 2: тесты** в `test_combat.c`: без Tank Farm A 6 быстрых сбитий по $10 дают $60, `best_combo == 0`;
  с A — как сейчас; A разрушена после 3 сбитий → 4-е платит $10 и `g.combo == 0`.
- [ ] **Step 3:** `make -C tests test` — падают.
- [ ] **Step 4:** реализовать.
- [ ] **Step 5:** `make -C tests test` — зелёное.

### Task 4: Баланс

**Files:**
- Modify: `tests/test_balance.c`, при необходимости `src/c/logic/config.c`, `docs/MECHANICS.md`

- [ ] **Step 1:** бот утром вызывает `reload_all` вместо цикла `tower_reload`; бот стартует через
  `game_new` (случайный второй объект).
- [ ] **Step 2:** новая проверка: для каждого второго объекта (A/B/L/C, через `game_new_start`
  на тех же сидах) среднее ночей бота на Normal не дальше 3 ночей от общего среднего;
  печатать средние по старту.
- [ ] **Step 3:** `make -C tests test`. Если критерии не проходят — подкрутить цифры ролей
  (не доходы и не волну), записать причину в комментарий у констант и референсные числа в шапку
  `test_balance.c` и раздел «Проверка баланса» в MECHANICS.md.

### Task 5: UI дня — цены, роль в панели, кнопка Reload

**Files:**
- Modify: `src/c/app/day_window.c`, `src/c/app/day_panel.c`

**Interfaces:**
- Consumes: `tower_price`, `reload_all_cost`, `reload_all`, `ObjectSpec.role` (Task 1–2).

- [ ] **Step 1:** нижний бар показывает `tower_price(g, t)`, красный — если денег меньше неё.
- [ ] **Step 2:** панель объекта показывает роль (`ObjectSpec.role`) для любого статуса; у центра —
  имя. Как уместить в две строки 28 px — решить по эмулятору.
- [ ] **Step 3:** кнопка Reload: прямоугольник 46×28 в левом нижнем углу карты (над баром,
  зеркально миникарте), две строки `Reload` / `$N` (`N = reload_all_cost`, красное при нехватке).
  Видна, если `reload_all_cost > 0` и нет панели, перемещения и взведённого старта. Тап →
  `reload_all`; `ACT_NO_MONEY` → `feedback_short()`. Если перекрывает стрелку налёта слева/снизу —
  сдвинуть.
- [ ] **Step 4:** `pebble build` без ошибок и предупреждений.

### Task 6: UI ночи — HUD, карточка, подпись метки, подсказка

**Files:**
- Modify: `src/c/app/night_window.c`

**Interfaces:**
- Consumes: `ability_cost`, `ability_cooldown`, `ability_ready` (Task 3).

- [ ] **Step 1: HUD.** Вместо иконки и трёх точек — `AIR` / `EW` / `FLARE` (жёлтым — готова,
  серым — нельзя); на перезарядке — имя и секунды (`EW 7`). Иконку оставить, если влезает.
- [ ] **Step 2: карточка.** На Up/Down — блок по центру ~2 с (по реальному времени, как метка):
  иконка ×2, название, строка действия, `$цена · Ns`. Тексты:
  `AIR STRIKE` — "Destroys all drones in the circle after 1.5s";
  `EW PULSE` — "Slows drones in the circle for 5s";
  `FLARE` — "Lights the circle for 6s, towers in it get full range".
  Новое Up/Down заменяет карточку; пауза её прячет.
- [ ] **Step 3: подпись метки.** Под кругом метки: `Tap again: Air Strike $60`, либо `Wait 12s`,
  либо `Need $60`; у нижнего края — над кругом; не выходит за края по горизонтали.
- [ ] **Step 4: подсказка.** В баннере начала ночи третья строка `Up/Down: ability · Tap map: aim`
  (баннер подрастает).
- [ ] **Step 5:** `pebble build` без ошибок.

### Task 7: Тексты, документация, проверка в эмуляторе

**Files:**
- Modify: `src/c/app/help_window.c`, `docs/MECHANICS.md`

- [ ] **Step 1:** How to play: «Night» — комбо только с Tank Farm A; «Money» — второй объект
  случайный; новая страница «Plant units» — роли пяти объектов; упомянуть кнопку Reload на
  странице «Towers» или «Day: manage».
- [ ] **Step 2:** MECHANICS.md: колонка «Роль» в таблице завода, старт, комбо, цены со скидками,
  кнопка Reload, способности (HUD, карточка, подпись), How to play — 9 страниц.
- [ ] **Step 3:** `make -C tests test` и `pebble build` — зелёные.
- [ ] **Step 4:** эмулятор `emery`: скриншоты дня (бар, панель объекта с ролью, кнопка Reload) и
  ночи (HUD, карточка, подпись метки, баннер с подсказкой); поправить всё, что не влезает.
- [ ] **Step 5:** показать пользователю, после «ок» — коммит одной строкой.
