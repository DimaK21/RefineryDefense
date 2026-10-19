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

## 3. Stage и Live

В LootLocker есть два окружения: Stage (тест) и Live (для игроков). Таблицы, созданные в Stage,
в Live не видны. Если вверху консоли есть переключатель окружения, проверь, что всё из шагов 1–2
сделано в **Live**, или выкати изменения из Stage в Live (кнопка Deploy/Publish, если консоль её
показывает).

## 4. Вписать ключ в проект

**Game Settings → API**: скопировать **Game Key** (рабочий ключ обычно начинается с `prod_`).
Если там же указан адрес API вида `xxxx.api.lootlocker.io`, взять и его.

Открыть [src/pkjs/lootlocker_config.js](../src/pkjs/lootlocker_config.js) и заполнить:

```js
module.exports = {
  gameKey: 'prod_...',             // Game Key из консоли
  domain: 'api.lootlocker.io',     // или адрес из консоли, без https://
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
`easy`/`normal`/`hard`, Enable Game API Writes включён, ключ из того же окружения (Live), где
созданы таблицы.
