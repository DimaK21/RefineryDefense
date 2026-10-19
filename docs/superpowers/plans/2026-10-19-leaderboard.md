# Онлайн-лидерборд через LootLocker — план

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** игрок задаёт имя из 6 символов, отправляет новый рекорд в LootLocker, видит своё место
и смотрит топ по каждой сложности из пункта меню Leaders.

**Architecture:** чистые правила (символы имени, разбор строк таблицы, сравнение рекордов) — в
`src/c/logic/board.c` с C-тестами. Всё общение с LootLocker — в PebbleKit JS (`src/pkjs`): модуль
`lootlocker.js` с подменяемым HTTP и хранилищем, его гоняют node-тесты; `index.js` только
связывает AppMessage с этим модулем. На часах `net.c` — один запрос за раз, тайм-аут, колбэк;
три новых окна (имя, Leaders, результат отправки) читают только `net.h`, `storage.h` и `board.h`.

**Tech Stack:** C99, Pebble SDK 3 (`emery`, 200×228, тач), PebbleKit JS (CommonJS `require`,
`enableMultiJS`), тесты — `make -C tests test` (clang + node 22).

**Spec:** [docs/superpowers/specs/2026-10-19-leaderboard-design.md](../specs/2026-10-19-leaderboard-design.md)

## Global Constraints

- Сортировка как у `best_update`: больше ночей лучше, при равенстве — больше очков.
  Число для LootLocker: `nights * 1000000 + min(score, 999999)`.
- Таблицы LootLocker: ключи `easy`, `normal`, `hard` (индекс = `Difficulty`), тип Player.
- Имя: до 6 символов из `A–Z`, `0–9`, пробел; пробелы по краям обрезаются; пустое не сохраняется.
- В сеть — только по явному действию: открыть Leaders, отправить рекорд. На `ready` в JS ничего
  не отправляется.
- Тайм-аут запроса с часов — 15 000 мс.
- Тексты ошибок ровно: `No phone connection`, `No internet`, `Server error`. Пустая таблица:
  `No records yet. Be the first!`. Все тексты UI — на английском, как остальная игра.
- Без телефона и интернета игра работает как раньше; формат сохранения игры не меняется.
- Цель — приложение Pebble от Core Devices на Android; `Pebble.getAccountToken()` может быть
  пустым → случайный UUID в `localStorage`.
- Версия `1.4.0`.
- Коммиты — одна строка до 80 символов, без тела и атрибуции; коммитить только после «ок»
  пользователя.

## Review Focus

- Ответ телефона приходит после того, как игрок ушёл с экрана (Back посреди «Loading…») →
  колбэк не трогает уничтоженное окно, новое окно не получает чужой ответ (тест на `seq` в
  Task 3, ревью Task 4–6).
- Сессия LootLocker протухла (401) → один перелогин и повтор, без бесконечного цикла
  (тест в Task 3).
- Имя с `|` или переводом строки не может сломать упаковку строк: JS заменяет такие символы в
  чужих именах пробелом, C-разбор пропускает битые строки (тесты в Task 1 и Task 3).
- Сервер хранит результат лучше отправленного → экран показывает `Your best online: …`, досылка в
  Leaders больше не предлагается (тест в Task 3 на `online`, ревью Task 6).
- Ответ на запрос таблицы приходит, когда игрок уже переключил сложность → показывается только
  ответ на текущую сложность (ревью Task 5).

---

### Task 1: Правила имени, строк таблицы и сравнения рекордов

**Files:**
- Create: `src/c/logic/board.h`, `src/c/logic/board.c`
- Test: `tests/test_board.c`

**Interfaces:**
- Produces (в `board.h`):
  - `#define NAME_LEN 6`, `#define BOARD_MAX_ROWS 10`
  - `typedef struct { uint32_t rank; char name[NAME_LEN + 1]; uint16_t nights; uint32_t score; } BoardRow;`
  - `char name_cycle(char c, int dir);` — порядок `A..Z`, `0..9`, пробел, по кругу; `dir` ±1;
    символ вне набора → `'A'`.
  - `void name_trim(char *s);` — обрезает пробелы по краям на месте.
  - `bool name_valid(const char *s);` — после обрезки не пустое, длина ≤ `NAME_LEN`, только
    символы набора.
  - `bool board_parse_row(const char *line, size_t len, BoardRow *out);` — формат
    `rank|name|nights|score`, имя может быть пустым или короче 6, длиннее — обрезается до 6.
  - `int board_parse_rows(const char *packed, BoardRow *out, int max);` — строки через `\n`,
    битые пропускаются, возвращает число разобранных.
  - `bool best_beats(Best a, Best b);` — `a` лучше `b` по правилу `best_update`.

