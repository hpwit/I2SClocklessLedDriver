#ifdef PLATFORM_VERSION

// #define SETUP_WIFI 1 // choose whether to include WiFi

  #include "Arduino.h"

  #define NUM_LEDS_PER_STRIP 256
  #define NUMSTRIPS 6
  #include "I2SClocklessLedDriver.h"

  #if SETUP_WIFI
    #include "WiFiThings.h"
  #endif

// here we have 3 colors per pixel
uint8_t leds[NUMSTRIPS * NUM_LEDS_PER_STRIP * 3];

  #ifdef CONFIG_IDF_TARGET_ESP32S3
// uint8_t pins[6] = {9, 10,12,8,18,17};
uint8_t pins[6] = {16, 10, 12, 8, 18, 17};
  #else
uint8_t pins[6] = {2, 12, 13, 25, 33, 32};
  #endif

I2SClocklessLedDriver driver;

void setup() {
  Serial.begin(115200);

  #if SETUP_WIFI
  setupWiFi();
  #endif

  driver.initled(leds, pins, NUMSTRIPS, NUM_LEDS_PER_STRIP, ORDER_GRB);
  driver.setBrightness(10);
}

int off = 0;
long time1, time2, time3;

void loop() {
  time1 = ESP.getCycleCount();

  uint8_t effect = 2;

  switch (effect) {
  case 0:
    // rainbowy
    for (int j = 0; j < NUMSTRIPS; j++)
      for (int i = 0; i < NUM_LEDS_PER_STRIP; i++)
        driver.setPixel((i + off) % NUM_LEDS_PER_STRIP + NUM_LEDS_PER_STRIP * j, (NUM_LEDS_PER_STRIP - i) * 255 / NUM_LEDS_PER_STRIP, i * 255 / NUM_LEDS_PER_STRIP,
                        (((128 - i) + 255) % 255) * 255 / NUM_LEDS_PER_STRIP);
    break;
  case 1:
    // random effect
    for (int i = 0; i < NUM_LEDS_PER_STRIP * NUMSTRIPS * 3; i++) leds[i] *= 0;  // fadetoblack 70%
    driver.setPixel(random(NUM_LEDS_PER_STRIP * NUMSTRIPS), 255, random(255), 0);
    break;
  case 2:
    // lines
    for (int i = 0; i < NUM_LEDS_PER_STRIP * NUMSTRIPS * 3; i++) leds[i] *= 0;  // black
    uint16_t time = 200;                                                        // ms
    uint8_t row = (millis() / time) % 16;
    for (uint8_t col = 0; col < 16; col++) driver.setPixel(row * 16 + ((row % 2 == 0) ? col : (15 - col)), 255, 0, 0);
    break;
  }

  time2 = ESP.getCycleCount();

  driver.showPixels();

  time3 = ESP.getCycleCount();

  #if SETUP_WIFI
  loopWiFi(time1, time2, time3);
  #endif

  if (off % 100 == 0)
    Serial.printf("Calcul pixel fps:%.2f   showPixels fps:%.2f   Total fps:%.2f \n", (float)240000000 / (time2 - time1), (float)240000000 / (time3 - time2), (float)240000000 / (time3 - time1));
  off++;
}
#endif