#pragma once

#ifdef AUDIO_ENABLE
#	define STARTUP_SONG SONG(PREONIC_SOUND)

#	define GAME_SOUND \
		Q__NOTE(_G5), \
		Q__NOTE(_FS5),\
		Q__NOTE(_DS5),\
		Q__NOTE(_A4), \
		Q__NOTE(_GS4),\
		Q__NOTE(_E5), \
		Q__NOTE(_GS5),\
		HD_NOTE(_C6),

#	define DEFAULT_LAYER_SONGS { SONG(PREONIC_SOUND), \
                                 SONG(GAME_SOUND) \
                                }
#endif

// Key tapping settings.
#define TAPPING_TERM 200
#define TAPPING_TERM_PER_KEY
#define PERMISSIVE_HOLD
#define IGNORE_MOD_TAP_INTERRUPT

#define MUSIC_MASK (keycode != KC_NO)

// Mouse settings.
#define MOUSEKEY_INTERVAL 16
#define MOUSEKEY_DELAY 0
#define MOUSEKEY_TIME_TO_MAX 60
#define MOUSEKEY_MAX_SPEED 7
#define MOUSEKEY_WHEEL_DELAY 0

/*
 * MIDI options
 */

/* enable basic MIDI features:
   - MIDI notes can be sent when in Music mode is on
*/

#define MIDI_BASIC

/* enable advanced MIDI features:
   - MIDI notes can be added to the keymap
   - Octave shift and transpose
   - Virtual sustain, portamento, and modulation wheel
   - etc.
*/
//#define MIDI_ADVANCED

/* override number of MIDI tone keycodes (each octave adds 12 keycodes and allocates 12 bytes) */
//#define MIDI_TONE_KEYCODE_OCTAVES 2

// Enable unicode on linux.
#define UNICODE_SELECTED_MODES UC_LNX