- [ ] **Step 1: Write the failing tests** в `tests/test_board.c` (стиль как `tests/test_save.c`):
  - `test_name_cycle`: `name_cycle('A', 1) == 'B'`, `name_cycle('Z', 1) == '0'`,
    `name_cycle('9', 1) == ' '`, `name_cycle(' ', 1) == 'A'`, `name_cycle('A', -1) == ' '`,
    `name_cycle('a', 1) == 'A'`.
  - `test_name_trim_valid`: `"  DIM "` → `"DIM"`; `name_valid("DIM")`, `name_valid("A1 B2")`,
    `!name_valid("      ")`, `!name_valid("")`, `!name_valid("ABCDEFG")`, `!name_valid("ab")`.
  - `test_parse_row`: `"14|DIM|12|3400"` → rank 14, name `"DIM"`, nights 12, score 3400;
    `"3||5|10"` → пустое имя, ok; `"1|ABCDEFGH|2|3"` → имя `"ABCDEF"`; `"x|A|1|1"`, `"1|A|1"`,
    `""` → false.
  - `test_parse_rows`: `"1|A|9|900\nbad\n2|B|8|800"` → 2 строки, вторая rank 2; `max = 1` → 1;
    пустая строка → 0.
  - `test_best_beats`: `{12,3000}` бьёт `{11,9000}`; `{12,3001}` бьёт `{12,3000}`; равные — нет;
    `{0,0}` не бьёт `{0,0}`.

- [ ] **Step 2: Run** `make -C tests test_board.bin` — Expected: ошибка сборки (нет `board.h`).

- [ ] **Step 3: Implement** `board.h`/`board.c` по интерфейсам выше. `best_beats` выражается через
  то же условие, что `best_update` в [save.c:107](../../../src/c/logic/save.c) — вынести условие в
  `best_beats` и вызвать его из `best_update`, чтобы правило жило в одном месте. Без `atoi`/`strtol`
  с локалью — свой разбор десятичных цифр (в логике нет `pebble.h`).

- [ ] **Step 4: Run** `make -C tests test` — Expected: все тесты, включая `test_board.bin`,
  `0 failed`.

- [ ] **Step 5: Commit** (после «ок»): `Add leaderboard name and row rules`

---

### Task 2: Хранение имени и отправленных рекордов

**Files:**
- Modify: `src/c/app/persist_keys.h`, `src/c/app/storage.h`, `src/c/app/storage.c`
- Test: `tests/test_persist_keys.c` (проверяется автоматически через `PERSIST_KEYS`)

**Interfaces:**
- Consumes: `NAME_LEN`, `name_valid` из Task 1; `Best` из `save.h`.
- Produces:
  - `#define KEY_NAME 8`, `#define KEY_SENT_BESTS 9`, оба добавлены в `PERSIST_KEYS`.
  - `bool storage_name(char out[NAME_LEN + 1]);` — false, если имени нет или оно не
    `name_valid` (тогда `out` — пустая строка).
  - `void storage_set_name(const char *name);`
  - `Best storage_sent_best(Difficulty d);` — `{0,0}`, если ничего не отправлялось.
  - `void storage_set_sent_best(Difficulty d, Best b);`

- [ ] **Step 1: Add** ключи и `CHECK(KEY_NAME != KEY_SENT_BESTS)` в `test_keys_distinct`.
- [ ] **Step 2: Implement** функции в `storage.c` по образцу `storage_best`/`storage_set_best`
  (массив `Best[DIFF_COUNT]` в одном ключе, проверка размера при чтении).
- [ ] **Step 3: Run** `make -C tests test` и `pebble build` — Expected: `0 failed`, сборка без
  предупреждений.
- [ ] **Step 4: Commit** (после «ок»): `Store player name and sent records`

---

### Task 3: Клиент LootLocker на телефоне

**Files:**
- Create: `src/pkjs/lootlocker.js`, `src/pkjs/lootlocker_config.js`, `src/pkjs/index.js`
- Create: `tests/pkjs/test_lootlocker.js`
- Modify: `tests/Makefile` (цель `test` после C-тестов запускает `node pkjs/test_lootlocker.js`)
- Modify: `package.json` — `messageKeys`

**Interfaces:**
- Produces (`messageKeys`, заменяют `dummy`): `REQ` (1 = table, 2 = submit), `SEQ`, `DIFF`,
  `NIGHTS`, `SCORE`, `NAME`, `STATUS` (0 ok, 1 no internet, 2 server error), `RANK`, `TOTAL`,
  `ROWS` (строки `rank|name|nights|score` через `\n`), `ME` (одна такая строка или пусто),
  `ON_NIGHTS`, `ON_SCORE` (лучший результат игрока на сервере после submit).
  Ответ всегда несёт тот же `SEQ`, что запрос.
