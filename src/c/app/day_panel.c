#include "day_panel.h"
#include "feedback.h"
#include "ui_colors.h"
#include "../logic/config.h"
#include "../logic/economy.h"

#define MAX_BTNS 5
#define CLOSE_W 40

typedef enum { BTN_MOVE, BTN_SELL, BTN_REPAIR, BTN_RELOAD, BTN_OBJECT, BTN_CLOSE } BtnAct;
typedef struct { BtnAct act; const char *top; char bottom[16]; int16_t w; bool enabled, poor; } Btn;

static PanelKind s_kind;
static int s_idx;
static bool s_sell_armed;   /* first Sell tap asks "Sure?", the second one sells */

void day_panel_open(PanelKind kind, int idx) { s_kind = kind; s_idx = idx; s_sell_armed = false; }
void day_panel_close(void) { s_kind = PANEL_NONE; s_sell_armed = false; }
PanelKind day_panel_kind(void) { return s_kind; }
int day_panel_index(void) { return s_idx; }

static void prv_cost_btn(Btn *b, BtnAct act, const char *top, int32_t cost, int32_t money, int16_t w) {
  b->act = act; b->top = top; b->w = w;
  b->enabled = cost > 0;
  b->poor = cost > money;
  if (cost > 0) snprintf(b->bottom, sizeof(b->bottom), "$%ld", (long)cost);
  else snprintf(b->bottom, sizeof(b->bottom), "-");
}

static int prv_buttons(const Game *g, int16_t width, Btn *out) {
  int n = 0;
  if (s_kind == PANEL_TOWER) {
    int16_t w = width / MAX_BTNS;
    out[n] = (Btn){BTN_MOVE, "Move", "", w, true, false}; n++;
    out[n] = (Btn){BTN_SELL, s_sell_armed ? "Sure?" : "Sell", "", w, true, false};
    snprintf(out[n].bottom, sizeof(out[n].bottom), "+$%ld", (long)tower_sell_value(g, s_idx)); n++;
    prv_cost_btn(&out[n++], BTN_REPAIR, "Repair", tower_repair_cost(g, s_idx), g->money, w);
    prv_cost_btn(&out[n++], BTN_RELOAD, "Reload", tower_reload_cost(g, s_idx), g->money, w);
    out[n] = (Btn){BTN_CLOSE, "X", "", width - 4 * w, true, false}; n++;
  } else if (s_kind == PANEL_OBJECT) {
    const char *verb = g->objects[s_idx].status == OBJ_NOT_BUILT ? "Build"
                     : g->objects[s_idx].status == OBJ_DESTROYED ? "Rebuild" : "Repair";
    int32_t cost = object_action_cost(g, (ObjectId)s_idx);
    const char *name = OBJECT_SPECS[s_idx].name, *role = OBJECT_SPECS[s_idx].role;
    Btn *b = &out[n++];
    b->act = BTN_OBJECT; b->w = width - CLOSE_W;
    if (cost <= 0) {
      /* intact: nothing to buy, so name and role; a tap does nothing (object_action is ACT_INVALID) */
      b->top = name; b->enabled = true; b->poor = false;
      snprintf(b->bottom, sizeof(b->bottom), "%s", role ? role : "");
    } else {
      /* name and cost do not fit: the role tells units apart, the centre has none and keeps its name */
      b->top = role ? role : name; b->enabled = true; b->poor = cost > g->money;
      snprintf(b->bottom, sizeof(b->bottom), "%s $%ld", verb, (long)cost);
    }
    out[n] = (Btn){BTN_CLOSE, "X", "", CLOSE_W, true, false}; n++;
  }
  return n;
}

void day_panel_draw(GContext *ctx, GRect area, const Game *g) {
  Btn btns[MAX_BTNS];
  int n = prv_buttons(g, area.size.w, btns);
  graphics_context_set_fill_color(ctx, COL_BAR_BG);
  graphics_fill_rect(ctx, area, 0, GCornerNone);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  int16_t x = area.origin.x;
  for (int i = 0; i < n; i++) {
    const Btn *b = &btns[i];
    GRect r = GRect(x, area.origin.y, b->w, area.size.h);
    if (b->act == BTN_SELL && s_sell_armed) {
      graphics_context_set_fill_color(ctx, COL_NO_MONEY);
      graphics_fill_rect(ctx, r, 0, GCornerNone);
    }
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_rect(ctx, r);
    GColor top = b->enabled ? COL_BAR_TEXT : COL_DISABLED;
    GColor bottom = !b->enabled ? COL_DISABLED : b->poor ? COL_NO_MONEY : COL_BAR_TEXT;
    bool one_line = b->bottom[0] == 0;
    graphics_context_set_text_color(ctx, top);
    graphics_draw_text(ctx, b->top, font,
                       GRect(r.origin.x, r.origin.y + (one_line ? 4 : -2), r.size.w, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    if (!one_line) {
      graphics_context_set_text_color(ctx, bottom);
      graphics_draw_text(ctx, b->bottom, font, GRect(r.origin.x, r.origin.y + 11, r.size.w, 16),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
    x += b->w;
  }
}

/* Repair/Reload/Build keep the panel open so the updated cost (now greyed) is visible. */
static PanelResult prv_stay(ActResult r) {
  if (r == ACT_NO_MONEY) feedback_short();
  return PANEL_RES_NONE;
}

PanelResult day_panel_tap(Game *g, GRect area, int16_t x, int16_t y) {
  Btn btns[MAX_BTNS];
  int n = prv_buttons(g, area.size.w, btns);
  int16_t bx = area.origin.x;
  for (int i = 0; i < n; i++) {
    const Btn *b = &btns[i];
    if (x >= bx && x < bx + b->w) {
      bool confirm = b->act == BTN_SELL && s_sell_armed;
      s_sell_armed = false;   /* any other tap disarms */
      if (!b->enabled) return PANEL_RES_NONE;
      switch (b->act) {
        case BTN_MOVE:   day_panel_close(); return PANEL_RES_MOVE;
        case BTN_CLOSE:  day_panel_close(); return PANEL_RES_CLOSED;
        case BTN_SELL:
          if (!confirm) { s_sell_armed = true; return PANEL_RES_NONE; }
          if (tower_sell(g, s_idx) != ACT_OK) return PANEL_RES_NONE;
          day_panel_close();
          return PANEL_RES_CLOSED;
        case BTN_REPAIR: return prv_stay(tower_repair(g, s_idx));
        case BTN_RELOAD: return prv_stay(tower_reload(g, s_idx));
        case BTN_OBJECT: return prv_stay(object_action(g, (ObjectId)s_idx));
      }
    }
    bx += b->w;
  }
  s_sell_armed = false;
  return PANEL_RES_NONE;
}
