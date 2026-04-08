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

#ifdef CONFIG_IDF_TARGET_ESP32P4
  // P4: no DMA in flight to quiesce.  Update topology, reconfigure PARLIO, update LUTs.
  this->numStrips = numStrips;
  totalLeds = 0;
  firstIndexPerOutput[0] = 0;
  for (int i = 0; i < numStrips; i++) {
    stripSize[i]  = sizes[i];
    totalLeds    += sizes[i];
    pins[i]       = pinsq[i];
    if (i > 0) firstIndexPerOutput[i] = firstIndexPerOutput[i - 1] + sizes[i - 1];
  }
  this->numLedPerStrip = maxLength(sizes, numStrips);
  offsetDisplay.offsetx    = 0;
  offsetDisplay.offsety    = 0;
  offsetDisplay.panelWidth = this->numLedPerStrip;
  offsetDisplay.panelHeight = 9999;
  defaultOffsetDisplay = offsetDisplay;
  linewidth     = this->numLedPerStrip;
  nbDmaBuffer   = dmaBuffer;
  this->nbComponents = nbComponents;
  this->pR = pR;  this->pG = pG;  this->pB = pB;  this->pW = pW;  this->pW2 = pW2;
  hwInit();
  #if HAS_PARLIO_DRIVER
  if (p4TxUnit == NULL) {
    initSuccess = false;
    ESP_LOGE(TAG, "updateDriver (P4): PARLIO reconfiguration failed — driver disabled");
    return;
  }
  #endif
  setBrightness(brightness);
  ESP_LOGD(TAG, "updateDriver (P4) %d x %d", numStrips, this->numLedPerStrip);
  return;
#endif

  // Compute new geometry locally so deleteDriver() still sees the old
  // this->numLedPerStrip (used as a loop bound for FULL_DMA_BUFFER frees).
  uint16_t newNumLedPerStrip = maxLength(sizes, numStrips);

  // Wait for any in-progress DMA transfer to complete before freeing buffers.
  // Do this before mutating any members so a timeout leaves the object consistent.
  if (isDisplaying) {
    if (waitDisp == NULL) waitDisp = xSemaphoreCreateCounting(10, 0);
    if (waitDisp == NULL) {
      ESP_LOGE(TAG, "updateDriver: failed to create waitDisp semaphore, aborting");
      return;
    }
    wasWaitingtofinish = true;  // Set AFTER semaphore exists so ISR can safely give it
    if (xSemaphoreTake(waitDisp, pdMS_TO_TICKS(500)) == pdFALSE) {
      wasWaitingtofinish = false;  // Clear on timeout to prevent stale ISR signal
      ESP_LOGE(TAG, "updateDriver: timeout waiting for DMA to idle, aborting reconfiguration");
      return;  // members unchanged — old DMA state remains consistent
    }
    wasWaitingtofinish = false;
  }

  deleteDriver();  // uses old numLedPerStrip and nbDmaBuffer as loop bounds

  // Now safe to apply all new geometry and configuration.
  this->numStrips = numStrips;
  totalLeds = 0;
  firstIndexPerOutput[0] = 0;
  for (int i = 0; i < numStrips; i++) {
    stripSize[i] = sizes[i];
    totalLeds += sizes[i];
    if (i > 0) firstIndexPerOutput[i] = firstIndexPerOutput[i - 1] + sizes[i - 1];
  }
  this->numLedPerStrip = newNumLedPerStrip;
  offsetDisplay.offsetx = 0;
  offsetDisplay.offsety = 0;
  offsetDisplay.panelWidth = newNumLedPerStrip;
  offsetDisplay.panelHeight = 9999;
  defaultOffsetDisplay = offsetDisplay;
  linewidth = newNumLedPerStrip;

  setShowDelay();
  setPins(pinsq);

  nbDmaBuffer = dmaBuffer;

  this->nbComponents = nbComponents;
  this->pR = pR;
  this->pG = pG;
  this->pB = pB;
  this->pW = pW;
  this->pW2 = pW2;

  initTransferBuffers();  // needs nbComponents and numLedPerStrip for buffer sizing

  setBrightness(brightness);  // allocate/free gamma maps based on new pW

  ESP_LOGD(TAG, "updateLeds %d x %d (%d)", numStrips, numLedPerStrip, nbDmaBuffer);
}

/** deleteDriver — frees all DMA buffers and the waitDisp semaphore.  Safe to call
 *  multiple times (all pointers are nulled after free). */
void I2SClocklessLedDriver::deleteDriver() {
#ifdef CONFIG_IDF_TARGET_ESP32P4
  #if HAS_PARLIO_DRIVER
  if (p4TxUnit != NULL) {
    esp_err_t err;
    if ((err = parlio_tx_unit_wait_all_done(p4TxUnit, portMAX_DELAY)) != ESP_OK)
      ESP_LOGE(TAG, "deleteDriver: parlio_tx_unit_wait_all_done failed: %s", esp_err_to_name(err));
    if ((err = parlio_tx_unit_disable(p4TxUnit)) != ESP_OK)
      ESP_LOGE(TAG, "deleteDriver: parlio_tx_unit_disable failed: %s", esp_err_to_name(err));
    if ((err = parlio_del_tx_unit(p4TxUnit)) != ESP_OK)
      ESP_LOGE(TAG, "deleteDriver: parlio_del_tx_unit failed: %s", esp_err_to_name(err));
    p4TxUnit = NULL;
  }
  #endif
  if (p4Buffer1) { heap_caps_free(p4Buffer1); p4Buffer1 = nullptr; }
  if (p4Buffer2) { heap_caps_free(p4Buffer2); p4Buffer2 = nullptr; }
  p4BufferActive      = nullptr;
  initSuccess         = false;
  p4LastOutputs       = -1;
  p4LastLedsPerOutput = -1;
#endif

  #if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32  // P4 uses PARLIO — no I2S/DMA buffers
  if (dmaBuffersTampon) {
    for (int i = 0; i < nbDmaBuffer + 2; i++) {
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