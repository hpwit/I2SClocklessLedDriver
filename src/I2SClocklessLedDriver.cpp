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
 * DMA buffer depth, colour order).  Waits for any in-flight transfer, frees buffers,
 * applies the new configuration, then reallocates — same sequence as initled() but
 * skipping hardware init (hwInit) and gamma reset.  Leaves the object
 * unchanged if arguments are invalid or if the DMA wait times out.
 *
 * Platform notes:
 *   ESP32/S3 — waits for in-flight DMA via waitDisp semaphore (released by ISR).
 *   P4       — isDisplaying is always false (hwStop is synchronous), so the wait
 *              block is a no-op; execution falls straight through to deleteDriver().
 */
void I2SClocklessLedDriver::updateDriver(uint8_t* pinsq, uint16_t* sizes, uint8_t numStrips, uint8_t dmaBuffer, uint8_t channelsPerLight, uint8_t offsetRed, uint8_t offsetGreen, uint8_t offsetBlue, uint8_t offsetWhite, uint8_t offsetWhite2, bool extractWhiteFromRGB) {
  if (pinsq == nullptr || sizes == nullptr || numStrips == 0 || numStrips > MAX_PINS || dmaBuffer == 0) {
    ESP_LOGE(TAG, "updateDriver: invalid args numStrips=%u dmaBuffer=%u sizes=%p pinsq=%p", numStrips, dmaBuffer, (void*)sizes, (void*)pinsq);
    return;  // leave driver in previous consistent state
  }

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

  // Free old transfer buffers BEFORE updating geometry (deleteBuffers uses old nbDmaBuffer and numLedPerStrip)
  // deleteBuffers() resets error state at the start
  deleteBuffers();

  // Apply all configuration (geometry, color, pins, timing) from new arguments
  applyConfiguration(pinsq, sizes, numStrips, dmaBuffer, channelsPerLight, offsetRed, offsetGreen, offsetBlue, offsetWhite, offsetWhite2, extractWhiteFromRGB);

  // Reallocate transfer buffers with new geometry
  initBuffers();

  if (initErrorOccurred) {
    // Free any partial allocations made by initBuffers() before it failed.
    // deleteBuffers() resets initErrorOccurred; restore it so the caller can
    // detect the failure via initSuccess == false.
    deleteBuffers();
    initErrorOccurred = true;
    initSuccess = false;
    return;
  }

  // Restore brightness
  setBrightness(brightness);

  if (initErrorOccurred) {
    initSuccess = false;
    return;
  }

  initSuccess = !initErrorOccurred && numStrips > 0 && numLedPerStrip > 0;
  ESP_LOGD(TAG, "updateDriver %d x %d (%d)", numStrips, numLedPerStrip, nbDmaBuffer);
}

/** deleteBuffers — frees all runtime buffers; symmetric with initBuffers().
 *  Frees: transfer buffers, FULL_DMA buffers, sprites (HARDWARESPRITES), hmap (__HARDWARE_MAP).
 *  Does NOT free semaphores — they persist across reconfiguration.
 *  Resets error flags at start so retry after a failed operation works correctly.
 *  Does not affect hardware state; assumes hardware is already idle or stopped separately. */
void I2SClocklessLedDriver::deleteBuffers() {
  // Reset error state so reconfiguration/retry works correctly
  initErrorOccurred = false;
  initSuccess = false;
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3
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
    if ((err = parlio_tx_unit_wait_all_done(p4TxUnit, portMAX_DELAY)) != ESP_OK) ESP_LOGE(TAG, "deleteBuffers: parlio_tx_unit_wait_all_done failed: %s", esp_err_to_name(err));
    if ((err = parlio_tx_unit_disable(p4TxUnit)) != ESP_OK) ESP_LOGE(TAG, "deleteBuffers: parlio_tx_unit_disable failed: %s", esp_err_to_name(err));
    if ((err = parlio_del_tx_unit(p4TxUnit)) != ESP_OK) ESP_LOGE(TAG, "deleteBuffers: parlio_del_tx_unit failed: %s", esp_err_to_name(err));
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
}

