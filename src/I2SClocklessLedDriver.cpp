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
// 🌙 update driver: recreate dma buffers if num_strips or num_led_per_strip or dmaBuffer size changed
void I2SClocklessLedDriver::updateDriver(uint8_t* Pinsq, uint16_t* sizes, uint8_t num_strips, uint8_t dmaBuffer, uint8_t nb_components, uint8_t p_r, uint8_t p_g, uint8_t p_b, uint8_t p_w, uint8_t p_w2) {
  if (Pinsq == nullptr || sizes == nullptr || num_strips == 0 || num_strips > MAX_PINS) {
    ESP_LOGE(TAG, "updateDriver: invalid args num_strips=%u sizes=%p Pinsq=%p", num_strips, (void*)sizes, (void*)Pinsq);
    return;
  }

  // do what ledsDriver.initled is doing, except i2sInit

  // from initled
  this->num_strips = num_strips;
  total_leds = 0;
  for (int i = 0; i < num_strips; i++) {
    stripSize[i] = sizes[i];
    total_leds += sizes[i];
  }
  uint16_t num_led_per_strip = maxLength(sizes, num_strips);

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

  // Wait for any in-progress DMA transfer to complete before freeing buffers.
  if (isDisplaying) {
    if (I2SClocklessLedDriver_waitDisp == NULL) I2SClocklessLedDriver_waitDisp = xSemaphoreCreateCounting(10, 0);
    if (xSemaphoreTake(I2SClocklessLedDriver_waitDisp, pdMS_TO_TICKS(500)) == pdFALSE) {
      ESP_LOGE(TAG, "updateDriver: timeout waiting for DMA to idle, aborting reconfiguration");
      return;
    }
  }

  deleteDriver();  // free previous allocations

  __NB_DMA_BUFFER = dmaBuffer;  // set new buffer count

  initDMABuffers();  // create them again

  // Update color component assignments and gamma maps atomically after DMA is
  // reconfigured, so loadAndTranspose never sees p_w != UINT8_MAX with a null __white_map.
  this->nb_components = nb_components;
  this->p_r = p_r;
  this->p_g = p_g;
  this->p_b = p_b;
  this->p_w = p_w;
  this->p_w2 = p_w2;
  setBrightness(_brightness);  // allocate/free gamma maps based on new p_w

  ESP_LOGD(TAG, "updateLeds %d x %d (%d)", num_strips, num_led_per_strip, __NB_DMA_BUFFER);
}

// 🌙 delete driver when the driver is stopped
void I2SClocklessLedDriver::deleteDriver() {
  #if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32  // P4 for PhysicalDriver not supported yet
  if (DMABuffersTampon) {
    for (int i = 0; i < __NB_DMA_BUFFER + 2; i++) {
      if (DMABuffersTampon[i]) {
        if (DMABuffersTampon[i]->buffer) heap_caps_free(DMABuffersTampon[i]->buffer);
        heap_caps_free(DMABuffersTampon[i]);
        DMABuffersTampon[i] = nullptr;
      }
    }
    heap_caps_free(DMABuffersTampon);
    DMABuffersTampon = nullptr;
  }
  #endif

  #ifdef FULL_DMA_BUFFER
  if (DMABuffersTransposed) {
    for (int i = 0; i < num_led_per_strip + 2; i++) {
      if (DMABuffersTransposed[i]) {
        if (DMABuffersTransposed[i]->buffer) heap_caps_free(DMABuffersTransposed[i]->buffer);
        heap_caps_free(DMABuffersTransposed[i]);
        DMABuffersTransposed[i] = nullptr;
      }
    }
    free(DMABuffersTransposed);
    DMABuffersTransposed = nullptr;
  }
  #endif

  #if HARDWARESPRITES == 1
  if (target) {
    free(target);
    target = nullptr;
  }
  #endif

  #ifdef __HARDWARE_MAP
    #ifndef __NON_HEAP
  if (_hmap) {
    free(_hmap);
    _hmap = nullptr;
  }
    #endif
  #endif

  if (I2SClocklessLedDriver_waitDisp) {
    vSemaphoreDelete(I2SClocklessLedDriver_waitDisp);
    I2SClocklessLedDriver_waitDisp = NULL;
  }
}

#endif