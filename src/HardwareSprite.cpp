#include "HardwareSprite.h"

int spriteCount = 0;
uint16_t* target = nullptr;  // to be sized in the main
uint8_t spriteLeds[NBSPRITE * SPRITE_HEIGHT * SPRITE_WIDTH * NB_COMPONENTSS];

HardwareSprite sprites[NBSPRITE];
