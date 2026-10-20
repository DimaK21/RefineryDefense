# Refinery Defense — mechanics and rules

A reference for how the game works **right now**, taken from the code. If a number here disagrees
with the code, the code wins: every balance constant lives in
[config.h](../src/c/logic/config.h) and [config.c](../src/c/logic/config.c).

## In short

Tower defense for Pebble Time 2 (`emery`, 200×228, touch) and Pebble Round 2 (`gabbro`, round
260×260, touch). By day you build air defense around an oil refinery; by night you watch the drone
raid and can call in support abilities. The game is endless and ends when every refinery object is
destroyed. The record is the number of nights survived, kept separately per difficulty.

Loop: **Day → Night → Night report → Day of the next night …** → **Game Over**.

## Difficulty

Chosen on New Game and stored with the run. Numbers are in `DIFFICULTY_SPECS`
([config.c](../src/c/logic/config.c)).

| Level | Starting money | Wave budget |
|---|---|---|
| Easy | $600 | 70% |
| Normal | $500 | 100% |
| Hard | $500 | 160% |

All other rules are the same. Records (nights and score) are kept per level.

## Map

- The world is 200×228 units (= pixels of the night screen). Grid of 16×19 cells, 12 units each.
- By day the map is shown at ×2 with scrolling (24 px cells) and a minimap in the bottom-right
  corner. By night it is ×1 and the whole map fits on screen.
- On Round 2 the night field is centered in the circle at ×1; the empty corners are clipped by the
  circle. By day the tower bar and Play button sit at the bottom, and the camera is clamped to an
  inner rectangle so that edge cells can be brought into the visible part of the circle.
- A defense unit takes 1 cell, a refinery object 2×2. Nothing can be placed on an object.

## Refinery

| Object | Letter | Cell (col,row) | Income per night at 100% | Build cost | Role while alive |
|---|---|---|---|---|---|
| Distillation Column | D | 7,8 (center) | 30 | 180 | — |
| Tank Farm A | A | 3,12 | 20 | 120 | combo (see "Combo") |
| Tank Farm B | B | 11,4 | 20 | 120 | missiles 50% cheaper |
| Loading Terminal | L | 11,13 | 25 | 150 | units 15% cheaper: purchase and repair |
| Cracking Unit | C | 3,4 | 40 | 250 | abilities 30% cheaper and recharge 30% faster |

- Each object has 100 HP. States: not built (dashed outline with a letter), alive (object drawing
  with an HP bar), destroyed (gray with a cross).
- **Roles.** Every object except the center grants its role while alive (at any HP). A destroyed or
  unbuilt object grants nothing; Rebuild brings the role back. The role is checked at the moment of
  spending or of a kill: destruction mid-night applies immediately, cooldowns already running are not
  recalculated. Discounted prices round down, but never below $1. Constants are `OBJ_ROLE_*` and
  `ROLE_*_OFF_PCT` in [config.h](../src/c/logic/config.h); role labels are `ObjectSpec.role`.
- Game start: the Distillation Column and **one random** other object are built (drawn from the run
  RNG; night seeds are not shifted by it), no units, money by difficulty (on Normal **$500**: enough
  for a radar, two Gepards, two Tors and one more Gepard).
- Day actions (tap an object):
  - **Build** — full build cost.
  - **Repair** — to 100 HP: `cost × missing HP / 100` (rounded down).
  - **Rebuild** — restore a destroyed object: 150% of the build cost.
- More living objects means more income, but also bigger raids (see "Waves").

## Air defense units

| Type | Letter | Cost | Range | Reload | Damage | Ammo | HP | Missile |
|---|---|---|---|---|---|---|---|---|
| MG Nest | M | 30 | 22 | 0.25 s | 1 | ∞ | 3 | — |
| Gepard | G | 70 | 34 | 0.5 s | 2 | ∞ | 5 | — |
| Tor | T | 90 | 44 | 2 s | 6 | 6 | 3 | $5 |
| Patriot | P | 250 | 80 | 4 s | 20 | 3 | 6 | $30 |
| EW Jammer | E | 100 | 36 | — | — | — | 4 | — |
| Radar | R | 80 | 70 | — | — | — | 3 | — |

- **MG and Gepard** fire hitscan (tracer); damage applies instantly.
- **Tor and Patriot** launch a homing missile (60 units/s, hits within radius 4). The missile is
  spent on launch. At most 24 missiles can be in the air; if no slot is free, the unit waits. If the
  target dies first, the missile vanishes.
- **Ammo does not regenerate** — it is bought by day with **Reload**
  (`missing missiles × missile price`; with a living Tank Farm B missiles cost half: Tor $2,
  Patriot $15). An empty Tor/Patriot stays silent at night.
