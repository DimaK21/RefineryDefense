#pragma once
#include <pebble.h>
#include "../logic/types.h"

/* Action panel that replaces the bottom bar after a tap on a tower or a plant object. */
typedef enum { PANEL_NONE, PANEL_TOWER, PANEL_OBJECT } PanelKind;
typedef enum { PANEL_RES_NONE, PANEL_RES_CLOSED, PANEL_RES_MOVE } PanelResult;

void      day_panel_open(PanelKind kind, int idx);
void      day_panel_close(void);
PanelKind day_panel_kind(void);
int       day_panel_index(void);
void      day_panel_draw(GContext *ctx, GRect area, const Game *g);
/* Tap at screen (x, y) inside `area`. PANEL_RES_MOVE: caller enters move mode for the tower. */
PanelResult day_panel_tap(Game *g, GRect area, int16_t x, int16_t y);
