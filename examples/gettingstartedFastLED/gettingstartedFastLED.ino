#include "FastLED.h"
#include "I2SClocklessLedDriver.h"
#define NUM_LEDS_PER_STRIP 256
#define NUMSTRIPS 16
//here we have 3 colors per pixel
//uint8_t leds[numstrips*ledsperstrip*3];
//this one below is same as the one above
CRGB leds[NUMSTRIPS*NUM_LEDS_PER_STRIP];

int pins[16]={0,2,4,5,12,13,14,15,16,18,19,21,22,23,25,26};

I2SClocklessLedDriver driver;
void setup() {
    Serial.begin(115200);
    
    driver.initled((uint8_t*)leds,pins,NUMSTRIPS,NUM_LEDS_PER_STRIP,ORDER_GRB);
    driver.setBrightness(10);
    
}

int off=0;
long time1,time2,time3;
void loop() {
    time1=ESP.getCycleCount();
    for(int j=0;j<NUMSTRIPS;j++)
    {
        
        for(int i=0;i<NUM_LEDS_PER_STRIP;i++)
        {
            
           leds[(i+off)%NUM_LEDS_PER_STRIP+NUM_LEDS_PER_STRIP*j]=CRGB((NUM_LEDS_PER_STRIP-i)*255/NUM_LEDS_PER_STRIP,i*255/NUM_LEDS_PER_STRIP,(((128-i)+255)%255)*255/NUM_LEDS_PER_STRIP);
            
        }
    }
    time2=ESP.getCycleCount();
    driver.showPixels();
    time3=ESP.getCycleCount();
    Serial.printf("Calcul pixel fps:%.2f   showPixels fps:%.2f   Total fps:%.2f \n",(float)240000000/(time2-time1),(float)240000000/(time3-time2),(float)240000000/(time3-time1));
    off++;
    
}
