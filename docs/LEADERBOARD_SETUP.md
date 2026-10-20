# LootLocker setup for the online leaderboard

Without this setup the game works as usual, but Leaders and record submission show `Server error`.
Everything is done once in the [LootLocker](https://console.lootlocker.com) web console.

## 1. Enable guest login

**Game Settings → Platforms → Guest Login** — enable and save. Players are signed in with a hidden
phone identifier and never see a login screen.

## 2. Create three leaderboards

**Leaderboards → Create**, three times, one per difficulty:

| Name (anything) | Key (exactly) |
|---|---|
| Easy | `easy` |
| Normal | `normal` |
| Hard | `hard` |

For each one:
- **Type:** Player Leaderboards.
- **Direction Method:** Descending (higher is better).
- **Enable Game API Writes:** **on**. Without it the game can't submit results.
- **Overwrite Score on Submit:** **off**, so a weaker result never replaces the best one.
- **Enable Metadata:** not needed.

Don't configure scheduled resets (Cron) or rewards.

## 3. Environment and key

Since March 2025 new LootLocker games have a single environment, and the console only issues a
`dev_…` key. It is the game's regular production key: real players use it too, there will be no
`prod_` key. LootLocker recommends a separate game for testing; for this project one game is enough,
test entries can be deleted in the console (Leaderboards → leaderboard → entry).

(Older games may have Stage and Live environments: then `dev_` is Stage, `prod_` is Live, and the
player build must use the Live key.)

## 4. Put the key into the project

**Game Settings → API**: copy the **Game Key** and the API address, which looks like
`https://xxxx.api.lootlocker.com`.

`src/pkjs/lootlocker_config.js` is not in git. The first `pebble build` creates it from
[lootlocker_config.example.js](../src/pkjs/lootlocker_config.example.js) (or copy it yourself).
Fill it in:

```js
module.exports = {
  gameKey: 'dev_...',                         // Game Key from the console
  domain: 'https://xxxx.api.lootlocker.com',  // address from the console, with or without https://
  gameVersion: '1.4.1'
};
```

The key is a client key and ships inside the `.pbw` anyway, but keeping it out of the repository
stops other builds of this source from writing to your leaderboards.

## 5. Verify

1. Build with `pebble build` and install on the watch through the Pebble app.
2. Settings → Name — set a name.
3. Menu → Leaders: should show `No records yet. Be the first!` (not `Server error`).
4. Play to Game Over with a new record → Select → should show `#1 of 1`.
5. In the LootLocker console: Leaderboards → the leaderboard → the entry is listed.

If `Server error` persists, check in order: Guest Login is enabled, leaderboard keys are exactly
`easy`/`normal`/`hard`, Enable Game API Writes is on, the key and API address are from the same
game.
