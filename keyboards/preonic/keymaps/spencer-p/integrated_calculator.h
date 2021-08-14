#pragma once

enum igcalc_keycodes {
  IGCALC = SAFE_RANGE,
  SAFE_RANGE2
};

bool process_integrated_calculator(uint16_t keycode, keyrecord_t *record);
