# I2SClocklessLedDriver for esp32
## Introduction
### What kind of led wan you drive
This library is a new take on driving ws2812 leds with I2S on an esp32. It allows to drive up to 16 strips leds in parallel of  
* RGB:
    * WS2812,
    * WS2813,
    * WS2815 
* RGBW 
    * SK6812. 

If you are using RGB led type then this library is fully compatible with fastLED library (in which you cand find my previous version of  my I2S driver).

### Why have I have rewritten the library ?
I have rewritten the library out of the FastLED framework  to allow easier testing but also create a pixel pusher independant of the library you want to use. Once totally done I will certainly re-merge it with the FasLED library.

But the main reason is the way I wanted to drive the leds. Indeed the library gives you two more options that can prove to be useful. One of the mode could reminder the older of us to some old school stuff.

## Let's start
### Array of strips
In most leds driver librairies you declare each strip attached to one pin, one line at a time.

example 1: For 4 strips in FastLED
```C
CRGB leds1[number_of_leds1];
CRGB leds2[number_of_leds2];
CRGB leds3[number_of_leds3];
CRGB leds4[number_of_leds4];

FasLED.addLeds<PIN1,ORDER>(leds1,number_of_leds1);
FasLED.addLeds<PIN2,ORDER>(leds2,number_of_leds2);
FasLED.addLeds<PIN3,ORDER>(leds3,number_of_leds3);
FasLED.addLeds<PIN4,ORDER>(leds4,number_of_leds4);
```

If you are using a large array of same length strips, you would do this:
```C
CRGB leds[4*NUM_LED_PER_STRIPS];
FasLED.addLeds<PIN1,ORDER>(leds,0,NUM_LED_PER_STRIPS);
FasLED.addLeds<PIN2,ORDER>(leds,NUM_LED_PER_STRIPS,NUM_LED_PER_STRIPS);
FasLED.addLeds<PIN3,ORDER>(leds,2*NUM_LED_PER_STRIPS,NUM_LED_PER_STRIPS);
FasLED.addLeds<PIN4,ORDER>(leds,3*NUM_LED_PER_STRIPS,NUM_LED_PER_STRIPS);
```

For information if you want to get (for development purpose ease) the leds1,leds2,...
```C
CRGB *leds1=leds;
CRGB *leds2=leds+NUM_LED_PER_STRIPS;
CRGB *leds3=leds+2*NUM_LED_PER_STRIPS;
CRGB *leds4=leds+3*NUM_LED_PER_STRIPS;
```

### Declare a new driver

```C
#include "I2SClocklessLedDriver.h"

I2SClocklessLedDriver driver;
```


 `initled(uint8_t *leds,int * Pins,int num_strips,int num_led_per_strip,colorarrangment cArr)`
 This function initialize the strips.
 
 
 