- **Reload all:** if any Tor/Patriot is short on missiles, a `Reload / $N` button appears in the
  bottom-left of the day map (N is the full sum, red if you can't afford it). Tapping it refills
  everyone. If money runs short, it adds one missile at a time to the unit with the lowest ammo
  share, skipping units whose missile is no longer affordable. If not even one is affordable — a
  short vibration. The button is hidden while an action panel is open, a unit is being moved, or the
  night start is armed.
- Unit prices in the bottom bar and repairs include the Loading Terminal discount (MG $25,
  Gepard $59, Tor $76, Patriot $212, Jammer $85, Radar $68). Sell always refunds 50% of the base
  price.
- **EW Jammer** doesn't shoot. A drone whose path passes within radius 36 flies 40% slower and its
  weave doubles.
- **Radar** doesn't shoot, but:
  - a unit standing inside a radar's coverage (cell center within radius 70) fires at **full**
    range; without radar — only **60%** of its range;
  - drones inside radar coverage are always visible and **identified** (see Gerbera).
- Day actions (tap a unit):
  - **Move** — to a free cell, free of charge.
  - **Sell** — a second tap on "Sure?" confirms; refunds 50% of the price (regardless of HP).
  - **Repair** — to full HP: `⌈cost × 30% × missing HP / max HP⌉`.
  - **Reload** — Tor/Patriot only.
- A unit at 0 HP is destroyed and removed — it can't be restored, only bought again.
- At most 40 units on the map.

### Target selection

Among drones within its current range, a unit shoots the one with the **least distance left** to its
target (the most dangerous one). Tor and Patriot skip identified Gerberas (no missiles wasted on
decoys); MG and Gepard shoot at everything.

## Night visibility

A drone is drawn only if it is inside any radar's coverage, a burning Flare, or the current range of
any shooting unit (60% or 100%, see above). Jammers don't grant visibility, and a radar only through
its coverage zone. The rest of the sky is dark. Explosions, tracers and missiles are always visible.

## Drones

| Type | Speed | HP | Object damage | Unit damage | Reward | From night | Threat |
|---|---|---|---|---|---|---|---|
| Switchblade | 14 | 2 | 10 | 1 | $3 | 1 | 2 |
| Shahed-136 | 8 | 6 | 35 | — | $8 | 2 | 4 |
| Gerbera | 9 | 2 | — | — | $1 | 3 | 1 |
| Harpy | 16 | 3 | 10 | 4 | $10 | 4 | 6 |
| Shahed-238 | 22 | 26 | 35 | — | $15 | 6 | 7 |

Speed is in units per second. "Threat" is how many wave-budget points one drone costs.

**Targets:**
- **Switchblade** — 20% chance to go for the nearest unit, otherwise a random living object.
  Arrives in swarms of 3–5, 0.3 s apart.
- **Shahed-136, Shahed-238** — a random living object.
- **Gerbera** — a decoy: flies to an object but does no damage on arrival. Outside radar it looks
  like a Shahed-136; inside radar it is a gray circle.
- **Harpy** — the nearest radar; no radars — the nearest unit; no units — an object.
- If the target disappears en route, the drone picks a new one. If there's nothing to attack, the
  drone vanishes.

**Flight:** spawns at a random point on the map edge (one of the wave's active sides). Flies through
two waypoints at 1/3 and 2/3 of the way, offset sideways by up to ±30 units, plus a sinusoidal weave
with amplitude 3–8 units. On reaching the target (within 4 units) it explodes and deals damage.

## Waves

- Threat budget of night N: `(10 + 2·N²) × (1 + 0.1 × max(0, living objects − 2))`, rounded down,
  then × the difficulty percentage (Easy 70%, Normal 100%, Hard 160%), rounded down again.
  Examples on Normal with 2 objects: night 1 → 12, 2 → 18, 3 → 28, 5 → 60, 10 → 210.
- Types are available from the night in the table and while the budget allows. With large budgets
  the generator prefers heavy drones to stay within the 200-drones-per-wave limit.
- Attack sides: nights 1–2 — one, 3–5 — two, 6+ — all four. They are shown in advance during the
  day: red double arrows at the map edges and a red strip on the matching minimap edges
  (`wave_sides` uses the same seed as `wave_generate`).
- All drones spawn within the first 40 s of the night. At most 64 drones can be in the sky at once;
  the rest wait for a free slot.
- A wave is determined by the night seed: replaying the same night gives the same raid.

## Night

- The simulation runs in 40 ms steps. **Select** — ×2 speed. **Back** — pause; while paused,
  **Back** exits to the menu, **Select** resumes. **Up/Down** — choose an ability. Losing focus (a
  notification) pauses the game.
- The night ends when every drone of the wave has spawned and the sky is empty.
- **Game Over** happens the moment no living object remains.

### Visual feedback

The game is designed to be played without sound, so everything important is visible on screen:

- A downed drone — a 4-phase explosion (flash, fireball, burning ring, fading ring) with flying
  debris, 0.4 s, and a floating yellow `+$N` reward label, 0.7 s.
- A drone reaching an object or unit — a large explosion with debris in all directions, 0.6 s.
- Night start — a "NIGHT N / Drones incoming" banner with the hint "Up/Down: ability · Tap map: aim"
  and attack-side arrows for the first 2.5 s.
- A drone crossing the map edge — a blinking red blip with an expanding ring at the edge, 1.2 s,
  even if the drone itself isn't visible yet.
- Drone silhouettes: Switchblade — a small cross, Shahed-136 — a red delta along its heading,
  Shahed-238 — a larger delta with a yellow exhaust, Harpy — a long pink dart, identified Gerbera —
  a gray ring (unidentified looks like a Shahed-136).
- MG and Gepard show a white muzzle flash along with the tracer; Tor/Patriot missiles leave a gray
  trail.
- An object at HP ≤ 66 burns (one flame and smoke), at HP ≤ 33 — two flames. A destroyed object
  smokes. At night the flames flicker; by day the frame is static.

### Abilities

Numbers are in `ABILITY_SPECS` ([config.c](../src/c/logic/config.c)).

| Ability | Cost | Cooldown | Radius | Effect |
|---|---|---|---|---|
| Air Strike | $60 | 25 s | 25 | instantly destroys every drone in the circle (with rewards) |
| EW Pulse | $25 | 12 s | 40 | for 5 s, drones whose path is in the circle fly 60% slower |
| Flare | $15 | 8 s | 45 | for 6 s the zone works as radar: drones are visible and identified, towers inside fire at full range |

- With a living Cracking Unit, cost and cooldown are 30% lower: Air Strike $42 / 17.5 s,
  EW Pulse $17 / 8.4 s, Flare $10 / 5.6 s.
- All abilities are ready at the start of the night; after use an ability goes on cooldown. All
  three take effect on confirmation; EW and Flare zones don't carry over past the end of the night.
- EW Pulse and Jammer don't stack: the strongest slowdown (60%) applies. Only the Jammer doubles the
  weave.
- **Up/Down** choose an ability. A tap on the map places a marker (a circle of its radius); a tap
  near the marker (within 20 px) fires it. The marker is gray if the ability is on cooldown or
  unaffordable — then confirming gives a short vibration and spends nothing. The marker disappears
  3 s after it was last placed.
- Under the marker (above it near the bottom edge) is a label: `Tap again: Air Strike $60`, or
  `Wait 12s` (cooldown), or `Need $60` (money). The price already includes the Cracking Unit
  discount.
- After Up/Down a card appears in the center of the screen for 2 s (real time, the game keeps
  running): a large icon, the name, what it does, cost and cooldown. Another Up/Down replaces it, a
  tap on the map hides it (and places a marker right away); it is hidden while paused.
- HUD: the selected ability name `AIR` / `EW` / `FLARE`, yellow if usable, gray if not; on cooldown
  the remaining seconds are shown next to it (`EW 7`).
- On the map: Air Strike — an expanding blast ring; EW Pulse — a purple double ring; Flare — a pale
  yellow ring with a star. Zones blink during their last second.
- Kill rewards are credited immediately and can be spent on abilities the same night.

### Combo

- Combo is the role of **Tank Farm A**: without a living Tank Farm A no streak is counted, the
  multiplier is always ×1, there's no COMBO label, and "Best combo" in the report is 0. If it is
  destroyed mid-streak, the next kill has no multiplier.
- A kill within 2 s of the previous one continues the streak; otherwise the streak restarts. Air
  Strike kills count too.
- The reward (money and score) is multiplied: streak 1–2 → ×1, 3–5 → ×1.5, 6+ → ×2, rounded down.
  The `+$N` above the drone shows the multiplied amount.
- With a streak ≥ 3, `COMBO N  x1.5` (yellow) or `x2` (orange) is shown below the HUD.

## Night report and money

- In the morning income is paid: `Σ object income × its HP / 100` over living objects.
- Score: the reward for every kill (with the combo multiplier) + 100 for each night survived.
- The report screen: kills, rewards, income, hits on objects, units lost, best streak.
- Record: more nights wins; on a tie, the higher score. Kept per difficulty; the Game Over screen
  shows the level in its header and that level's record.

## Online leaderboard

- Three LootLocker leaderboards, one per difficulty; ordering is the same as for the record (nights,
  then score). A single number is submitted: `nights × 1,000,000 + min(score, 999,999)`; the server
  keeps the player's best result.
- Name — up to 6 characters `A–Z`, `0–9`, space; set in Settings → Name or on the first submission.
  Up/Down — character (hold — fast), Select — next, hold Select — save, Back — back/cancel.
- Game Over with a new record: footer `Select: send · Tap: skip`. Submitting shows the rank
  `#N of M` and up to two rows above and below. If the server already has a better result — a
  `Your best online` row.
- Menu → Leaders: top 10 and your own row (below a dashed line if you're not in the top). Select/tap
  — next difficulty, Up/Down — scroll. If your local record is better than the submitted one — an
  `Unsent best` row, and holding Select submits it.
- The game only goes online for these actions. Errors: `No phone connection`, `No internet`,
  `Server error`; without a connection the game works as usual. Service setup —
  [LEADERBOARD_SETUP.md](LEADERBOARD_SETUP.md).

## Day controls

- Swipe — scroll the map.
- Bottom bar: 6 unit types with prices (red — not enough money) and a ▶ button.
  **Up/Down** — change the selected type.
- A tap on an empty cell shows a "ghost" of the unit with its range; a second tap on the same cell
  buys it. Not enough money — a short vibration.
- A tap on a unit or object opens its action panel. A tap outside the panel closes it.
- **▶ or Select** — the first press arms the night start ("Tap again to start night"), a second
  within 2 s starts the night. Any other tap cancels.
- **Back** — save and exit to the menu.

## Saving

- The run is stored in one persist key: night number, money, score, seed, objects, units,
  difficulty. Format version 2; a version 1 save (without difficulty) loads as Normal.
- All persist key numbers (save, records, settings) are defined in one place,
  [persist_keys.h](../src/c/app/persist_keys.h); a test checks that they don't collide.
- Records are a separate key, an array per difficulty. The old single record is treated as the
  Normal record on first read.
- Save points: new game, leaving the day, night start, night report.
- Quitting mid-night — **Continue** restarts that night with the same raid.
- Game Over erases the save. A corrupted save is erased when Continue is attempted.

## Settings

The **Settings** screen in the main menu. Stored separately from the run and survives Game Over.

| Setting | Default | What it does |
|---|---|---|
| Sound | off | Missile launch and explosion tones through the speaker. Off or system mute — silence. |
| Vibration | on | Short — a hit on an object and "not enough money"; long — an object destroyed. |

Also there: **How to play** — 9 short pages of rules for the player (goal, day, night, abilities,
units, drones, money, object roles). A tap on the right 2/3 of the screen or Down/Select — next page;
on the left third or Up — previous; after the last page the screen closes. The texts live in
[help_window.c](../src/c/app/help_window.c): if you change a mechanic, update them too.

## Screens

Menu (Continue · level / New Game / Leaders / Settings, night records `E · N · H` at the bottom).
New Game opens difficulty selection Easy / Normal / Hard (the selected level's record at the bottom,
Back — back) → Day → Night → Report → Day … → Game Over → Menu. If a save exists, New Game first
asks "Overwrite save?" — a second tap within 2.5 s starts a new game and erases the old run.

In the HUD money is yellow and refinery health is green (> 66%), amber (> 33%) or red. The report
and Game Over screens ignore input for the first 0.6 s so that a stray tap from the night doesn't
skip them. If touch is disabled in the system, a screen asks to enable it.

## Balance check

[test_balance.c](../tests/test_balance.c) plays 60 seeds with a bot (it starts with a random second
object like a player; each morning it repairs objects below 60 HP, refills missiles with "Reload
all", buys a radar and places Gepards/Tors in a ring around living objects; at night every 0.4 s it
drops Air Strike and EW Pulse on clusters of 3+ visible drones) and as many with an "empty" player
who builds nothing. The bot's outcome is bimodal: a seed either loses by nights 4–8 or grows into a
wall at nights 16–21, where cells near the objects run out. So the test checks stable quantities
rather than the median:

- mean nights 11–20;
- lower quartile ≥ 6 (three quarters of seeds survive night 6, when Shahed-238s arrive from four
  sides);
- at most 5% of seeds lost on night 5 or earlier;
- no seed reaches 60 nights, and the empty player loses on nights 1–3;
- for each of the four starting objects the mean is within 3 nights of the overall mean.

For Easy and Hard the same bot checks only the ordering: the Easy mean is at least 3 nights above
Normal, Hard at least 3 below; on Easy at most 5% of seeds are lost by night 5, on Hard at most 10%;
the empty player loses by night 5 on Easy and by night 3 on Hard.

Reference numbers on 60 seeds: Normal — mean 17.1, quartile 17, median 19, 2 of 60 lost by night 5,
16.4–18.3 depending on the starting object; Easy — mean 23.4, quartile 22; Hard — mean 10.1,
quartile 7, median 9, 1 of 60 lost by night 5.