/** deleteDriver — tears down all hardware resources and frees all buffers.
 *  Safe to call multiple times (all handles/pointers are nulled after free).
 *  After this call the driver is fully quiesced; hwInit() + initBuffers()
 *  are required before the next showPixels(). */
void I2SClocklessLedDriver::deleteDriver() {
  initErrorOccurred = false;
  initSuccess = false;

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3
  // Disable interrupts first to prevent ISR from running while we tear down hardware.
  // This avoids race conditions where the ISR tries to access registers/buffers being freed.
  #ifdef CONFIG_IDF_TARGET_ESP32
  if (intrHandle != nullptr) {
    esp_intr_disable(intrHandle);  // Disable before freeing to stop ISR execution
  }
  #elif CONFIG_IDF_TARGET_ESP32S3
  if (dmaChan != nullptr) {
    // Disable GDMA interrupt callback to prevent ISR firing during teardown
    gdma_tx_event_callbacks_t nullCbs = {.on_trans_eof = NULL, .on_descr_err = NULL};
    gdma_register_tx_event_callbacks(dmaChan, &nullCbs, this);
  }
  #endif

  // Brief delay to ensure any in-flight ISR completes gracefully
  vTaskDelay(pdMS_TO_TICKS(5));

  // Tear down hardware before freeing DMA buffers so the peripheral cannot
  // continue to access memory that is about to be freed.
  #ifdef CONFIG_IDF_TARGET_ESP32
  if (intrHandle != nullptr) {
    esp_intr_free(intrHandle);
    intrHandle = nullptr;
  }
  // Reset and disable the I2S peripheral completely
  periph_module_disable(I2S_DEVICE == 0 ? PERIPH_I2S0_MODULE : PERIPH_I2S1_MODULE);
  #elif CONFIG_IDF_TARGET_ESP32S3
  if (dmaChan != nullptr) {
    gdma_disconnect(dmaChan);
    gdma_del_channel(dmaChan);
    dmaChan = nullptr;
  }
  // Reset and disable the LCD_CAM peripheral completely - update : this caused watchdogs, as recreation seems to fail... periph_module_reset(PERIPH_LCD_CAM_MODULE);
  periph_module_disable(PERIPH_LCD_CAM_MODULE);
  vTaskDelay(pdMS_TO_TICKS(50));
  #endif
#elif CONFIG_IDF_TARGET_ESP32P4
  // P4: wait for PARLIO to finish before cleanup
  #if HAS_PARLIO_DRIVER
  if (p4TxUnit != NULL) {
    esp_err_t err;
    if ((err = parlio_tx_unit_wait_all_done(p4TxUnit, portMAX_DELAY)) != ESP_OK) ESP_LOGE(TAG, "deleteDriver: parlio_tx_unit_wait_all_done failed: %s", esp_err_to_name(err));
    if ((err = parlio_tx_unit_disable(p4TxUnit)) != ESP_OK) ESP_LOGE(TAG, "deleteDriver: parlio_tx_unit_disable failed: %s", esp_err_to_name(err));
  }
  #endif
#endif

  // Free all buffers (transfer buffers, FULL_DMA buffers, sprites, hmap)
  deleteBuffers();

  // Free semaphores (only on full teardown, not on buffer reallocation)
  if (waitDisp) {
    vSemaphoreDelete(waitDisp);
    waitDisp = NULL;
  }
}

/** initBuffers — allocate all runtime buffers from current geometry.
 *  Symmetric with deleteBuffers(): everything freed there is (re)allocated here.
 *  Called after applyConfiguration() so geometry/color members are already set.
 *  Used by both initled() (full initialization) and updateDriver() (reconfiguration). */
