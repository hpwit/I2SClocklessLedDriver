/**
    @title     I2SClocklessLedDriver
    @file      colorarrangement.h
    @repo      https://github.com/hpwit/I2SClocklessLedDriver
    @Copyright © 2025 Yves Bazin
    @license   MIT License

    Colour-order convenience layer.  This file is intentionally separate from
    the hardware driver so the switch-case decoder does not live next to register
    code.  All initled() overloads that accept a ColorArrangement enum call
    applyColorArrangement() and then forward to the canonical initled().
**/

#pragma once

#include <stdint.h>

/**
 * ColorArrangement — wire order of colour channels.
 *
 * Pass one of these values to the convenience initled() overloads.
 * The canonical initled() takes explicit pR/pG/pB/pW/pW2 offsets instead.
 */
enum ColorArrangement {
  ORDER_GRBW,
  ORDER_RGB,
  ORDER_RBG,
  ORDER_GRB,
  ORDER_GBR,
  ORDER_BRG,
  ORDER_BGR,
  ORDER_RGBW,
  ORDER_RGBCCT,
};

/**
 * applyColorArrangement — decode a ColorArrangement enum into explicit
 * per-channel byte offsets used by the canonical initled().
 *
 * On return, pW and pW2 are UINT8_MAX when the channel is absent.
 *
 * @param cArr          Colour byte order.
 * @param nbComponents  Output: bytes per pixel (3, 4, or 5).
 * @param pR            Output: wire-order position of the Red channel.
 * @param pG            Output: wire-order position of the Green channel.
 * @param pB            Output: wire-order position of the Blue channel.
 * @param pW            Output: wire-order position of the White channel (UINT8_MAX = absent).
 * @param pW2           Output: wire-order position of the warm White channel (UINT8_MAX = absent).
 */
inline void applyColorArrangement(ColorArrangement cArr,
                                   uint8_t& nbComponents,
                                   uint8_t& pR, uint8_t& pG, uint8_t& pB,
                                   uint8_t& pW, uint8_t& pW2) {
  pW  = UINT8_MAX;
  pW2 = UINT8_MAX;
  switch (cArr) {
  case ORDER_RGB:
    nbComponents = 3; pR = 0; pG = 1; pB = 2;
    break;
  case ORDER_RBG:
    nbComponents = 3; pR = 0; pG = 2; pB = 1;
    break;
  case ORDER_GRB:
    nbComponents = 3; pR = 1; pG = 0; pB = 2;
    break;
  case ORDER_GBR:
    nbComponents = 3; pR = 2; pG = 0; pB = 1;
    break;
  case ORDER_BRG:
    nbComponents = 3; pR = 1; pG = 2; pB = 0;
    break;
  case ORDER_BGR:
    nbComponents = 3; pR = 2; pG = 1; pB = 0;
    break;
  case ORDER_GRBW:
    nbComponents = 4; pR = 1; pG = 0; pB = 2; pW = 3;
    break;
  case ORDER_RGBW:
    nbComponents = 4; pR = 0; pG = 1; pB = 2; pW = 3;
    break;
  case ORDER_RGBCCT:
    nbComponents = 5; pR = 0; pG = 1; pB = 2; pW = 3; pW2 = 4;
    break;
  }
}
