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

uint8_t gNbDmaBuffer = 6;
uint8_t gNumStrips = 16;

#ifdef CONFIG_IDF_TARGET_ESP32S3
clock_speed clock1123Khz = {4, 20, 9};
clock_speed clock1111Khz = {4, 2, 1};
clock_speed clock1000Khz = {5, 1, 0};
clock_speed clock800Khz = {6, 4, 1};
#endif

/**
 * updateDriver — reconfigures the driver at runtime (pin map, strip count/lengths,
 * DMA buffer depth, colour order).  Safely waits for any in-flight DMA transfer
 * to complete before freeing and reallocating buffers.  Leaves the object
 * unchanged if arguments are invalid or if the wait times out.
 */
void I2SClocklessLedDriver::updateDriver(uint8_t* pinsq, uint16_t* sizes, uint8_t numStrips, uint8_t dmaBuffer, uint8_t nbComponents, uint8_t pR, uint8_t pG, uint8_t pB, uint8_t pW, uint8_t pW2) {
  if (pinsq == nullptr || sizes == nullptr || numStrips == 0 || numStrips > MAX_PINS || dmaBuffer == 0) {
    ESP_LOGE(TAG, "updateDriver: invalid args numStrips=%u dmaBuffer=%u sizes=%p pinsq=%p", numStrips, dmaBuffer, (void*)sizes, (void*)pinsq);
    return;
  }

  // Compute new geometry locally so deleteDriver() still sees the old
  // this->numLedPerStrip (used as a loop bound for FULL_DMA_BUFFER frees).
  uint16_t newNumLedPerStrip = maxLength(sizes, numStrips);

  // Wait for any in-progress DMA transfer to complete before freeing buffers.
  // Do this before mutating any members so a timeout leaves the object consistent.
  if (isDisplaying) {
    wasWaitingtofinish = true;
    if (waitDisp == NULL) waitDisp = xSemaphoreCreateCounting(10, 0);
    if (waitDisp == NULL) {
      wasWaitingtofinish = false;
      ESP_LOGE(TAG, "updateDriver: failed to create waitDisp semaphore, aborting");
      return;
    }
    if (xSemaphoreTake(waitDisp, pdMS_TO_TICKS(500)) == pdFALSE) {
      ESP_LOGE(TAG, "updateDriver: timeout waiting for DMA to idle, aborting reconfiguration");
      return;  // members unchanged — old DMA state remains consistent
    }
    wasWaitingtofinish = false;
  }

  deleteDriver();  // uses old numLedPerStrip and gNbDmaBuffer as loop bounds

  // Now safe to apply all new geometry and configuration.
  this->numStrips = numStrips;
  totalLeds = 0;
  for (int i = 0; i < numStrips; i++) {
    stripSize[i] = sizes[i];
    totalLeds += sizes[i];
  }
  this->numLedPerStrip = newNumLedPerStrip;
  offsetDisplay.offsetx = 0;
  offsetDisplay.offsety = 0;
  offsetDisplay.panelWidth = newNumLedPerStrip;
  offsetDisplay.panelHeight = 9999;
  defaultOffsetDisplay = offsetDisplay;
  linewidth = newNumLedPerStrip;

  setShowDelay();
  setGlobalNumStrips();
  setPins(pinsq);

  gNbDmaBuffer = dmaBuffer;

  this->nbComponents = nbComponents;
  this->pR = pR;
  this->pG = pG;
  this->pB = pB;
  this->pW = pW;
  this->pW2 = pW2;

  initDMABuffers();  // needs nbComponents and numLedPerStrip for buffer sizing

  setBrightness(brightness);  // allocate/free gamma maps based on new pW

  ESP_LOGD(TAG, "updateLeds %d x %d (%d)", numStrips, numLedPerStrip, gNbDmaBuffer);
}

/** deleteDriver — frees all DMA buffers and the waitDisp semaphore.  Safe to call
 *  multiple times (all pointers are nulled after free). */
void I2SClocklessLedDriver::deleteDriver() {
  #if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32  // P4 for PhysicalDriver not supported yet
  if (dmaBuffersTampon) {
    for (int i = 0; i < gNbDmaBuffer + 2; i++) {
      if (dmaBuffersTampon[i]) {
        if (dmaBuffersTampon[i]->buffer) heap_caps_free(dmaBuffersTampon[i]->buffer);
        heap_caps_free(dmaBuffersTampon[i]);
        dmaBuffersTampon[i] = nullptr;
      }
    }
    heap_caps_free(static_cast<void*>(dmaBuffersTampon));
    dmaBuffersTampon = nullptr;
  }
  #endif

  #ifdef FULL_DMA_BUFFER
  if (dmaBuffersTransposed) {
    for (int i = 0; i < numLedPerStrip + 2; i++) {
      if (dmaBuffersTransposed[i]) {
        if (dmaBuffersTransposed[i]->buffer) heap_caps_free(dmaBuffersTransposed[i]->buffer);
        heap_caps_free(dmaBuffersTransposed[i]);
        dmaBuffersTransposed[i] = nullptr;
      }
    }
    free(dmaBuffersTransposed);
    dmaBuffersTransposed = nullptr;
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
  if (hmap) {
    free(hmap);
    hmap = nullptr;
  }
    #endif
  #endif

  if (waitDisp) {
    vSemaphoreDelete(waitDisp);
    waitDisp = NULL;
  }
}