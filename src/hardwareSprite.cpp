#include "hardwareSprite.h"

int _spritenumber = 0;
uint16_t* target = nullptr;  // to be sized in the main
uint8_t _spritesleds[NBSPRITE * SPRITE_HEIGHT * SPRITE_WIDTH * nb_componentss];

hardwareSprite sprites[NBSPRITE];
