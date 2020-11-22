#include "I2SClocklessLedDriver.h"

#define ledsperstrip 256
#define numstrips 16
//here we have 4 colors per pixel
uint8_t leds[numstrips*ledsperstrip*4];

int pins[16]={0,2,4,5,12,13,14,15,16,18,19,21,22,23,25,26};

I2SClocklessLedDriver driver;
void setup() {
    Serial.begin(115200);
    
    driver.initled(leds,pins,numstrips,ledsperstrip,ORDER_GRBW);
    driver.setBrightness(10);
    
}

int off=0;
long time1,time2,time3;
void loop() {
    time1=ESP.getCycleCount();
    for(int j=0;j<numstrips;j++)
    {
        
        for(int i=0;i<ledsperstrip;i++)
        {
            
            driver.setPixel((i+off)%ledsperstrip+ledsperstrip*j,255-i,i,((128-i)+255)%255,i/25);
            
        }
    }
    time2=ESP.getCycleCount();
    driver.showPixels();
    time3=ESP.getCycleCount();
    Serial.printf("Calcul pixel fps:%.2f   showPixels fps:%.2f   Total fps:%.2f \n",(float)240000000/(time2-time1),(float)240000000/(time3-time2),(float)240000000/(time3-time1));
    off++;
    
}