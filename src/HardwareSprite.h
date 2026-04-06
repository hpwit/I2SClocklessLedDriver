#ifndef HARDWARESPRITES
  #define HARDWARESPRITES 0
#endif

#pragma once

#if HARDWARESPRITES == 1
#include "FastLED.h"

#include <cstdint>

#ifndef NBSPRITE
  #define NBSPRITE 8
#endif
#ifndef SPRITE_WIDTH
  #define SPRITE_WIDTH 20
#endif
#ifndef SPRITE_HEIGHT
  #define SPRITE_HEIGHT 20
#endif
#ifndef NB_COMPONENTSS
  #define NB_COMPONENTSS 3
#endif

extern int spriteCount;
extern uint16_t* target;  // to be sized in the main
extern uint8_t spriteLeds[NBSPRITE * SPRITE_HEIGHT * SPRITE_WIDTH * NB_COMPONENTSS];

/**
 * HardwareSprite — a fixed-size sprite that composites into the driver's
 * transposed DMA buffer via reorder().
 *
 * Each instance occupies a slice of the global spriteLeds[] array.
 * At most NBSPRITE instances may be constructed; the constructor silently
 * sets leds = nullptr if that limit is exceeded.
 * Set displaySprite = true and call reorder(panelWidth, panelHeight) each
 * frame to blend the sprite into the target buffer.
 */
class HardwareSprite {
 public:
  HardwareSprite() noexcept {
    if (spriteCount >= NBSPRITE) {
      displaySprite = false;
      leds = nullptr;
      spritenumber = -1;
      return;
    }
    displaySprite = false;
    leds = reinterpret_cast<CRGB*>(&spriteLeds[spriteCount * SPRITE_WIDTH * SPRITE_HEIGHT * NB_COMPONENTSS]);
    spritenumber = spriteCount;
    spriteCount++;
  };
  bool displaySprite;
  int spritenumber;
  CRGB transparentColor = CRGB(0, 0, 0);
  int posX = 0;
  int posY = 0;

  int offset(int x, int y, int width, int height) {
    if ((posX + x) >= width || (posX + x) < 0 || (posY + y) >= height || (posY + y) < 0) {
      // Serial.printf("%d %d,%d %d ",x,y,posX+x,posY+y);
      // Serial.println("out");
      return -1;
    }
    // Serial.println("ok");
#if SNAKEPATTERN == 1
    if ((posY + y) % 2 == 0) {
      return (posY + y) * width + x + posX;
    } else {
      return width * (y + posY + 1) - 1 - (x + posX);
    }
#else
    return (y + posY) * width + x + posX;

#endif
  }
  void setTransparentColor(CRGB color) {
    if (leds == nullptr) return;
    transparentColor = color;
    for (int i = 0; i < SPRITE_WIDTH * SPRITE_HEIGHT; i++) {
      leds[i] = color;
    }
  }

  // Composites this sprite into the target buffer.
  // Precondition: `target` must point to a buffer of at least `width * height`
  // uint16_t elements. This is the caller's responsibility; no size validation
  // is performed here since `target` carries no associated size metadata.
  void reorder(int width, int height) {
    if (displaySprite && leds != nullptr && target != nullptr) {
      const int bufferSize = width * height;
      for (int i = 0; i < SPRITE_WIDTH; i++) {
        for (int j = 0; j < SPRITE_HEIGHT; j++) {
          if (leds[j * SPRITE_WIDTH + i] != transparentColor) {
            int pixelOffset = offset(i, j, width, height);
            if (pixelOffset >= 0 && pixelOffset < bufferSize) target[pixelOffset] = (uint16_t)(((j * SPRITE_WIDTH + i) + spritenumber * SPRITE_WIDTH * SPRITE_HEIGHT) * NB_COMPONENTSS + 1);
            // else
            //   Serial.printf("%d %d out\n",i,j);
            // lednumber[j * WIDTH + i] = offset(i, j, width, height);
            // Serial.printf("%d %d %d\n",i,j,lednumber[j * WIDTH + i]);
            //  _led[j * WIDTH + i] = leds[j * WIDTH + i];
          }
        }
      }
    }
  }

  CRGB* leds;
};

extern HardwareSprite sprites[NBSPRITE];
#endif