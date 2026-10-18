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
#define COL_HUD_MONEY   GColorYellow
#define COL_BAR_BG      GColorDarkGray
#define COL_BAR_SEL     GColorCobaltBlue
#define COL_BAR_TEXT    GColorWhite
#define COL_NO_MONEY    GColorRed
#define COL_DISABLED    GColorLightGray
#define COL_HP_FULL     GColorGreen
#define COL_HP_EMPTY    GColorRed
#define COL_DESTROYED   GColorDarkGray
#define COL_NOT_BUILT   GColorLightGray
#define COL_PAD         GColorDarkGray
#define COL_DRONE       GColorRed
#define COL_DECOY       GColorLightGray
#define COL_MISSILE     GColorYellow
#define COL_EXPLOSION   GColorOrange
#define COL_DRONE_TRAIL GColorDarkCandyAppleRed
#define COL_HARPY       GColorShockingPink
#define COL_MISSILE_TRAIL GColorLightGray
#define COL_AIR_MARK    GColorWhite
#define COL_EW_ZONE     GColorVividViolet
#define COL_FLARE_ZONE  GColorPastelYellow
#define COL_MINIMAP_BG  GColorDarkGreen
#define COL_VIEW_FRAME  GColorWhite

/* Menu, settings and summary screens */
#define UI_BG           GColorOxfordBlue
#define UI_CARD         GColorBlack
#define UI_ACCENT       GColorChromeYellow
#define UI_TEXT         GColorWhite
#define UI_MUTED        GColorLightGray
#define UI_GOOD         GColorScreaminGreen
#define UI_BAD          GColorSunsetOrange
#define UI_SILHOUETTE   GColorBlack
#define UI_FLAME        GColorOrange
#define UI_HDR_INFO     GColorCobaltBlue
#define UI_HDR_GOOD     GColorIslamicGreen
#define UI_HDR_BAD      GColorDarkCandyAppleRed

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

/* Plant health in the HUDs: green, then amber, then red. */
static inline GColor health_color(int pct) {
  return pct > 66 ? GColorScreaminGreen : pct > 33 ? GColorChromeYellow : GColorSunsetOrange;
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
