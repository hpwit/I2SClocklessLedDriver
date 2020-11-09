# I2SClocklessLedDriver for esp32
## Introduction
This library is a new take on driving ws2812 leds with I2S on an esp32.
It allows

```C
#include "I2SClocklessLedDriver.h"

I2SClocklessLedDriver driver;
```


 `initled(uint8_t *leds,int * Pins,int num_strips,int num_led_per_strip,colorarrangment cArr)`

