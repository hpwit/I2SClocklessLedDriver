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
 * DMA buffer depth, colour order).  Tears down all hardware resources, applies the
 * new configuration, then reinitialises — same sequence as initLedImpl but preceded
 * by a wait for any in-flight transfer and a full teardown.  Leaves the object
 * unchanged if arguments are invalid or if the DMA wait times out.
 *
 * Platform notes:
 *   ESP32/S3 — waits for in-flight DMA via waitDisp semaphore (released by ISR).
 *   P4       — isDisplaying is always false (hwStop is synchronous), so the wait
 *              block is a no-op; execution falls straight through to deleteDriver().
 */
void I2SClocklessLedDriver::updateDriver(uint8_t* pinsq, uint16_t* sizes, uint8_t numStrips, uint8_t dmaBuffer, uint8_t nbComponents, uint8_t pR, uint8_t pG, uint8_t pB, uint8_t pW, uint8_t pW2) {
  if (pinsq == nullptr || sizes == nullptr || numStrips == 0 || numStrips > MAX_PINS || dmaBuffer == 0) {
    ESP_LOGE(TAG, "updateDriver: invalid args numStrips=%u dmaBuffer=%u sizes=%p pinsq=%p", numStrips, dmaBuffer, (void*)sizes, (void*)pinsq);
    return;  // leave driver in previous consistent state
  }

  // Compute new geometry before deleteDriver() which still needs the old numLedPerStrip
  // as a loop bound (FULL_DMA_BUFFER frees iterate up to numLedPerStrip + 2).
  uint16_t newNumLedPerStrip = maxLength(sizes, numStrips);

  // Wait for any in-progress transfer.  On P4, isDisplaying is always false so this
  // block is unreachable; on ESP32/S3 the ISR signals waitDisp when the frame ends.
  if (isDisplaying) {
    if (waitDisp == NULL) waitDisp = xSemaphoreCreateCounting(10, 0);
    if (waitDisp == NULL) {
      ESP_LOGE(TAG, "updateDriver: failed to create waitDisp semaphore, aborting");
      return;
    }
    wasWaitingtofinish = true;  // set AFTER semaphore exists so ISR can safely give it
    if (xSemaphoreTake(waitDisp, pdMS_TO_TICKS(500)) == pdFALSE) {
      wasWaitingtofinish = false;
      ESP_LOGE(TAG, "updateDriver: timeout waiting for transfer to idle, aborting");
      return;
    }
    wasWaitingtofinish = false;
  }

  deleteDriver();  // tears down HW (GDMA/ISR on S3/ESP32, PARLIO on P4) and frees buffers

  initErrorOccurred = false;
  initSuccess = false;

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
  nbDmaBuffer = dmaBuffer;
  this->nbComponents = nbComponents;
  this->pR = pR;
  this->pG = pG;
  this->pB = pB;
  this->pW = pW;
  this->pW2 = pW2;

  setShowDelay();
  setPins(pinsq);
  hwInit();
  if (initErrorOccurred) {
    initSuccess = false;
    return;
  }
  initTransferBuffers();
  setBrightness(brightness);
  initSuccess = !initErrorOccurred && numStrips > 0 && numLedPerStrip > 0;
  ESP_LOGD(TAG, "updateDriver %d x %d (%d)", numStrips, numLedPerStrip, nbDmaBuffer);
}

/** deleteDriver — tears down all hardware resources and frees all buffers.
 *  Safe to call multiple times (all handles/pointers are nulled after free).
 *  After this call the driver is fully quiesced; hwInit() + initTransferBuffers()
 *  are required before the next showPixels(). */
void I2SClocklessLedDriver::deleteDriver() {

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32
  // Tear down hardware before freeing DMA buffers so the peripheral cannot
  // continue to access memory that is about to be freed.
  #ifdef CONFIG_IDF_TARGET_ESP32S3
  if (dmaChan != nullptr) {
    gdma_disconnect(dmaChan);
    gdma_del_channel(dmaChan);
    dmaChan = nullptr;
  }
  #else  // CONFIG_IDF_TARGET_ESP32
  if (intrHandle != nullptr) {
    esp_intr_free(intrHandle);
    intrHandle = nullptr;
  }
  #endif
  if (transferBuffers) {
    for (int i = 0; i < nbDmaBuffer + 2; i++) {
      if (transferBuffers[i]) {
        if (transferBuffers[i]->buffer) heap_caps_free(transferBuffers[i]->buffer);
        heap_caps_free(transferBuffers[i]);
        transferBuffers[i] = nullptr;
      }
    }
    heap_caps_free(static_cast<void*>(transferBuffers));
    transferBuffers = nullptr;
  }
#elif CONFIG_IDF_TARGET_ESP32P4
  #if HAS_PARLIO_DRIVER
  if (p4TxUnit != NULL) {
    esp_err_t err;
    if ((err = parlio_tx_unit_wait_all_done(p4TxUnit, portMAX_DELAY)) != ESP_OK) ESP_LOGE(TAG, "deleteDriver: parlio_tx_unit_wait_all_done failed: %s", esp_err_to_name(err));
    if ((err = parlio_tx_unit_disable(p4TxUnit)) != ESP_OK) ESP_LOGE(TAG, "deleteDriver: parlio_tx_unit_disable failed: %s", esp_err_to_name(err));
    if ((err = parlio_del_tx_unit(p4TxUnit)) != ESP_OK) ESP_LOGE(TAG, "deleteDriver: parlio_del_tx_unit failed: %s", esp_err_to_name(err));
    p4TxUnit = NULL;
  }
  #endif
  if (p4Buffer1) {
    heap_caps_free(p4Buffer1);
    p4Buffer1 = nullptr;
  }
  if (p4Buffer2) {
    heap_caps_free(p4Buffer2);
    p4Buffer2 = nullptr;
  }
  p4BufferActive = nullptr;
  initSuccess = false;
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