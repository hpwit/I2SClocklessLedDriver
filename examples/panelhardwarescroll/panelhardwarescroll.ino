#include "FastLED.h"
#include "I2SClocklessLedDriver.h"
#define ledsperstrip 360 //each strip will make 3 rows hence each row will be 120 leds wide
#define numstrips 16
//hence the total height of the panel will be 16*3 =48 and the width =120


CRGB leds[numstrips * ledsperstrip];


int pins[16] = {0, 2, 4, 5, 12, 13, 14, 15, 16, 18, 19, 21, 22, 23, 25, 26};

OffsetDisplay offd;
I2SClocklessLedDriver driver;
void setup()
{
  Serial.begin(115200);

  driver.initled((uint8_t *)leds, pins, numstrips, ledsperstrip, ORDER_GRB);
  driver.setBrightness(20);
  for (int j = 0; j < numstrips; j++)
  {
     leds[ledsperstrip * j] = CRGB::Red;
    for (int i = 1; i < j + 2; i++)
    {
      leds[i + ledsperstrip * j] = CRGB::Green;
    }
    leds[17+ledsperstrip * j] = CRGB::Blue;
  }
  driver.showPixels();
  delay(1000); 
  offd = driver.getDefaultOffset();
  offd.panel_width=120; 
  offd.panel_height=48;


}

int off = 0;
long time1, time2, time3;
void loop()
{
  
  offd.offsetx = 120*cos(3.14*off/360);
  offd.offsety = 120*sin(3.14*off/360);
  time2 = ESP.getCycleCount();
  driver.showPixels(offd); 
  time3 = ESP.getCycleCount();
  Serial.printf("Calcul pixel fps:%.2f   showPixels fps:%.2f   Total fps:%.2f \n", (float)240000000 / (time2 - time1), (float)240000000 / (time3 - time2), (float)240000000 / (time3 - time1));
  off++;
  delay(50);
}
