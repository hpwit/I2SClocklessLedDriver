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
// uint8_t pins[NUMSTRIPS] = {9, 10,12,8,18,17};
uint8_t pins[NUMSTRIPS] = {16, 10, 12, 8, 18, 17};
  #else
uint8_t pins[NUMSTRIPS] = {2, 12, 13, 25, 33, 32};
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

uint32_t off = 0;
uint32_t time1, time2, time3;

void loop() {
  time1 = ESP.getCycleCount();

  uint8_t effect = (off / 500) % 3;

  switch (effect) {
  case 0:
    // rainbowy
    for (int j = 0; j < NUMSTRIPS; j++)
      for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) driver.setPixel((i + off) % NUM_LEDS_PER_STRIP + NUM_LEDS_PER_STRIP * j, (NUM_LEDS_PER_STRIP - i) * 255 / NUM_LEDS_PER_STRIP, i * 255 / NUM_LEDS_PER_STRIP, (((128 - i) + 255) % 255) * 255 / NUM_LEDS_PER_STRIP);
    break;
  case 1:
    // random effect
    memset(leds, 0, sizeof(leds));  // black
    driver.setPixel(random(NUM_LEDS_PER_STRIP * NUMSTRIPS), 255, random(255), 0);
    break;
  case 2:
    // lines
    memset(leds, 0, sizeof(leds));  // black
    uint16_t time = 200;                                                       // ms
    uint8_t row = (millis() / time) % NUMSTRIPS;
    for (uint16_t col = 0; col < NUM_LEDS_PER_STRIP; col++) driver.setPixel(row * NUM_LEDS_PER_STRIP + ((row % 2 == 0) ? col : (NUM_LEDS_PER_STRIP - 1 - col)), 255, 0, 0);
    break;
  }

  time2 = ESP.getCycleCount();

  driver.showPixels();

  time3 = ESP.getCycleCount();

  #if SETUP_WIFI
  loopWiFi(time1, time2, time3);
  #endif

  if (off % 100 == 0) {
    const float cpu_hz = ESP.getCpuFreqMHz() * 1000000.0f;
    const uint32_t calc_cycles = time2 - time1;
    const uint32_t show_cycles = time3 - time2;
    const uint32_t total_cycles = time3 - time1;
    if (calc_cycles && show_cycles && total_cycles) {
      Serial.printf("Calcul pixel fps:%.2f   showPixels fps:%.2f   Total fps:%.2f \n",
                    cpu_hz / calc_cycles, cpu_hz / show_cycles, cpu_hz / total_cycles);
    }
  }
  off++;
}
#endif