# I2SClocklessLedDriver for esp32
## Introduction
### What kinf of led wan you drive
This library is a new take on driving ws2812 leds with I2S on an esp32. It allows to drive up to 16 strips leds in parallel of  
* RGB:
    * WS2812,
    * WS2813,
    * WS2815 
* RGBW 
    * SK6812. 
I
f you are using RGB led type then this library is fully compatible with fastLED library (in which you cand find my previous version of  my I2S driver).

Why did I have rewritten the library ?

```C
#include "I2SClocklessLedDriver.h"

I2SClocklessLedDriver driver;
```


 `initled(uint8_t *leds,int * Pins,int num_strips,int num_led_per_strip,colorarrangment cArr)`
 This function initialize the strips.
 
 
 