- Produces (`lootlocker_config.js`): `module.exports = { gameKey: '', domain: 'api.lootlocker.io', gameVersion: '1.4.0' }`
  — пустой ключ до настройки пользователем.
- Produces (`lootlocker.js`): `module.exports = { pack, unpack, createClient }`
  - `pack(nights, score) -> number` и `unpack(n) -> {nights, score}` по формуле из Global
    Constraints.
  - `createClient({ http, store, config, playerIdentifier })`, где
    `http(method, url, headers, bodyObj, cb(err, status, json))` — `err` только при сетевой
    ошибке; `store` — `{getItem, setItem}`.
  - `client.table(diff, name, cb(reply))` → `{status, total, rows, me}`: топ-10 и своя строка
    (пусто, если игрока нет в таблице).
  - `client.submit(diff, nights, score, name, cb(reply))` → `{status, rank, total, rows, online}`:
    `rows` — до 2 выше, своя, до 2 ниже; `online` — `{nights, score}` из сохранённого сервером
    `score`.
  - Оба перед запросом таблицы ставят имя (`PATCH /game/player/name`), если оно отличается от
    сохранённого в `store` под `ll_name`; пустое имя — без `PATCH`.
  - Сессия: `ll_session` в `store`; нет — гостевой вход. На 401 — один перелогин и повтор того же
    вызова, второй 401 → `status 2`.
  - Имена в `rows`/`me`: символы `|` и `\n` заменяются пробелом.
- Пути и поля LootLocker сверить с актуальной справкой до кода (ссылки в спеке, раздел
  «Телефон»). Если путь «свое место» или пагинация в справке другие — следовать справке, менять
  только внутренности `createClient`.

- [ ] **Step 1: Write the failing tests** `tests/pkjs/test_lootlocker.js` на `node:assert` с
  фейковым `http`, который пишет журнал вызовов и отвечает по таблице `method + path`:
  - `pack(12, 3400) === 12003400`; `pack(1, 5000000) === 1999999`;
    `unpack(12003400)` → `{nights: 12, score: 3400}`.
  - `table` без сессии в `store`: первый вызов — гостевой вход с `game_key` и
    `player_identifier`, затем `PATCH` имени, затем список; `reply.status === 0`,
    `rows.length === 10`, `me` — строка игрока, если его `member_id` есть в ответе места.
  - Повторный `table` с тем же именем — нет ни входа, ни `PATCH`.
  - 401 на списке → вход ещё раз и повтор; два 401 подряд → `status === 2`, всего не больше двух
    входов.
  - `err` от `http` → `status === 1`; 500 → `status === 2`.
  - `submit(1, 12, 3400, 'DIM')` шлёт `score: 12003400` в `/leaderboards/normal/submit`; сервер
    вернул `score: 13000100` → `online` = `{nights: 13, score: 100}`.
  - Имя `'A|B'` в ответе сервера → в `rows` `'A B'`.
- [ ] **Step 2: Run** `node tests/pkjs/test_lootlocker.js` — Expected: падает на `require`.
- [ ] **Step 3: Implement** `lootlocker.js` и `lootlocker_config.js`.
- [ ] **Step 4: Implement `index.js`:** `appmessage` → по `REQ` вызывает `table`/`submit`, отвечает
  `Pebble.sendAppMessage` с тем же `SEQ`; `http` поверх `XMLHttpRequest` (сетевая ошибка и
  тайм-аут XHR 12 000 мс → `err`); `store` = `localStorage`; `playerIdentifier` —
  `Pebble.getAccountToken()` или UUID, один раз сохранённый под `ll_pid`. В `ready` — ничего.
- [ ] **Step 5: Run** `make -C tests test` и `pebble build` — Expected: C и JS тесты `0 failed` /
  без исключений, сборка проходит.
- [ ] **Step 6: Commit** (после «ок»): `Add LootLocker client in PebbleKit JS`

---

### Task 4: Связь часов с телефоном

**Files:**
- Create: `src/c/app/net.h`, `src/c/app/net.c`
- Modify: `src/c/app/main.c` (`net_init()` до первого окна)

**Interfaces:**
- Consumes: `messageKeys` из Task 3 (`MESSAGE_KEY_*`), `BoardRow`, `board_parse_rows`,
  `board_parse_row`, `BOARD_MAX_ROWS` из Task 1.
