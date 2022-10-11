#define NUM_LEDS_PER_STRIP 256
#define NUMSTRIPS 3
#define USE_FASTLED //to enable the link between Pixel type and CRGB type if you need it and to increase compatibility
#include "I2SClocklessLedDriver.h"

//here we have 3 colors per pixel
uint8_t leds[NUMSTRIPS*NUM_LEDS_PER_STRIP*3];

int pins[3]={0,2,4};
int lengths[3]={NUM_LEDS_PER_STRIP,3*NUM_LEDS_PER_STRIP,2*NUM_LEDS_PER_STRIP};

I2SClocklessLedDriver driver;
void setup() {
    Serial.begin(115200);
    
    driver.initled(leds,pins,lengths,NUMSTRIPS,ORDER_GRB);
    driver.setBrightness(10);
    
}

int off=0;
long time1,time2,time3;
void loop() {
    time1=ESP.getCycleCount();
    for(int j=0;j<NUMSTRIPS;j++)
    {
        
        for(int i=0;i<lengths[i];i++)
        {
            //driver.strip(j) return the pointer to the strip j
          
            driver.strip(j)[(i+off)%lengths[j]]=Pixel( (lengths[j]-i)*255/lengths[j] ,i*255/lengths[j],(((128-i)+255)%255)*255/lengths[j]);
          
           //or
           //driver.strip(j)[(i+off)%lengths[j]]=CRGB( (lengths[j]-i)*255/lengths[j] ,i*255/lengths[j],(((128-i)+255)%255)*255/lengths[j]);
            
        }
    }
    time2=ESP.getCycleCount();
    driver.showPixels();
    time3=ESP.getCycleCount();
    Serial.printf("Calcul pixel fps:%.2f   showPixels fps:%.2f   Total fps:%.2f \n",(float)240000000/(time2-time1),(float)240000000/(time3-time2),(float)240000000/(time3-time1));
    off++;
    
}