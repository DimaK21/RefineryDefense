#pragma once
#include "types.h"

bool     night_begin(Game *g);
void     sim_tick(Game *g, uint16_t dt_ms);
/* Night only: ready = off cooldown and affordable. ability_use pays, starts the cooldown and
 * the effect at p; false (nothing spent) if not ready. */
bool     ability_ready(const Game *g, Ability a);
bool     ability_use(Game *g, Ability a, Vec p);
uint32_t sim_take_events(Game *g);
uint16_t nights_survived(const Game *g);
void     report_ack(Game *g);