void I2SClocklessLedDriver::initBuffers() {
  setPins(this->pins);

#if HARDWARESPRITES == 1
  target = (uint16_t*)malloc(numLedPerStrip * numStrips * 2 + 2);
  if (!target) {
    ESP_LOGE(TAG, "initBuffers: failed to allocate hardware sprite target buffer");
    initErrorOccurred = true;
    return;
  }
#endif

#ifdef __HARDWARE_MAP
  #ifndef __NON_HEAP
  hmap = (uint32_t*)malloc(totalLeds * sizeof(*hmap));
  if (!hmap) {
    ESP_LOGE(TAG, "initBuffers: failed to allocate hardware map buffer");
    initErrorOccurred = true;
    return;
  }
  #endif
  if (!hmap) {
    ESP_LOGE(TAG, "initBuffers: no memory for hmap");
    initErrorOccurred = true;
    return;
  }
  createhardwareMap();
#endif

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3
  #ifdef CONFIG_IDF_TARGET_ESP32  // d0-wrover crashes with memory region error if set in PSRAM
  transferBuffers = (I2SClocklessLedDriverDMABuffer**)heap_caps_calloc_prefer(nbDmaBuffer + 2, sizeof(I2SClocklessLedDriverDMABuffer*), 2, MALLOC_CAP_DEFAULT, MALLOC_CAP_DEFAULT);
  #elif CONFIG_IDF_TARGET_ESP32S3
  transferBuffers = (I2SClocklessLedDriverDMABuffer**)heap_caps_calloc_prefer(nbDmaBuffer + 2, sizeof(I2SClocklessLedDriverDMABuffer*), 2, MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT);
  #endif
  if (!transferBuffers) {
    ESP_LOGE(TAG, "initBuffers: failed to allocate transferBuffers array");
    initErrorOccurred = true;
    return;
  }
  for (int i = 0; i < nbDmaBuffer + 1; i++) {
    transferBuffers[i] = allocateDMABuffer(channelsPerLight * 8 * 2 * 3);
    if (!transferBuffers[i]) {
      ESP_LOGE(TAG, "initBuffers: failed to allocate transferBuffers[%d]", i);
      initErrorOccurred = true;
      return;
    }
  }
  transferBuffers[nbDmaBuffer + 1] = allocateDMABuffer(channelsPerLight * 8 * 2 * 3 * 4);
  if (!transferBuffers[nbDmaBuffer + 1]) {
    ESP_LOGE(TAG, "initBuffers: failed to allocate transferBuffers[%d]", nbDmaBuffer + 1);
    initErrorOccurred = true;
    return;
  }
  for (int i = 0; i < nbDmaBuffer; i++) {
    putdefaultones((uint16_t*)transferBuffers[i]->buffer);
  }

  #ifdef FULL_DMA_BUFFER
  /*
   * Create n+2 buffers: buffer[0] ensures lines start at zero; buffer[n+1] is longer
   * so the I2S returns to zero with enough inter-frame gap for LOOP mode.
   */
  dmaBuffersTransposed = (I2SClocklessLedDriverDMABuffer**)malloc(sizeof(I2SClocklessLedDriverDMABuffer*) * (numLedPerStrip + 2));
  if (!dmaBuffersTransposed) {
    ESP_LOGE(TAG, "initBuffers: failed to allocate dmaBuffersTransposed array");
    initErrorOccurred = true;
    return;
  }

  for (int i = 0; i < numLedPerStrip + 2; i++) {
    if (i < numLedPerStrip + 1)
      dmaBuffersTransposed[i] = allocateDMABuffer(channelsPerLight * 8 * 2 * 3);
    else
      dmaBuffersTransposed[i] = allocateDMABuffer(channelsPerLight * 8 * 2 * 3 * 4);

    if (!dmaBuffersTransposed[i]) {
      ESP_LOGE(TAG, "initBuffers: failed to allocate dmaBuffersTransposed[%d]", i);
      // Clean up previously allocated buffers
      for (int j = 0; j < i; j++) {
        if (dmaBuffersTransposed[j]) {
          if (dmaBuffersTransposed[j]->buffer) heap_caps_free(dmaBuffersTransposed[j]->buffer);
          heap_caps_free(dmaBuffersTransposed[j]);
        }
      }
      free(dmaBuffersTransposed);
      dmaBuffersTransposed = nullptr;
      initErrorOccurred = true;
      return;
    }

    #ifdef CONFIG_IDF_TARGET_ESP32
      if (i < numLedPerStrip) dmaBuffersTransposed[i]->descriptor.eof = 0;
      if (i > 0) {
        dmaBuffersTransposed[i - 1]->descriptor.qe.stqe_next = &(dmaBuffersTransposed[i]->descriptor);
        if (i < numLedPerStrip + 1) {
          putdefaultones((uint16_t*)dmaBuffersTransposed[i]->buffer);
        }
      }
    #elif CONFIG_IDF_TARGET_ESP32S3
      if (i < numLedPerStrip) dmaBuffersTransposed[i]->dw0.suc_eof = 0;
      if (i > 0) {
        dmaBuffersTransposed[i - 1]->next = dmaBuffersTransposed[i];
        if (i < numLedPerStrip + 1) {
          putdefaultones((uint16_t*)dmaBuffersTransposed[i]->buffer);
        }
      }
    #endif
  }
  #endif

#elif CONFIG_IDF_TARGET_ESP32P4
  if (!p4Buffer1) {
    p4Buffer1 = (uint16_t*)heap_caps_calloc_prefer(PARLIO_P4_BUFFER_BYTES, 1, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_CACHE_ALIGNED, MALLOC_CAP_DMA);
    if (!p4Buffer1) {
      ESP_LOGE(TAG, "initBuffers: failed to allocate p4Buffer1 — out of memory");
      initErrorOccurred = true;
      return;
    }
  }
  if (!p4Buffer2) {
    p4Buffer2 = (uint16_t*)heap_caps_calloc_prefer(PARLIO_P4_BUFFER_BYTES, 1, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_CACHE_ALIGNED, MALLOC_CAP_DMA);
    if (!p4Buffer2) {
      ESP_LOGE(TAG, "initBuffers: failed to allocate p4Buffer2 — out of memory");
      heap_caps_free(p4Buffer1);
      p4Buffer1 = nullptr;
      p4BufferActive = nullptr;
      initErrorOccurred = true;
      return;
    }
  }
  p4BufferActive = p4Buffer1;

  #if !HAS_PARLIO_DRIVER
  ESP_LOGE(TAG, "PARLIO driver not available — ESP-IDF v5.1+ required for ESP32-P4 support");
  initErrorOccurred = true;
  return;
  #else
  {
    uint8_t outputs = numStrips;
    if (outputs > SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH) {
      ESP_LOGE(TAG, "initBuffers: numStrips (%u) exceeds SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH (%u)", outputs, SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH);
      initErrorOccurred = true;
      return;
    }
    const uint16_t max_leds = numLedPerStrip;

    p4Config.clk_src = PARLIO_CLK_SRC_DEFAULT;
    if (outputs <= 1)
      p4Config.data_width = 1;
    else if (outputs <= 2)
      p4Config.data_width = 2;
    else if (outputs <= 4)
      p4Config.data_width = 4;
    else if (outputs <= 8)
      p4Config.data_width = 8;
    else
      p4Config.data_width = 16;

    const uint32_t required_bytes = ((uint32_t)max_leds * channelsPerLight * 32u * p4Config.data_width + 7u) / 8u;
    if (required_bytes > PARLIO_P4_BUFFER_BYTES) {
      ESP_LOGE(TAG, "initBuffers: configuration requires %u bytes, but only %u are allocated", (unsigned)required_bytes, (unsigned)PARLIO_P4_BUFFER_BYTES);
      initErrorOccurred = true;
      return;
    }

    p4Config.clk_in_gpio_num = gpio_num_t(-1);
    p4Config.valid_gpio_num = gpio_num_t(-1);
    p4Config.clk_out_gpio_num = gpio_num_t(-1);

    for (int i = 0; i < SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH; ++i) {
      p4Config.data_gpio_nums[i] = (i < outputs) ? gpio_num_t(pins[i]) : gpio_num_t(-1);
    }

    #ifdef PARLIO_AUTO_OVERCLOCK
    if (max_leds <= 256)
      p4Config.output_clk_freq_hz = 1200000u * 4u;
    else if (max_leds <= 512)
      p4Config.output_clk_freq_hz = 1100000u * 4u;
    else
      p4Config.output_clk_freq_hz = 800000u * 4u;
    #else
    p4Config.output_clk_freq_hz = 800000u * 4u;
    #endif
    p4Config.valid_start_delay = 0;
    p4Config.valid_stop_delay = 0;
    p4Config.dma_burst_size = 64;
    p4Config.trans_queue_depth = 16;
    p4Config.max_transfer_size = 65535;
    p4Config.flags.clk_gate_en = 0;
    p4Config.flags.io_loop_back = 0;
    p4Config.flags.allow_pd = 0;
    p4Config.flags.invert_valid_out = 0;

    // TX unit is created lazily on the first showPixels() call via
    // ensureParlioTxUnitInitialized().  deleteBuffers() already tore down any
    // previous unit before initBuffers() was called, so p4TxUnit is NULL here.
    p4TxUnit = NULL;

    ESP_LOGD(TAG, "PARLIO config prepared (%u outputs, %u LEDs/output) — TX unit deferred to first use", (unsigned)outputs, (unsigned)max_leds);
  }
  #endif
  return;  // P4 done
#endif
}

