/**
    @title     I2SClocklessLedDriver
    @file      I2SClocklessLedDriver.cpp
    @repo      https://github.com/hpwit/I2SClocklessLedDriver
    @Copyright © 2025 Yves Bazin
    @license   MIT License
    @license   For non MIT usage, commercial licenses must be purchased. Contact us for more information.
**/

// IDF5.5: Split ICLDriver.h into ICLDriver.h and ICLDriver.cpp to allow multiple includes in projects (no duplicate definition)

#include "I2SClocklessLedDriver.h"

// IDF5.5: __NB_DMA_BUFFER and NUM_STRIPS are global variables to allow changing it at runtime
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
uint8_t __NB_DMA_BUFFER = 6;
uint8_t NUM_STRIPS = 16;
#endif

#ifdef CONFIG_IDF_TARGET_ESP32S3
clock_speed clock_1123KHZ = {4, 20, 9};
clock_speed clock_1111KHZ = {4, 2, 1};
clock_speed clock_1000KHZ = {5, 1, 0};
clock_speed clock_800KHZ = {6, 4, 1};
#endif

// IDF5.5: updateLeds
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
void I2SClocklessLedDriver::updateDriver(uint8_t* Pinsq, uint16_t* sizes, uint8_t num_strips, uint8_t dmaBuffer, uint8_t nb_components, uint8_t p_r, uint8_t p_g, uint8_t p_b, uint8_t p_w) {
  // do what ledsDriver.initled is doing, except i2sInit

  // from initled
  this->num_strips = num_strips;
  total_leds = 0;
  for (int i = 0; i < num_strips; i++) {
    stripSize[i] = sizes[i];
    total_leds += sizes[i];
  }
  uint16_t num_led_per_strip = maxLength(sizes, num_strips);

  this->nb_components = nb_components;
  this->p_r = p_r;
  this->p_g = p_g;
  this->p_b = p_b;
  this->p_w = p_w;

  // from __initled:

  this->num_led_per_strip = num_led_per_strip;
  _offsetDisplay.offsetx = 0;
  _offsetDisplay.offsety = 0;
  _offsetDisplay.panel_width = num_led_per_strip;
  _offsetDisplay.panel_height = 9999;
  _defaultOffsetDisplay = _offsetDisplay;
  linewidth = num_led_per_strip;

  // setShowDelay(num_led_per_strip);
  setShowDelay();
  setGlobalNumStrips();

  setPins(Pinsq);  // if pins and lengths changed, set that right

  // i2sInit(); //not necessary, initled did it, no need to change

  deleteDriver();  // free previous allocations

  __NB_DMA_BUFFER = dmaBuffer;  // set new buffer count

  initDMABuffers();  // create them again

  ESP_LOGD(TAG, "updateLeds %d x %d (%d)", num_strips, num_led_per_strip, __NB_DMA_BUFFER);
}

void I2SClocklessLedDriver::deleteDriver() {
  #if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32  // P4 for PhysicalDriver not supported yet
  for (int i = 0; i < __NB_DMA_BUFFER + 2; i++) {
    heap_caps_free(DMABuffersTampon[i]->buffer);
    heap_caps_free(DMABuffersTampon[i]);
  }
  heap_caps_free(DMABuffersTampon);
  #endif
  // anything else to delete? I2S ...
}

#endif