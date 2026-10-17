#include "feedback.h"
#include "../logic/drones.h"
#include "../logic/gesture.h"

#if defined(PBL_PLATFORM_EMERY)
#define TONE_GAP_MS 150

static TouchClock s_clock;
static bool s_toned;
static uint32_t s_last_tone_ms;
#endif

void feedback_events(uint32_t events) {
  if (events & EV_OBJECT_DESTROYED) vibes_long_pulse();
  else if (events & EV_OBJECT_HIT) vibes_short_pulse();
#if defined(PBL_PLATFORM_EMERY)
  /* Sample the clock on every call so its ms-field steps stay small (see touch_clock_ms). */
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  uint32_t now = touch_clock_ms(&s_clock, (uint32_t)s, ms);
  if (!(events & (EV_MISSILE | EV_EXPLOSION)) || speaker_is_muted()) return;
  if (s_toned && now - s_last_tone_ms < TONE_GAP_MS) return;
  if (events & EV_EXPLOSION) speaker_play_tone(110, 120, 60, SpeakerWaveformSawtooth);
  else speaker_play_tone(880, 60, 40, SpeakerWaveformSquare);
  s_toned = true;
  s_last_tone_ms = now;
#endif
}
