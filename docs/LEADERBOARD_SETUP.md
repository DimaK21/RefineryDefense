# Настройка LootLocker для онлайн-лидерборда

Без этой настройки игра работает как обычно, но Leaders и отправка рекорда показывают
`Server error`. Всё делается один раз в веб-консоли [LootLocker](https://console.lootlocker.com).

## 1. Включить вход гостей

**Game Settings → Platforms → Guest Login** — включить и сохранить. Игроки входят по скрытому
идентификатору телефона и не видят никакого логина.

## 2. Создать три таблицы

**Leaderboards → Create**, три раза, по одной на сложность:

| Name (любое) | Key (ровно так) |
|---|---|
| Easy | `easy` |
| Normal | `normal` |
| Hard | `hard` |

Для каждой:
- **Type:** Player Leaderboards.
- **Direction Method:** Descending (больше — выше).
- **Enable Game API Writes:** **включить**. Без этого игра не сможет отправить результат.
- **Overwrite Score on Submit:** **выключить**. Тогда более слабый результат не затрёт лучший.
- **Enable Metadata:** не нужно.

Сброс по расписанию (Cron) и награды не настраивать.

## 3. Окружение и ключ

С марта 2025 года у новых игр в LootLocker одно окружение, и консоль выдаёт только ключ вида
`dev_…`. Это обычный рабочий ключ игры: с ним играют и настоящие игроки, `prod_`-ключа не будет.
LootLocker советует для тестов завести отдельную игру. Для этого проекта хватит одной: тестовые
записи можно удалить в консоли (Leaderboards → таблица → запись).

(У старых игр бывают окружения Stage и Live: тогда `dev_` — Stage, `prod_` — Live, и в сборку
для игроков идёт ключ Live.)

## 4. Вписать ключ в проект

**Game Settings → API**: скопировать **Game Key** и адрес API вида
`https://xxxx.api.lootlocker.com`.

Открыть [src/pkjs/lootlocker_config.js](../src/pkjs/lootlocker_config.js) и заполнить:

```js
module.exports = {
  gameKey: 'dev_...',                         // Game Key из консоли
  domain: 'https://xxxx.api.lootlocker.com',  // адрес из консоли, можно с https:// или без
  gameVersion: '1.4.0'
};
```

Ключ клиентский: он всё равно лежит внутри `.pbw`, поэтому его можно коммитить.

## 5. Проверить

1. Собрать: `pebble build`, поставить на часы через приложение Pebble (Core Devices).
2. Settings → Name — задать имя.
3. Меню → Leaders: должно появиться `No records yet. Be the first!` (а не `Server error`).
4. Сыграть до Game Over с новым рекордом → Select → должно показать `#1 of 1`.
5. В консоли LootLocker: Leaderboards → нужная таблица → в списке появилась запись.

Если `Server error` остаётся — проверить по порядку: Guest Login включён, ключи таблиц ровно
`easy`/`normal`/`hard`, Enable Game API Writes включён, ключ и адрес API скопированы из этой же
игры.
