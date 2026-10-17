#pragma once
#include "types.h"

bool     night_begin(Game *g);
void     sim_tick(Game *g, uint16_t dt_ms);
bool     air_support_call(Game *g, Vec p);
uint32_t sim_take_events(Game *g);
uint16_t nights_survived(const Game *g);
void     report_ack(Game *g);
