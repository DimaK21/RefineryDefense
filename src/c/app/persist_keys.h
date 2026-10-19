#pragma once

/* Every persist key of the app in one place: two files picking the same number silently share it. */
#define KEY_OLD_HDR 1      /* pre-single-key layout: deleted on save */
#define KEY_OLD_TOWERS 2
#define KEY_OLD_BEST 3     /* one record from before difficulties: becomes Normal's */
#define KEY_SAVE 4         /* header + towers in one key: written atomically */
#define KEY_BESTS 5        /* Best per difficulty */
#define KEY_VIBE 6
#define KEY_SOUND 7         /* was 5 up to 1.3.0, colliding with KEY_BESTS: the old value is not migrated */

#define PERSIST_KEYS {KEY_OLD_HDR, KEY_OLD_TOWERS, KEY_OLD_BEST, KEY_SAVE, KEY_BESTS, KEY_VIBE, KEY_SOUND}