- Produces:
  ```c
  typedef enum { NET_OK, NET_NO_PHONE, NET_NO_INTERNET, NET_SERVER } NetStatus;
  typedef struct {
    NetStatus status;
    uint32_t rank, total;
    BoardRow rows[BOARD_MAX_ROWS]; int n_rows;
    BoardRow me; bool has_me;
    Best online;
  } NetReply;
  typedef void (*NetCallback)(const NetReply *r);
  void net_init(void);
  void net_request_table(Difficulty d, const char *name, NetCallback cb);
  void net_request_submit(Difficulty d, Best b, const char *name, NetCallback cb);
  void net_cancel(void);   /* окно уходит: колбэк больше не вызывается */
  const char *net_status_text(NetStatus s);   /* "No phone connection" / "No internet" / "Server error" / NULL для NET_OK */
  ```
- Один запрос за раз: новый запрос отменяет предыдущий. Каждый запрос получает новый `SEQ`;
  ответ с другим `SEQ` игнорируется. Ошибка отправки (`app_message_outbox_send` не `APP_MSG_OK`
  или `outbox_failed`) → `NET_NO_PHONE`; нет ответа за 15 000 мс → `NET_NO_PHONE`.
- Буферы `app_message_open`: inbox 1024, outbox 128 (поправить, если строки не влезают).
  `NetReply` — статический в `net.c`, колбэк получает указатель на него.

- [ ] **Step 1: Implement** `net.c` по интерфейсу.
- [ ] **Step 2: Run** `pebble build` — Expected: сборка без предупреждений.
- [ ] **Step 3: Verify** в эмуляторе: временно вызвать `net_request_table(DIFF_NORMAL, "TEST", …)`
  из меню с `APP_LOG` статуса; с пустым `gameKey` — `Server error`; при
  `pebble emu-bt-connection --connected no` — `No phone connection` не позже чем через 15 с.
  Убрать временный вызов.
- [ ] **Step 4: Commit** (после «ок»): `Add watch-phone request channel`

---

### Task 5: Окно ввода имени и строка Name в Settings

**Files:**
- Create: `src/c/app/name_window.h`, `src/c/app/name_window.c`
- Modify: `src/c/app/settings_window.c`, `src/c/app/main.c` (destroy)

**Interfaces:**
- Consumes: `name_cycle`, `name_trim`, `name_valid`, `NAME_LEN` (Task 1);
  `storage_name`, `storage_set_name` (Task 2).
- Produces:
  - `typedef void (*NameDone)(bool saved);`
  - `void name_window_push(NameDone done);` — `done` может быть `NULL`; вызывается после
    `window_stack_pop` окна.
  - `void name_window_destroy(void);`

Поведение — по спеке, раздел «Ввод имени»: Up/Down через
`window_single_repeating_click_subscribe` с интервалом 100 мс; Select — следующая позиция,
на 6-й — сохранить; долгий Select (`window_long_click_subscribe`) — сохранить сразу; Back —
позиция назад, на первой — отмена. Пустое после обрезки имя не сохраняется (окно остаётся).
Новая строка `Name` в Settings: подпись — имя или `Not set`, открывает окно. 4 строки по
`ROW_H 44` не влезают — ужать строки так, чтобы все 4 и футер с версией помещались на 228 px.

- [ ] **Step 1: Implement** окно и строку в Settings.
- [ ] **Step 2: Run** `pebble build` — Expected: без предупреждений.
- [ ] **Step 3: Verify** в эмуляторе (`pebble install --emulator emery`): удержание Down листает
  быстро; ввод `DIM` + долгий Select → Settings показывает `DIM`; повторное открытие начинается
  с `DIM`; Back с первой позиции не меняет имя; все 4 строки Settings и версия видны.
- [ ] **Step 4: Commit** (после «ок»): `Add player name entry`

---

### Task 6: Экран Leaders и пункт меню

**Files:**
- Create: `src/c/app/leaders_window.h`, `src/c/app/leaders_window.c`
- Modify: `src/c/app/menu_window.c`, `src/c/app/main.c` (destroy)

**Interfaces:**
- Consumes: `net_request_table`, `net_request_submit`, `net_cancel`, `net_status_text`,
  `NetReply` (Task 4); `name_window_push` (Task 5); `storage_name`, `storage_best`,
  `storage_sent_best`, `storage_set_sent_best` (Task 2); `best_beats` (Task 1).
- Produces: `void leaders_window_push(Difficulty d);`, `void leaders_window_destroy(void);`

