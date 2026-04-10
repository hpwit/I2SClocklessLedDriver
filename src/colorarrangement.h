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
 * The canonical initled() takes explicit offsetRed/offsetGreen/offsetBlue/offsetWhite/offsetWhite2 offsets instead.
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
 * On return, offsetWhite and offsetWhite2 are UINT8_MAX when the channel is absent.
 *
 * @param cArr          Colour byte order.
 * @param channelsPerLight  Output: bytes per pixel (3, 4, or 5).
 * @param offsetRed            Output: wire-order position of the Red channel.
 * @param offsetGreen            Output: wire-order position of the Green channel.
 * @param offsetBlue            Output: wire-order position of the Blue channel.
 * @param offsetWhite            Output: wire-order position of the White channel (UINT8_MAX = absent).
 * @param offsetWhite2           Output: wire-order position of the warm White channel (UINT8_MAX = absent).
 */
inline void applyColorArrangement(ColorArrangement cArr,
                                   uint8_t& channelsPerLight,
                                   uint8_t& offsetRed, uint8_t& offsetGreen, uint8_t& offsetBlue,
                                   uint8_t& offsetWhite, uint8_t& offsetWhite2) {
  offsetWhite  = UINT8_MAX;
  offsetWhite2 = UINT8_MAX;
  switch (cArr) {
  case ORDER_RGB:
    channelsPerLight = 3; offsetRed = 0; offsetGreen = 1; offsetBlue = 2;
    break;
  case ORDER_RBG:
    channelsPerLight = 3; offsetRed = 0; offsetGreen = 2; offsetBlue = 1;
    break;
  case ORDER_GRB:
    channelsPerLight = 3; offsetRed = 1; offsetGreen = 0; offsetBlue = 2;
    break;
  case ORDER_GBR:
    channelsPerLight = 3; offsetRed = 2; offsetGreen = 0; offsetBlue = 1;
    break;
  case ORDER_BRG:
    channelsPerLight = 3; offsetRed = 1; offsetGreen = 2; offsetBlue = 0;
    break;
  case ORDER_BGR:
    channelsPerLight = 3; offsetRed = 2; offsetGreen = 1; offsetBlue = 0;
    break;
  case ORDER_GRBW:
    channelsPerLight = 4; offsetRed = 1; offsetGreen = 0; offsetBlue = 2; offsetWhite = 3;
    break;
  case ORDER_RGBW:
    channelsPerLight = 4; offsetRed = 0; offsetGreen = 1; offsetBlue = 2; offsetWhite = 3;
    break;
  case ORDER_RGBCCT:
    channelsPerLight = 5; offsetRed = 0; offsetGreen = 1; offsetBlue = 2; offsetWhite = 3; offsetWhite2 = 4;
    break;
  }
}
