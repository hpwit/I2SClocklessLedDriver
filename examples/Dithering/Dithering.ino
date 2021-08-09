#include "FastLED.h"
#include "I2SClocklessLedDriver.h"
#define NUM_LEDS_PER_STRIP 50
#define NUMSTRIPS 16
//here we have 3 colors per pixel
//uint8_t leds[numstrips*ledsperstrip*3];
//this one below is same as the one above
CRGB leds1[NUMSTRIPS*NUM_LEDS_PER_STRIP];
CRGB leds2[NUMSTRIPS*NUM_LEDS_PER_STRIP];
CRGB leds3[NUMSTRIPS*NUM_LEDS_PER_STRIP];
CRGB leds4[NUMSTRIPS*NUM_LEDS_PER_STRIP];
CRGB leds5[NUMSTRIPS*NUM_LEDS_PER_STRIP];
CRGB leds6[NUMSTRIPS*NUM_LEDS_PER_STRIP];
int pins[16]={0,2,4,5,12,13,14,15,16,18,19,21,22,23,25,26};

I2SClocklessLedDriver driver;
void setup() {
    Serial.begin(115200);
    
    driver.initled((uint8_t*)leds1,pins,NUMSTRIPS,NUM_LEDS_PER_STRIP,ORDER_GRB);
    CRGB color=CRGB(10,0,0);
//led 0 equivalent of 10/6=1,67
//led 1 equivalent of 20/6=3,33
//led 2 equivalent of 30/6=5
//led 3 equivalent of 40/6=6,67
//led 4 equivalent of 50/6=8,33
//led 5 equivalent of 10

    for(int j=0;j<NUMSTRIPS;j++)
    {
      
        leds1[5+j*NUM_LEDS_PER_STRIP]=color; 

        leds2[4+j*NUM_LEDS_PER_STRIP]=color;
        leds2[5+j*NUM_LEDS_PER_STRIP]=color;
        
        leds3[3+j*NUM_LEDS_PER_STRIP]=color;
        leds3[4+j*NUM_LEDS_PER_STRIP]=color;
        leds3[5+j*NUM_LEDS_PER_STRIP]=color;


        leds4[2+j*NUM_LEDS_PER_STRIP]=color;
        leds4[3+j*NUM_LEDS_PER_STRIP]=color;
        leds4[4+j*NUM_LEDS_PER_STRIP]=color;
        leds4[5+j*NUM_LEDS_PER_STRIP]=color;
           
        leds5[1+j*NUM_LEDS_PER_STRIP]=color;
        leds5[2+j*NUM_LEDS_PER_STRIP]=color;
        leds5[3+j*NUM_LEDS_PER_STRIP]=color;
        leds5[4+j*NUM_LEDS_PER_STRIP]=color;
        leds5[5+j*NUM_LEDS_PER_STRIP]=color; 

        leds6[0+j*NUM_LEDS_PER_STRIP]=color;
        leds6[1+j*NUM_LEDS_PER_STRIP]=color;
        leds6[2+j*NUM_LEDS_PER_STRIP]=color;
        leds6[3+j*NUM_LEDS_PER_STRIP]=color;
        leds6[4+j*NUM_LEDS_PER_STRIP]=color;
        leds6[5+j*NUM_LEDS_PER_STRIP]=color; 
        
    }
    
}


void loop() {
  
    while(1)
    {
       driver.showPixels(); //equals to driver.showPixels((uint8_t *)leds1); as the leds are initialized with leds1
       driver.showPixels((uint8_t *)leds2);
       driver.showPixels((uint8_t *)leds3);
       driver.showPixels((uint8_t *)leds4);
       driver.showPixels((uint8_t *)leds5);
       driver.showPixels((uint8_t *)leds6);
      
    }
   
}
