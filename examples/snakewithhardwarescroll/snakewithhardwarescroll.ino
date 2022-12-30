#define ALTERNATEPATTERN 0
#include "FastLED.h"
#define ENABLE_HARDWARE_SCROLL
#include "I2SClocklessLedDriver.h"
#define NUM_LEDS_PER_STRIP 256
#define NUMSTRIPS 16
//here we have 3 colors per pixel
//uint8_t leds[numstrips*ledsperstrip*3];
//this one below is same as the one above

//each strip will have a snake of size the equials the strimnumber
CRGB leds[NUMSTRIPS * NUM_LEDS_PER_STRIP];


int pins[16] = {0, 2, 4, 5, 12, 13, 14, 15, 16, 18, 19, 21, 22, 23, 25, 26};

OffsetDisplay offd;
I2SClocklessLedDriver driver;
void setup()
{
  Serial.begin(115200);

  driver.initled((uint8_t *)leds, pins, NUMSTRIPS, NUM_LEDS_PER_STRIP, ORDER_GRB);
  driver.setBrightness(20);
  for (int j = 0; j < NUMSTRIPS; j++)
  {
     leds[NUM_LEDS_PER_STRIP * j] = CRGB::Red;
    for (int i = 1; i < j + 2; i++)
    {
      leds[i + NUM_LEDS_PER_STRIP * j] = CRGB::Green;
    }
    leds[17+NUM_LEDS_PER_STRIP * j] = CRGB::Blue;
  }
  driver.showPixels();
  delay(1000); 
  offd = driver.getDefaultOffset();
  //uncomment the lines if you want all the strips considered as a big one.
  //offd.panel_width=ledsperstrip*numstrips; 
  //offd.panel_height=1;

}

int off = 0;
long time1, time2, time3;
void loop()
{
  //going forward
  offd.offsetx = -off;
  time2 = ESP.getCycleCount();
  driver.showPixels(offd); 
  time3 = ESP.getCycleCount();
  Serial.printf("Calcul pixel fps:%.2f   showPixels fps:%.2f   Total fps:%.2f \n", (float)240000000 / (time2 - time1), (float)240000000 / (time3 - time2), (float)240000000 / (time3 - time1));
  off++;
  delay(50);
}
