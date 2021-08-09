#define FULL_DMA_BUFFER
#include "I2SClocklessLedDriver.h"
#define NUM_LEDS_PER_STRIP 256
#define NUMSTRIPS 16
//here we have 3 colors per pixel
//uint8_t leds[numstrips*ledsperstrip*3];
//this one below is same as the one above
uint8_t leds[NUMSTRIPS*NUM_LEDS_PER_STRIP*3];

int pins[16]={0,2,4,5,12,13,14,15,16,18,19,21,22,23,25,26};

I2SClocklessLedDriver driver;
void setup() {
    Serial.begin(115200);
    
    driver.initled((uint8_t*)leds,pins,NUMSTRIPS,NUM_LEDS_PER_STRIP,ORDER_GRB);
    driver.setBrightness(10);
    driver.showPixelsFromBuffer(LOOP); //start displaying in  loop what ever is in the DMA buffer
    
}

int off=0;
void loop() {
    for(int j=0;j<NUMSTRIPS;j++)
    {
        
        for(int i=0;i<NUM_LEDS_PER_STRIP;i++)
        {
            
           driver.setPixelinBuffer((i+off)%NUM_LEDS_PER_STRIP+NUM_LEDS_PER_STRIP*j,(NUM_LEDS_PER_STRIP-i)*255/NUM_LEDS_PER_STRIP,i*255/NUM_LEDS_PER_STRIP,(((128-i)+255)%255)*255/NUM_LEDS_PER_STRIP);
            
        }
    }
    off++;
    if(off==500)
    {
      Serial.println("we stop the display loop");
      driver.stopDisplayLoop();
    }
    if(off==1500)
    {
      Serial.println("we restart the display loop");
      driver.showPixelsFromBuffer(LOOP);
    }
}