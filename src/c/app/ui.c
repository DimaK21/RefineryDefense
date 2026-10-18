#include "ui.h"
#include "ui_colors.h"

void ui_text(GContext *ctx, const char *txt, const char *font_key, GRect r, GTextAlignment align,
             GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, txt, fonts_get_system_font(font_key), r, GTextOverflowModeTrailingEllipsis,
                     align, NULL);
}

void ui_background(GContext *ctx, GRect bounds) {
  graphics_context_set_fill_color(ctx, UI_BG);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
}

void ui_header(GContext *ctx, GRect bounds, GColor band, const char *kicker, const char *title) {
  int w = bounds.size.w;
  graphics_context_set_fill_color(ctx, band);
  graphics_fill_rect(ctx, GRect(0, 0, w, UI_HEADER_H), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, UI_ACCENT);
  graphics_fill_rect(ctx, GRect(0, UI_HEADER_H, w, 2), 0, GCornerNone);
  if (kicker) {
    ui_text(ctx, kicker, FONT_KEY_GOTHIC_14_BOLD, GRect(0, 1, w, 16), GTextAlignmentCenter, UI_TEXT);
    ui_text(ctx, title, FONT_KEY_GOTHIC_24_BOLD, GRect(0, 12, w, 28), GTextAlignmentCenter, UI_TEXT);
  } else {
    ui_text(ctx, title, FONT_KEY_GOTHIC_28_BOLD, GRect(0, 4, w, 32), GTextAlignmentCenter, UI_TEXT);
  }
}

void ui_footer(GContext *ctx, GRect bounds, const char *hint) {
  ui_text(ctx, hint, FONT_KEY_GOTHIC_14_BOLD,
          GRect(0, bounds.size.h - UI_FOOTER_H + 2, bounds.size.w, 18), GTextAlignmentCenter, UI_MUTED);
}

void ui_button(GContext *ctx, GRect r, const char *label, bool selected, bool armed) {
  GColor fill = armed ? UI_BAD : selected ? UI_ACCENT : UI_CARD;
  GColor text = armed || !selected ? UI_TEXT : GColorBlack;
  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, r, 6, GCornersAll);
  graphics_context_set_stroke_color(ctx, selected || armed ? fill : UI_HDR_INFO);
  graphics_draw_round_rect(ctx, r, 6);
  ui_text(ctx, label, FONT_KEY_GOTHIC_18_BOLD, GRect(r.origin.x, r.origin.y + (r.size.h - 24) / 2,
          r.size.w, 22), GTextAlignmentCenter, text);
}

void ui_toggle(GContext *ctx, GRect r, bool on) {
  int knob = r.size.h / 2 - 2;
  graphics_context_set_fill_color(ctx, on ? UI_HDR_GOOD : GColorDarkGray);
  graphics_fill_rect(ctx, r, r.size.h / 2, GCornersAll);
  GPoint c = GPoint(on ? r.origin.x + r.size.w - r.size.h / 2 - 1 : r.origin.x + r.size.h / 2,
                    r.origin.y + r.size.h / 2);
  graphics_context_set_fill_color(ctx, on ? UI_TEXT : UI_MUTED);
  graphics_fill_circle(ctx, c, knob);
}

void ui_stat_row(GContext *ctx, GRect r, const char *label, const char *value, GColor value_color) {
  GRect t = GRect(r.origin.x, r.origin.y + (r.size.h - 24) / 2, r.size.w, 22);
  ui_text(ctx, label, FONT_KEY_GOTHIC_18, t, GTextAlignmentLeft, UI_MUTED);
  ui_text(ctx, value, FONT_KEY_GOTHIC_18_BOLD, t, GTextAlignmentRight, value_color);
  graphics_context_set_stroke_color(ctx, UI_HDR_INFO);
  int y = r.origin.y + r.size.h - 1;
  graphics_draw_line(ctx, GPoint(r.origin.x, y), GPoint(r.origin.x + r.size.w - 1, y));
}

/* ---- title art ---- */

static const GPoint STARS[] = {
  {14, 10}, {38, 30}, {61, 6}, {88, 22}, {120, 8}, {146, 28}, {171, 12}, {190, 34},
  {8, 52}, {176, 58}, {26, 74}, {192, 82}, {104, 64}, {52, 96}, {150, 100},
};

static void prv_rings(GContext *ctx, int x, int w, int top, int bottom) {
  graphics_context_set_stroke_color(ctx, UI_BG);
  for (int y = top + 6; y < bottom - 2; y += 7) graphics_draw_line(ctx, GPoint(x + 1, y), GPoint(x + w - 2, y));
}

void ui_skyline(GContext *ctx, GRect b) {
  int w = b.size.w, g = b.origin.y + b.size.h;   /* g: ground line */
  graphics_context_set_stroke_color(ctx, UI_MUTED);
  for (size_t i = 0; i < ARRAY_LENGTH(STARS); i++)
    if (STARS[i].y < b.size.h - 50) graphics_draw_pixel(ctx, GPoint(STARS[i].x, b.origin.y + STARS[i].y));

  graphics_context_set_fill_color(ctx, UI_SILHOUETTE);
  graphics_fill_rect(ctx, GRect(0, g - 5, w, 5), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(6, g - 20, 28, 16), 3, GCornersTop);       /* tank farm */
  graphics_fill_rect(ctx, GRect(36, g - 14, 20, 10), 3, GCornersTop);
  graphics_fill_rect(ctx, GRect(64, g - 48, 9, 44), 2, GCornersTop);       /* distillation columns */
  graphics_fill_rect(ctx, GRect(76, g - 36, 7, 32), 2, GCornersTop);
  prv_rings(ctx, 64, 9, g - 48, g - 4);
  prv_rings(ctx, 76, 7, g - 36, g - 4);
  graphics_fill_rect(ctx, GRect(86, g - 18, 36, 14), 0, GCornerNone);      /* cracking unit */
  graphics_fill_rect(ctx, GRect(110, g - 28, 6, 10), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(132, g - 44, 3, 40), 0, GCornerNone);      /* flare stack */
  graphics_fill_circle(ctx, GPoint(158, g - 17), 10);                      /* spherical tank */
  graphics_fill_rect(ctx, GRect(150, g - 10, 2, 6), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(164, g - 10, 2, 6), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(174, g - 16, 22, 12), 3, GCornersTop);
  graphics_context_set_stroke_color(ctx, UI_SILHOUETTE);                   /* pipe rack */
  graphics_draw_line(ctx, GPoint(34, g - 9), GPoint(64, g - 9));
  graphics_draw_line(ctx, GPoint(122, g - 12), GPoint(148, g - 12));

  graphics_context_set_fill_color(ctx, UI_FLAME);
  graphics_fill_circle(ctx, GPoint(133, g - 48), 3);
  graphics_context_set_fill_color(ctx, UI_ACCENT);
  graphics_fill_circle(ctx, GPoint(133, g - 47), 1);
}

int ui_hit(const GRect *rects, int n, GPoint p) {
  for (int i = 0; i < n; i++)
    if (grect_contains_point(&rects[i], &p)) return i;
  return -1;
}
