/* Copyright 2021 Spencer Peterson
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H
#include <stdio.h>
#include <string.h>
#include "muse.h"
#include "integrated_calculator.h"

#ifndef IGCALC_BUFSIZE
#   define IGCALC_BUFSIZE 128
#endif

#ifndef IGCALC_RESULT_BUFSIZE
#   define IGCALC_RESULT_BUFSIZE 16
#endif

#ifndef NAN
#   define NAN (0.0f / 0.0f)
#endif

struct node {
  bool is_value;
  union {
    int8_t token;
    double value;
  };
};

struct executor {
  char *text;
  size_t len;
  size_t index;

  int precision;

  bool has_pushback;
  struct node pushback;
};

float error_sound[][2] = SONG(Q__NOTE(_DS4), Q__NOTE(_C4));
float enable_sound[][2] = SONG(NUM_LOCK_ON_SOUND);

void integrated_calculator_compute(char *, size_t);
void integrated_calculator_reset(void);
char kc_to_char(uint16_t keycode);
double expression(struct executor *s);

char buf[IGCALC_BUFSIZE] = {0};
size_t buf_len = 0;
bool enabled = false;
bool engaged = false; // Records if we started to press KC_EQL.

bool process_integrated_calculator(uint16_t keycode, keyrecord_t *record) {
  // Check if user is trying to enter calculator mode.
  if (keycode == IGCALC && record->event.pressed) {
    enabled = true;
    PLAY_SONG(enable_sound);
    return false;
  }

  // If the user is not in calculator mode, nothing to do.
  if (!enabled) {
    return true;
  }

  // Get modifiers so we can detect shift.
  uint8_t mods = get_mods();
  bool shift = mods & MOD_MASK_SHIFT;

  // If there are modifiers on that aren't shift, just ignore them.
  if (mods && !shift) {
    return true;
  }

  // Rewrite the keycode if shift is pressed down.
  if (shift) switch (keycode) {
    case KC_EQL:
      keycode = KC_PLUS;
      break;
    case KC_8:
      keycode = KC_ASTR;
      break;
    case KC_9:
      keycode = KC_LPRN;
      break;
    case KC_0:
      keycode = KC_RPRN;
      break;

    // All of these keycodes should not be stored if shift is pressed; it's just
    // a different key.
    case KC_1:
    case KC_2:
    case KC_3:
    case KC_4:
    case KC_5:
    case KC_6:
    case KC_7:
    case KC_MINUS:
    case KC_SLASH:
    case KC_DOT:
      return true;
      break;
  } 

  switch (keycode) {
    // User can press escape to cancel the calculator mode.
    case KC_ESC:
      integrated_calculator_reset();
      return true;
      break;

    // Intercept and record keystrokes that might be part of an expression.
    // No further action is taken; processing should continue after.
    case KC_1:
    case KC_2:
    case KC_3:
    case KC_4:
    case KC_5:
    case KC_6:
    case KC_7:
    case KC_8:
    case KC_9:
    case KC_0:
    case KC_RPRN:
    case KC_LPRN:
    case KC_PLUS:
    case KC_MINUS:
    case KC_ASTR:
    case KC_SLASH:
    case KC_DOT:
      if (record->event.pressed) {
        if (buf_len >= IGCALC_BUFSIZE) {
          // Too many keys; no more :(
          PLAY_SONG(error_sound);
          return false;
        }
        buf[buf_len] = kc_to_char(keycode);
        buf_len += 1;
      }
      return true;
      break;

    case KC_BSPC:
      if (record->event.pressed && buf_len > 0) {
        buf_len -= 1;
      }
      return true;
      break;

    // Prevent user from moving the cursor.
    // We can't completely control this, but we can try to help avoid
    // strangeness.
    case KC_LEFT:
    case KC_DOWN:
    case KC_UP:
    case KC_RIGHT:
      if (record->event.pressed) {
        PLAY_SONG(error_sound);
      }
      return false;
      break;

    // Compute and send result on =.
    // We want to actually do it on = release, and only if = was previously
    // pressed (excluding = with Shift, which is really +).
    // Note that we only reach this case when shift is *not* pressed.
    case KC_EQL:
      if (engaged && !record->event.pressed) {
        // Immediately disengage and disable to prevent potential races.
        enabled = false;
        engaged = false;
        integrated_calculator_compute(buf, buf_len);
        integrated_calculator_reset();
        return false;
      } else if (record->event.pressed) {
        engaged = true;
        return true;
      }
      break;

  }
  // Key not relevant for the integrated calculator; skip it.
  return true;
}

void integrated_calculator_compute(char *buf, size_t buf_len) {
  static char result_str[IGCALC_RESULT_BUFSIZE];
  struct executor ex = {
    .text = buf,
    .len = buf_len,
    .has_pushback = false,
    .precision = 0,
  };
  double result = expression(&ex);

  // Unfortunately, at the moment, I don't have a good way to format a floating
  // point number. So I have to settle with this:
  //  - multiply by 10**precicion
  //  - cast to int, removing everything past the (new) decimal
  //  - print the int with a dot in the right place.
  //  This could introduce inaccuracy for large numbers.
  for (int i = 0; i < ex.precision; i++) {
    result *= 10;
  }
  int result_len = snprintf(result_str, IGCALC_RESULT_BUFSIZE, "%d", (int)result);
  int decimal_index = result_len - ex.precision - 1;

#ifdef RESTATE_THE_QUESTION
  send_char('\n');
  for (size_t i = 0; i < buf_len; i++) {
    send_char(buf[i]);
  }
  send_char('=');
#endif

  // If we printed all of the precision, then we must be missing a 0.
  if (result_len <= ex.precision) {
    send_char('0');
    send_char('.');
    for (int i = result_len+1; i <= ex.precision; i++) {
      send_char('0');
    }
  }

  for (int i = 0; i < result_len; i++) {
    send_char(result_str[i]);
    if (i == decimal_index && i+1 != result_len) {
      send_char('.');
    }
  }
}

char kc_to_char(uint16_t keycode) {
  switch (keycode) {
    case KC_1: return '1';
    case KC_2: return '2';
    case KC_3: return '3';
    case KC_4: return '4';
    case KC_5: return '5';
    case KC_6: return '6';
    case KC_7: return '7';
    case KC_8: return '8';
    case KC_9: return '9';
    case KC_0: return '0';
    case KC_RPRN: return ')';
    case KC_LPRN: return '(';
    case KC_PLUS: return '+';
    case KC_MINUS: return '-';
    case KC_ASTR: return '*';
    case KC_SLASH: return '/';
    case KC_DOT: return '.';
    default: return '\0';
  }
}

void integrated_calculator_reset() {
  memset(buf, '\0', IGCALC_BUFSIZE);
  buf_len = 0;
  enabled = false;
  engaged = false;
}

int get_precision(char *start, char *end) {
  for (char *cur = start; cur < end; cur++) {
    if (*cur == '.') {
      return end-cur-1;
    }
  }
  return 0;
}

struct node lex(struct executor *s) {
  struct node result = {0};
  if (s->index >= s->len || s->text[s->index] == '\0') {
    result.token = EOF;
    return result;
  }

  switch (s->text[s->index]) {
    case '+':
    case '-':
    case '*':
    case '/':
    case '(':
    case ')':
      result.token = s->text[s->index];
      s->index += 1;
      return result;
  }

  result.is_value = true;
  char *end, *start = s->text+s->index;
  result.value = strtod(start, &end);
  s->index += end-start;

  int precision = get_precision(start, end);
  if (precision > s->precision) {
    s->precision = precision;
  }

  return result;
}

struct node get_token(struct executor *s) {
  if (s->has_pushback) {
    s->has_pushback = false;
    return s->pushback;
  }
  return lex(s);
}

void pushback(struct executor *s, struct node n) {
  s->has_pushback = true;
  s->pushback = n;
}

double add(struct executor *s);
double unit(struct executor *s) {
  struct node n = get_token(s);
  if (n.is_value) {
    return n.value;
  }

  if (n.token == '-') {
    double val = unit(s);
    return -val;
  }
  if (n.token == '(') {
    double val = add(s);
    n = get_token(s);
    if (!(!n.is_value && n.token == ')')) {
      // User mismatched parenthesis.
      return NAN;
    }
    return val;
  }

  // Something not in the syntax.
  return NAN;
}

double mul(struct executor *s) {
  double left = unit(s);

  double right;
  struct node n;
  while (true) {
    n = get_token(s);
    if (!n.is_value) {
      if (n.token == '*') {
        right = unit(s);
        left = left*right;
      } else if (n.token == '/') {
        right = unit(s);
        left = left/right;
      } else {
        pushback(s, n);
        return left;
      }
    } else {
      // Value not expected here.
      return NAN;
    }
  }
}

double add(struct executor *s) {
  double left = mul(s);

  double right;
  struct node n;
  while (true) {
    n = get_token(s);
    if (!n.is_value) {
      if (n.token == '+') {
        right = mul(s);
        left = left+right;
      } else if (n.token == '-') {
        right = mul(s);
        left = left-right;
      } else {
        pushback(s, n);
        return left;
      }
    } else {
      // Value not expected here.
      return NAN;
    }
  }
}

double expression(struct executor *s) {
  double result = add(s);
  struct node n = get_token(s);
  if (!(!n.is_value && n.token == EOF)) {
    // The last token should always be EOF.
    // If it's not, then there's an error somewhere.
    return NAN;
  }
  return result;
}

// vim: ts=2 sw=2 et
