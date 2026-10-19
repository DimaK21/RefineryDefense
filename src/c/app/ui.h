#pragma once
#include <pebble.h>
#include "../logic/board.h"

/* Shared look of the menu, settings and summary screens. A round screen gets a taller header and
 * footer and UI_ROUND_INSET more side margin, so text stays where the circle is wide enough. */
#define UI_HEADER_H PBL_IF_ROUND_ELSE(54, 44)
#define UI_FOOTER_H PBL_IF_ROUND_ELSE(32, 22)
#define UI_ROUND_INSET PBL_IF_ROUND_ELSE(28, 0)

void ui_text(GContext *ctx, const char *txt, const char *font_key, GRect r, GTextAlignment align,
             GColor color);
void ui_background(GContext *ctx, GRect bounds);
/* Coloured band across the top: an optional small kicker line above a big title. */
void ui_header(GContext *ctx, GRect bounds, GColor band, const char *kicker, const char *title);
/* Muted hint centred in the bottom UI_FOOTER_H pixels. */
void ui_footer(GContext *ctx, GRect bounds, const char *hint);
void ui_button(GContext *ctx, GRect r, const char *label, bool selected, bool armed);
void ui_toggle(GContext *ctx, GRect r, bool on);
/* Label on the left, value on the right, hairline under the row. */
void ui_stat_row(GContext *ctx, GRect r, const char *label, const char *value, GColor value_color);
/* Night sky with stars and a refinery silhouette standing on the bottom edge of `bounds` (a little
 * above it on a round screen), centred. */
void ui_skyline(GContext *ctx, GRect bounds);
/* One leaderboard line in r (16 px high): rank, name, nights, score; `me` fills it with the accent. */
void ui_board_row(GContext *ctx, GRect r, const BoardRow *row, bool me);
/* Column titles matching ui_board_row, in r. */
void ui_board_header(GContext *ctx, GRect r);
/* Index of the first rect that contains p, or -1. */
int ui_hit(const GRect *rects, int n, GPoint p);
