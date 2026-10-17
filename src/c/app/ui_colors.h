#pragma once
#include <pebble.h>
#include "../logic/types.h"

#define COL_DAY_BG      GColorIslamicGreen
#define COL_GRID        GColorDarkGreen
#define COL_NIGHT_BG    GColorOxfordBlue
#define COL_RANGE       GColorMintGreen
#define COL_GHOST       GColorWhite
#define COL_SELECT      GColorYellow
#define COL_HUD_BG      GColorBlack
#define COL_HUD_TEXT    GColorWhite
#define COL_BAR_BG      GColorDarkGray
#define COL_BAR_SEL     GColorCobaltBlue
#define COL_BAR_TEXT    GColorWhite
#define COL_NO_MONEY    GColorRed
#define COL_DISABLED    GColorLightGray
#define COL_HP_FULL     GColorGreen
#define COL_HP_EMPTY    GColorRed
#define COL_DESTROYED   GColorDarkGray
#define COL_NOT_BUILT   GColorLightGray
#define COL_DRONE       GColorRed
#define COL_DECOY       GColorLightGray
#define COL_MISSILE     GColorYellow
#define COL_EXPLOSION   GColorOrange
#define COL_MINIMAP_BG  GColorDarkGreen
#define COL_VIEW_FRAME  GColorWhite

static inline GColor tower_color(int type) {
  switch (type) {
    case TW_MG:      return GColorLightGray;
    case TW_GEPARD:  return GColorChromeYellow;
    case TW_TOR:     return GColorVividCerulean;
    case TW_PATRIOT: return GColorBlue;
    case TW_JAMMER:  return GColorPurple;
    default:         return GColorWhite;   /* TW_RADAR */
  }
}

static inline char tower_letter(int type) {
  static const char letters[TW_COUNT] = {'M', 'G', 'T', 'P', 'E', 'R'};
  return (type >= 0 && type < TW_COUNT) ? letters[type] : '?';
}

static inline GColor object_color(int o) {
  switch (o) {
    case OBJ_DISTILLATION: return GColorWindsorTan;
    case OBJ_TANK_A:       return GColorLimerick;
    case OBJ_TANK_B:       return GColorLimerick;
    case OBJ_TERMINAL:     return GColorCadetBlue;
    default:               return GColorRoseVale;   /* OBJ_CRACKER */
  }
}
