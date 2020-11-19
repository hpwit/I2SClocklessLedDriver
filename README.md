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

### Why have I rewritten the library ?
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

example 2: If you are using a large array of same length strips, you would do this:
```C
CRGB leds[4*NUM_LED_PER_STRIPS];
FasLED.addLeds<PIN1,ORDER>(leds,0,NUM_LED_PER_STRIPS);
FasLED.addLeds<PIN2,ORDER>(leds,NUM_LED_PER_STRIPS,NUM_LED_PER_STRIPS);
FasLED.addLeds<PIN3,ORDER>(leds,2*NUM_LED_PER_STRIPS,NUM_LED_PER_STRIPS);
FasLED.addLeds<PIN4,ORDER>(leds,3*NUM_LED_PER_STRIPS,NUM_LED_PER_STRIPS);
```

example 3: For information if you want to get (for development purpose ease) the leds1,leds2,...
```C
CRGB *leds1=leds;
CRGB *leds2=leds+NUM_LED_PER_STRIPS;
CRGB *leds3=leds+2*NUM_LED_PER_STRIPS;
CRGB *leds4=leds+3*NUM_LED_PER_STRIPS;
```

If all the strips of the first example are of the same size, then the 2 examples are the doing exactly the same. Hence when using strips of different lengths we cannot put them in a  big array ? **FALSE**. You cant create a large array when using `NUM_LED_PER_STRIP` being the largest of `number_of_leds`. Of course you array woul be larger than you actual numbre of leds but we can do with the lost of space.

### OK, but what is the link between arry of strips and this driver ?
Here is how we would declare the 4 strips in of our example:
```C
CRGB leds[4*NUM_LED_PER_STRIPS];
int pins[4]={PIN1,PIN2,PIN3,PIN4};
driver.initled((uint8_t*)leds,pins,4,NUM_LED_PER_STRIPS,ORDER_GRB);
```
We are declaring that my `leds` array represent 4 strips of `NUM_LED_PER_STRIPS` leds ,each strip being linked to the pins defined in the pins array `pins`. This is way easier to declare a lot of strips. As discussed before if your strips are not of the same lentgh just define `NUM_LED_PER_STRIPS` with the largest `number_of_leds`.


### First let's declare a new driver

```C
#include "I2SClocklessLedDriver.h"

I2SClocklessLedDriver driver;
```

### How to define my leds array ?
#### You are using RGB type leds
RGB type leds store the information of the led over 3 bytes. `Red,Green,Blue`.  Hence the size in bytes of a led array of `NUM_LEDS` is `3xNUM_LEDS`
```C
uint8_t leds[3*NUM_LEDS];

//if you are using FastLED library this definition will be equivalent to the previous as the CRGB object is 3 bytes
CRGB leds[NUM_LEDS];

//you can use either of those
```
#### You are using RGBW type leds
This time to store the information of the led you will need 4 bytes `Red,Green,Blue,White` Hence the size in bytes of a led array of `NUM_LEDS` is `4xNUM_LEDS`
```C
uint8_t leds[4*NUM_LEDS];
```

### Driver functions
#### `initled(uint8_t *leds,int * Pins,int num_strips,int num_led_per_strip,colorarrangment cArr)`:

 This function initialize the strips.
 * `*leds`: a pointer to the leds array
* `*Pins`: a pointer to the pins array
*`num_strips`: the number of parallel strips
*`num_led_per_strip`: the number of leds per strip (or the number of leds in the longuest strip)
*`cArr`: The led ordering
    * `ORDER_GRBW`: For the RGBW strips
    * `ORDER_RGB`
    * `ORDER_RBG`
    * `ORDER_GRB` : The most often used
    * `ORDER_GBR`
    * `ORDER_BRG`
    * `ORDER_BGR`
 
 example 4: declaring 12 strips of 256 leds in GRB 
 ```C
 #define NUM_STRIPS 12
 #define NUM_LEDS_PER_STRIP 256
 
 #include "I2SClocklessLedDriver.h"

 I2SClocklessLedDriver driver;
 
 
 uint8_t leds[3*NUM_STRIPS*NUM_LEDS_PER_STRIP]; //equivalent of CRGB leds[NUM_LEDS_PER_STRIPS*NUM_LEDS_PER_STRIPS]
 int pins[NUM_STRIPS] ={0,2,4,5,12,13,14,15,16,29,25,26};
 driver.initled((uint8_t*)leds,pins,NUM_STRIPS,NUM_LED_PER_STRIP,ORDER_GRB);
 
 ```
 
 example 5: declaring 12 strips of 256 leds in RGBW
 ```C
 #define NUM_STRIPS 12
 #define NUM_LED_PER_STRIP 256
 
 #include "I2SClocklessLedDriver.h"

 I2SClocklessLedDriver driver;
 
 
 uint8_t leds[4*NUM_STRIPS*NUM_LED_PER_STRIP]; 
 int pins[NUM_STRIPS] ={0,2,4,5,12,13,14,15,16,29,25,26};
 driver.initled((uint8_t*)leds,pins,NUM_STRIPS,NUM_LED_PER_STRIP,ORDER_GRBW);
 
 ```
 #### `setBrightness(int brightness)`:
 
 This function sets the default brightness for 0->255
 
 #### `setPixel(uint32_t pos, uint8_t red, uint8_t green, uint8_t blue)`:
 Set the color of a pixel 
 
 NB: if you are familiar with FastLED it would be `leds[pos]=CRGB(red,green,blue)` as you will see in the examples
 
 #### `setPixel(uint32_t pos, uint8_t red, uint8_t green, uint8_t blue,uint8_t white)`:
 Set the color of a pixel for RGBW strips
 
 #### `showPixels()`:
 
 This function displays the pixels.
 
 #### Examples:
 
* `getting_started.ino`: an example to use 16 parallel strips of 256 leds 
* `getting_started_Fastled.ino`: an example to use 16 parallel strips of 256 leds using FastLED objects 
* `getting_started_RGBW.ino`: an example to use 16 parallel strips of 256 leds of RGBW leds
 
 
 
 
 