/** applyConfiguration — applies all configuration from arguments to member variables.
 *  Sets geometry (strips, sizes, LED counts, offsets), color order, DMA buffer count,
 *  and pin array.  Does NOT touch hardware, allocate buffers, or change gamma/leds.
 *  Called by initled() (preserves nbDmaBuffer default) and updateDriver() (uses new dmaBuffer). */
void I2SClocklessLedDriver::applyConfiguration(uint8_t* pinsq, uint16_t* sizes, uint8_t numStrips, uint8_t dmaBuffer, uint8_t channelsPerLight, uint8_t offsetRed, uint8_t offsetGreen, uint8_t offsetBlue, uint8_t offsetWhite, uint8_t offsetWhite2, bool extractWhiteFromRGB) {
  this->numStrips = numStrips;
  totalLeds = 0;
  firstIndexPerOutput[0] = 0;
  for (int i = 0; i < numStrips; i++) {
    stripSize[i] = sizes[i];
    totalLeds += sizes[i];
    if (i > 0) firstIndexPerOutput[i] = firstIndexPerOutput[i - 1] + sizes[i - 1];
  }
  this->numLedPerStrip = maxLength(sizes, numStrips);
  offsetDisplay.offsetx = 0;
  offsetDisplay.offsety = 0;
  offsetDisplay.panelWidth = this->numLedPerStrip;
  offsetDisplay.panelHeight = 9999;
  defaultOffsetDisplay = offsetDisplay;
  linewidth = this->numLedPerStrip;
  nbDmaBuffer = dmaBuffer;
  this->channelsPerLight = channelsPerLight;
  this->offsetRed = offsetRed;
  this->offsetGreen = offsetGreen;
  this->offsetBlue = offsetBlue;
  this->offsetWhite = offsetWhite;
  this->offsetWhite2 = offsetWhite2;
  this->extractWhiteFromRGB = extractWhiteFromRGB;

  // Clear all pin slots first to avoid stale values when shrinking or re-using
  memset(this->pins, 0, MAX_PINS * sizeof(uint8_t));
  for (int i = 0; i < numStrips && i < MAX_PINS; i++) {
    this->pins[i] = pinsq[i];
  }

  setShowDelay();
  ESP_LOGD(TAG, "applyConfiguration %d strips x %d leds (dmaBuffer=%d)", numStrips, this->numLedPerStrip, dmaBuffer);
}