Поведение — по спеке, раздел «Пункт меню Leaders». Уточнения:
- Пункт `Leaders` — в `Item` между игровыми пунктами и `ITEM_SETTINGS`; `s_items` на 4;
  раскладка `prv_btn_rect` и строка BEST — без наложения при 4 пунктах.
- Открывается на `storage_exists() ? storage_saved_difficulty() : s_last_diff`.
- Запрос без имени шлёт пустое имя (JS тогда не делает `PATCH`).
- Ответ применяется, только если он на текущую сложность (запомнить сложность запроса).
- Досылка видна, когда `best_beats(storage_best(d), storage_sent_best(d))`; долгий Select → имя,
  если его нет (`name_window_push` с колбэком, отмена — ничего не шлём) → `net_request_submit` →
  при `NET_OK` `storage_set_sent_best(d, best_beats(online, sent) ? online : sent)` и перезагрузка
  таблицы.
- `net_cancel()` на `disappear`.

- [ ] **Step 1: Implement** окно и пункт меню.
- [ ] **Step 2: Run** `pebble build` — Expected: без предупреждений.
- [ ] **Step 3: Verify** в эмуляторе: в меню 4 пункта, BEST не наезжает; Leaders с пустым ключом
  → `Loading…`, затем `Server error`; Select переключает E → N → H → E; Back посреди `Loading…`
  возвращает в меню без падения.
- [ ] **Step 4: Commit** (после «ок»): `Add Leaders screen`

---

### Task 7: Отправка рекорда после Game Over

**Files:**
- Create: `src/c/app/submit_window.h`, `src/c/app/submit_window.c`
- Modify: `src/c/app/gameover_window.c`, `src/c/app/main.c` (destroy)

**Interfaces:**
- Consumes: всё из Task 4, `name_window_push` (Task 5), `storage_*` (Task 2), `best_beats` (Task 1).
- Produces: `void submit_window_push(Difficulty d, Best b);`, `void submit_window_destroy(void);`

Поведение — по спеке, раздел «Конец игры с новым рекордом». Уточнения:
- В `gameover_window.c` при `s_record`: футер `Select: send · Tap: skip`; Select → если
  `storage_name` пусто, `name_window_push` с колбэком (отмена → остаёмся на Game Over), иначе
  `submit_window_push`. Остальные кнопки и тап — как сейчас.
- Экран результата: `Sending…` → место `#14 of 230` и до 5 строк вокруг, своя выделена
  `UI_ACCENT`; если `best_beats(online, b)` — строка `Your best online: N nights · S`. Ошибка —
  текст `net_status_text`. Back или тап — `app_go_menu()`.
- При `NET_OK`: `storage_set_sent_best(d, best_beats(online, b) ? online : b)`.

- [ ] **Step 1: Implement** окно и правку Game Over.
- [ ] **Step 2: Run** `make -C tests test` и `pebble build` — Expected: `0 failed`, сборка без
  предупреждений.
- [ ] **Step 3: Verify** в эмуляторе: проиграть на Easy с новым рекордом → футер
  `Select: send · Tap: skip`; Select без имени → окно имени → `Sending…` → `Server error`
  (пустой ключ); тап → меню, ничего не отправлено; без рекорда футер прежний.
- [ ] **Step 4: Commit** (после «ок»): `Offer to send a new record`

---

### Task 8: Версия, инструкция по LootLocker, проверка с ключом

**Files:**
- Modify: `package.json` (`version` → `1.4.0`), `docs/MECHANICS.md` (раздел про рекорд: онлайн-таблица)
- Create: `docs/LEADERBOARD_SETUP.md`

- [ ] **Step 1: Write** `docs/LEADERBOARD_SETUP.md` по-русски, по шагам для консоли LootLocker:
  включить Guest Login; создать таблицы `easy`/`normal`/`hard` (Player, по убыванию, без
  перезаписи меньшим); выкатить в Live; вписать Live Game API key и домен в
  `src/pkjs/lootlocker_config.js`. Названия пунктов консоли сверить с актуальной документацией.
- [ ] **Step 2: Bump** версию и дописать MECHANICS.md.
- [ ] **Step 3: Run** `make -C tests test` и `pebble build` — Expected: `0 failed`,
  `build/refinery-defense-1.4.0.pbw` создан.
- [ ] **Step 4: Verify с ключом** (когда пользователь вписал ключ): в эмуляторе отправить рекорд →
  место `#1 of 1`; Leaders показывает строку; смена имени в Settings → новое имя при следующем
  открытии Leaders. Финальную проверку на телефоне (Core Devices, Android) делает пользователь.
- [ ] **Step 5: Commit** (после «ок»): `Add LootLocker setup guide and bump to 1.4.0`
