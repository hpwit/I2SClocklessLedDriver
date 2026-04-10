/**
    @title     I2SClocklessLedDriver
    @file      i2s_impl.h
    @repo      https://github.com/hpwit/I2SClocklessLedDriver
    @Copyright © 2025 Yves Bazin
    @license   MIT License
**/

// Out-of-class definitions for I2SClocklessLedDriver ESP32 / ESP32-S3 methods.
// Included at the bottom of I2SClocklessLedDriver.h, after the class body.

#pragma once

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3

// Allocate and initialize one DMA descriptor + data buffer (platform-specific fields).
inline I2SClocklessLedDriver::I2SClocklessLedDriverDMABuffer* I2SClocklessLedDriver::allocateDMABuffer(int bytes) {
  // DMA descriptor structure (holds S3 dw0 or ESP32 descriptor fields)
  I2SClocklessLedDriverDMABuffer* transferBuffer = (I2SClocklessLedDriverDMABuffer*)heap_caps_malloc(sizeof(I2SClocklessLedDriverDMABuffer), MALLOC_CAP_DMA);
  if (!transferBuffer) {
    ESP_LOGE(TAG, "Failed to allocate transferBuffer");
    initErrorOccurred = true;
    return NULL;
  }

  transferBuffer->buffer = (uint8_t*)heap_caps_malloc(bytes, MALLOC_CAP_DMA);
  if (!transferBuffer->buffer) {
    ESP_LOGE(TAG, "Failed to allocate DMA buffer!");
    initErrorOccurred = true;
    free(transferBuffer);
    return NULL;
  }
  memset(transferBuffer->buffer, 0, bytes);
#ifdef CONFIG_IDF_TARGET_ESP32
  transferBuffer->descriptor.length = bytes;
  transferBuffer->descriptor.size = bytes;
  transferBuffer->descriptor.owner = 1;
  transferBuffer->descriptor.sosf = 1;
  transferBuffer->descriptor.buf = transferBuffer->buffer;
  transferBuffer->descriptor.offset = 0;
  transferBuffer->descriptor.empty = 0;
  transferBuffer->descriptor.eof = 1;
  transferBuffer->descriptor.qe.stqe_next = 0;
#elif CONFIG_IDF_TARGET_ESP32S3
  transferBuffer->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
  transferBuffer->dw0.size = bytes;
  transferBuffer->dw0.length = bytes;
  transferBuffer->dw0.suc_eof = 1;
#endif
  return transferBuffer;
}

// Fill buffer with default "1" bits for idle/sync phases (platform-specific bit layout).
inline void I2SClocklessLedDriver::putdefaultones(uint16_t* buffer) {
/*order to push the data to the pins
 0:D7
 1:1
 2:1
 3:0
 4:0
 5:D6
 6:D5
 7:1
 8:1
 9:0
 10:0
 11:D4
 12:D3
 13:1
 14:1
 15:0
 16:0
 17:D2
 18:D1
 19:1
 20:1
 21:0
 22:0
 23:D0
 */
#ifdef CONFIG_IDF_TARGET_ESP32
  for (int i = 0; i < channelsPerLight * 8 / 2; i++) {
    buffer[i * 6 + 1] = 0xffff;
    buffer[i * 6 + 2] = 0xffff;
  }
#elif CONFIG_IDF_TARGET_ESP32S3
  for (int i = 0; i < channelsPerLight * 8; i++) {
    buffer[i * 3 + 0] = 0xffff;
    // buffer[i * 6 + 2] = 0xffff;
  }
#endif
}

// Start hardware DMA transfer with resolved start-of-chain buffer (ping-pong or full-buffer mode).
inline void I2SClocklessLedDriver::hwStart() {
  // Resolve the start-of-chain DMA buffer.  In FULL_DMA_BUFFER mode the entire
  // frame is pre-transposed into dmaBuffersTransposed (transpose == false);
  // in normal ping-pong mode transferBuffers[nbDmaBuffer] is the sentinel entry
  // that points back to the ring head (transpose == true).
#ifdef FULL_DMA_BUFFER
  I2SClocklessLedDriverDMABuffer* startBuffer = transpose ? transferBuffers[nbDmaBuffer] : dmaBuffersTransposed[0];
#else
  I2SClocklessLedDriverDMABuffer* startBuffer = transferBuffers[nbDmaBuffer];
#endif

#ifdef CONFIG_IDF_TARGET_ESP32
  i2sReset();
  framesync = false;

  (&I2S0)->lc_conf.val = I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN | I2S_OUT_DATA_BURST_EN;

  (&I2S0)->out_link.addr = (uint32_t)&(startBuffer->descriptor);

  (&I2S0)->out_link.start = 1;

  (&I2S0)->int_clr.val = (&I2S0)->int_raw.val;

  (&I2S0)->int_clr.val = (&I2S0)->int_raw.val;
  (&I2S0)->int_ena.val = 0;

  /*
   If we do not use the regular showpixels, then no need to activate the interupt at the end of each pixels
   */
  // if(transpose)
  (&I2S0)->int_ena.out_eof = 1;

  (&I2S0)->int_ena.out_total_eof = 1;
  esp_intr_enable(intrHandle);

  // We start the I2S
  (&I2S0)->conf.tx_start = 1;
#elif CONFIG_IDF_TARGET_ESP32S3
  LCD_CAM.lcd_user.lcd_start = 0;
  gdma_reset(dmaChan);
  LCD_CAM.lcd_user.lcd_dout = 1;    // Enable data out
  LCD_CAM.lcd_user.lcd_update = 1;  // Update registers
  LCD_CAM.lcd_misc.lcd_afifo_reset = 1;

  //    memset(startBuffer->buffer,0,WS2812_DMA_DESCRIPTOR_BUFFER_MAX_SIZE);
  gdma_start(dmaChan, (intptr_t)startBuffer);  // Start DMA w/updated descriptor(s)
  // esp_intr_enable(dmaChan->intr);
  // vTaskDelay(1);                         // Must 'bake' a moment before...
  LCD_CAM.lcd_user.lcd_start = 1;
#endif
  // Set the mode to indicate that we've started
  isDisplaying = true;
}

// Reset I2S/GDMA hardware (platform-specific register sequence).
inline void IRAM_ATTR I2SClocklessLedDriver::i2sReset() {
#ifdef CONFIG_IDF_TARGET_ESP32
  const unsigned long lc_conf_reset_flags = I2S_IN_RST_M | I2S_OUT_RST_M | I2S_AHBM_RST_M | I2S_AHBM_FIFO_RST_M;
  (&I2S0)->lc_conf.val |= lc_conf_reset_flags;
  (&I2S0)->lc_conf.val &= ~lc_conf_reset_flags;
  const uint32_t conf_reset_flags = I2S_RX_RESET_M | I2S_RX_FIFO_RESET_M | I2S_TX_RESET_M | I2S_TX_FIFO_RESET_M;
  (&I2S0)->conf.val |= conf_reset_flags;
  (&I2S0)->conf.val &= ~conf_reset_flags;
#elif CONFIG_IDF_TARGET_ESP32S3
  gdma_reset(dmaChan);
  LCD_CAM.lcd_misc.lcd_afifo_reset = 1;
#endif
}

// Stop hardware DMA and release waiting tasks (called from ISR context).
static void IRAM_ATTR hwStop(I2SClocklessLedDriver* driver) {
#ifdef CONFIG_IDF_TARGET_ESP32
  esp_intr_disable(driver->intrHandle);

  esp_rom_delay_us(16);
  (&I2S0)->conf.tx_start = 0;
  while ((&I2S0)->conf.tx_start == 1) {
  }
#elif CONFIG_IDF_TARGET_ESP32S3

  // gdma_disconnect(dmaChan);
  LCD_CAM.lcd_user.lcd_start = 0;

  while (LCD_CAM.lcd_user.lcd_start) {
  }
  gdma_stop(dmaChan);
  // ets_delay_us(16);  // for sk6812
  esp_rom_delay_us(16);  // for sk6812
                         // esp_intr_disable(dmaChan->intr);
#endif
  driver->i2sReset();

  driver->isDisplaying = false;  // transfer complete

  portBASE_TYPE hpTaskAwoken = 0;  // track if ISR wakeup is needed
  if (driver->wasWaitingtofinish == true) {
    driver->wasWaitingtofinish = false;
    xSemaphoreGiveFromISR(driver->waitDisp, &hpTaskAwoken);
  }
  if (driver->isWaiting) {
    xSemaphoreGiveFromISR(driver->sem, &hpTaskAwoken);
  }
  if (hpTaskAwoken == pdTRUE) portYIELD_FROM_ISR();
}

#ifdef CONFIG_IDF_TARGET_ESP32
// I2S EOF interrupt callback (ESP32 variant): refill ping-pong buffer or stop on frame end.
static void IRAM_ATTR interruptHandler(void* arg) {
  #ifdef DO_NOT_USE_INTERUPT
  REG_WRITE(I2S_INT_CLR_REG(0), (REG_READ(I2S_INT_RAW_REG(0)) & 0xffffffc0) | 0x3f);
  return;
  #else
  I2SClocklessLedDriver* driver = (I2SClocklessLedDriver*)arg;

  if (!driver->enableDriver) {
    REG_WRITE(I2S_INT_CLR_REG(0), (REG_READ(I2S_INT_RAW_REG(0)) & 0xffffffc0) | 0x3f);
    // ((I2SClocklessLedDriver *)arg)->hwStop();
    hwStop(driver);
    return;
  }
  if (GET_PERI_REG_BITS(I2S_INT_ST_REG(I2S_DEVICE), I2S_OUT_EOF_INT_ST_S, I2S_OUT_EOF_INT_ST_S)) {
    driver->framesync = !driver->framesync;

    if (((I2SClocklessLedDriver*)arg)->transpose) {
      driver->ledToDisplay = driver->ledToDisplay + 1;  //++ gives volatile warning
      if (driver->ledToDisplay < driver->numLedPerStrip) {
        loadAndTranspose(driver);

        if (driver->ledToDisplayOut == driver->numLedPerStrip - driver->nbDmaBuffer)  // here it's not -1 because it takes time top have the change into account and it reread the buufer
        {
          driver->transferBuffers[(driver->dmaBufferActive) % driver->nbDmaBuffer]->descriptor.qe.stqe_next = &(driver->transferBuffers[driver->nbDmaBuffer + 1]->descriptor);
        }
        driver->dmaBufferActive = (driver->dmaBufferActive + 1) % driver->nbDmaBuffer;
      }
      driver->ledToDisplayOut = driver->ledToDisplayOut + 1;  //++ gives volatile warning
    } else {
      if (driver->framesync) {
        portBASE_TYPE hpTaskAwoken = 0;
        xSemaphoreGiveFromISR(driver->semSync, &hpTaskAwoken);
        if (hpTaskAwoken == pdTRUE) portYIELD_FROM_ISR();
      }
    }
  }

  if (GET_PERI_REG_BITS(I2S_INT_ST_REG(I2S_DEVICE), I2S_OUT_TOTAL_EOF_INT_ST_S, I2S_OUT_TOTAL_EOF_INT_ST_S)) {
    // ((I2SClocklessLedDriver *)arg)->hwStop();
    hwStop(driver);
    if (driver->isWaiting) {
      portBASE_TYPE hpTaskAwoken = 0;
      xSemaphoreGiveFromISR(driver->sem, &hpTaskAwoken);
      if (hpTaskAwoken == pdTRUE) portYIELD_FROM_ISR();
    }
  }
  REG_WRITE(I2S_INT_CLR_REG(0), (REG_READ(I2S_INT_RAW_REG(0)) & 0xffffffc0) | 0x3f);
  #endif
}
#elif CONFIG_IDF_TARGET_ESP32S3

// GDMA EOF interrupt callback (S3 variant): refill ping-pong buffer or stop on frame end.
static IRAM_ATTR bool interruptHandler(gdma_channel_handle_t dmaChan, gdma_event_data_t* eventData, void* userData) {
  // This DMA callback seems to trigger a moment before the last data has
  // issued (buffering between DMA & LCD peripheral?), so pause a moment
  // before stopping LCD data out. The ideal delay may depend on the LCD
  // clock rate...this one was determined empirically by monitoring on a
  // logic analyzer. YMMV.
  // vTaskDelay(100);
  // The LCD peripheral stops transmitting at the end of the DMA xfer, but
  // clear the lcd_start flag anyway -- we poll it in loop() to decide when
  // the transfer has finished, and the same flag is set later to trigger
  // the next transfer.
  I2SClocklessLedDriver* driver = (I2SClocklessLedDriver*)userData;

  if (!driver->enableDriver) {
    // driver->hwStop(driver);
    hwStop(driver);
    return true;
  }

  driver->framesync = !driver->framesync;

  // driver->ledToDisplay_in[driver->ledToDisplayOut]=driver->ledToDisplay+1;
  // driver->ledToDisplay_inbuffer[driver->ledToDisplayOut]=driver->dmaBufferActive;

  if (driver->transpose) {
    driver->ledToDisplay = driver->ledToDisplay + 1;
    if (driver->ledToDisplay < driver->numLedPerStrip) {
      loadAndTranspose(driver);

      if (driver->ledToDisplayOut == (driver->numLedPerStrip - driver->nbDmaBuffer))  // here it's not -1 because it takes time top have the change into account and it reread the buufer
      {
        driver->transferBuffers[(driver->dmaBufferActive) % driver->nbDmaBuffer]->next = (driver->transferBuffers[driver->nbDmaBuffer + 1]);
        // driver->ledToDisplay_inbufferfor[driver->ledToDisplayOut]=driver->dmaBufferActive;
      }

      driver->dmaBufferActive = (driver->dmaBufferActive + 1) % driver->nbDmaBuffer;
    }
    driver->ledToDisplayOut = driver->ledToDisplayOut + 1;
    if (driver->ledToDisplay >= driver->numLedPerStrip + driver->nbDmaBuffer + 1) {
      hwStop(driver);
    }
  } else {
    if (driver->framesync) {
      portBASE_TYPE hpTaskAwoken = 0;
      xSemaphoreGiveFromISR(driver->semSync, &hpTaskAwoken);
      if (hpTaskAwoken == pdTRUE) portYIELD_FROM_ISR();
    }
  }
  return true;
}
#endif

// Transpose one 8-bit colour channel from strip-parallel format to time-slice parallel format (platform-specific bit layout).
static void IRAM_ATTR transposeColorChannel(unsigned char* a, uint16_t* b, uint8_t numStrips) {
  uint32_t x, y, x1, y1, t;  // working registers for bit manipulation

  y = *reinterpret_cast<const unsigned int*>(a);

  if (numStrips > 4) {
    x = *reinterpret_cast<const unsigned int*>(a + 4);

    // pre-transform x
    t = (x ^ (x >> 7)) & AAA;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & CC;
    x = x ^ t ^ (t << 14);
  } else
    x = 0;

  if (numStrips > 8)
    y1 = *reinterpret_cast<const unsigned int*>(a + 8);
  else
    y1 = 0;

  if (numStrips > 12) {
    // pre-transform x
    x1 = *reinterpret_cast<const unsigned int*>(a + 12);
    t = (x1 ^ (x1 >> 7)) & AAA;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & CC;
    x1 = x1 ^ t ^ (t << 14);
  } else
    x1 = 0;

  // pre-transform y
  t = (y ^ (y >> 7)) & AAA;
  y = y ^ t ^ (t << 7);
  t = (y ^ (y >> 14)) & CC;
  y = y ^ t ^ (t << 14);

  if (numStrips > 8) {
    t = (y1 ^ (y1 >> 7)) & AAA;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & CC;
    y1 = y1 ^ t ^ (t << 14);
  }

  // final transform
  t = (x & FF) | ((y >> 4) & FF2);
  y = ((x << 4) & FF) | (y & FF2);
  x = t;

  t = (x1 & FF) | ((y1 >> 4) & FF2);
  y1 = ((x1 << 4) & FF) | (y1 & FF2);
  x1 = t;

#ifdef CONFIG_IDF_TARGET_ESP32

  *((uint16_t*)(b)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
  *((uint16_t*)(b + 5)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
  *((uint16_t*)(b + 6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
  *((uint16_t*)(b + 11)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
  *((uint16_t*)(b + 12)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
  *((uint16_t*)(b + 17)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
  *((uint16_t*)(b + 18)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
  *((uint16_t*)(b + 23)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#elif CONFIG_IDF_TARGET_ESP32S3
  *((uint16_t*)(b + 1)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
  *((uint16_t*)(b + 4)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
  *((uint16_t*)(b + 7)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
  *((uint16_t*)(b + 10)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
  *((uint16_t*)(b + 13)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
  *((uint16_t*)(b + 16)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
  *((uint16_t*)(b + 19)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
  *((uint16_t*)(b + 22)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif
}

// Load one LED row from leds[] buffer, apply LUT+reorder, and transpose to DMA buffer (ping-pong or full mode).
static void IRAM_ATTR loadAndTranspose(I2SClocklessLedDriver* driver)  // uint8_t *ledt, uint16_t *sizes, uint8_t num_stripst, uint16_t *buffer, int ledtodisp, uint8_t *mapg, uint8_t *mapr, uint8_t
                                                                       // *mapb, uint8_t *mapw, uint8_t channelsPerLight, int pr, int pg, int pb)
{
  // driver->leds, driver->stripSize, driver->numStrips, (uint16_t *)driver->transferBuffers[driver->dmaBufferActive]->buffer, driver->ledToDisplay, driver->redMap, driver->greenMap, driver->blueMap,
  // driver->whiteMap, driver->channelsPerLight, driver->offsetRed, driver->offsetGreen, driver->offsetBlue);
  Lines secondPixel[driver->channelsPerLight];  // temporary buffer for colour components (VLA)
  uint16_t* buffer = nullptr;  // points to active DMA buffer (resolved below)
  if (driver->transpose)
    buffer = (uint16_t*)driver->transferBuffers[driver->dmaBufferActive]->buffer;
  else
    buffer = (uint16_t*)driver->dmaBuffersTransposed[driver->dmaBufferActive]->buffer;
  if (buffer == nullptr) return;  // no DMA buffer on unsupported platforms

  uint16_t led_tmp = driver->ledToDisplay;
#ifdef __HARDWARE_MAP
      // led_tmp=driver->ledToDisplay*driver->numStrips;
#endif
  memset(secondPixel, 0, sizeof(secondPixel));
#ifdef _LEDMAPPING
  // #ifdef __SOFTWARE_MAP
  uint8_t* poli;
      // #endif
#else
  uint8_t* poli = driver->leds + driver->ledToDisplay * driver->channelsPerLight;
#endif
  for (int i = 0; i < driver->numStrips; i++) {
    if (driver->ledToDisplay < driver->stripSize[i]) {
#ifdef _LEDMAPPING
  #ifdef __SOFTWARE_MAP
      poli = driver->leds + driver->mapLed(led_tmp) * driver->channelsPerLight;
  #endif
  #ifdef __HARDWARE_MAP
      poli = driver->leds + *(driver->hmapOff);
  #endif
  #ifdef __HARDWARE_MAP_PROGMEM
      poli = driver->leds + pgm_read_word_near(driver->hmap + driver->hmapOff);
  #endif
#endif
      // Apply LUT tables + white extraction + channel reorder (Phase 9: unified method, called on all platforms)
      uint8_t mapped[5] = {};  // temporary buffer holding mapped pixel in wire order (offsetRed/offsetGreen/offsetBlue/offsetWhite/offsetWhite2)
      driver->rgbwBufferMapping(poli, mapped);  // brightness/gamma LUT + white extraction + channel reorder
      // distribute mapped components into their respective colour channels
      for (int c = 0; c < driver->channelsPerLight; c++) secondPixel[c].bytes[i] = mapped[c];
#ifdef __HARDWARE_MAP
      driver->hmapOff++;
#endif
#ifdef __HARDWARE_MAP_PROGMEM
      driver->hmapOff++;
#endif
    }
#ifdef _LEDMAPPING
  #ifdef __SOFTWARE_MAP
    led_tmp += driver->stripSize[i];
  #endif
#else
    poli += driver->stripSize[i] * driver->channelsPerLight;
#endif
  }

  transposeColorChannel(secondPixel[0].bytes, (uint16_t*)buffer, driver->numStrips);
  transposeColorChannel(secondPixel[1].bytes, (uint16_t*)buffer + 3 * 8, driver->numStrips);
  transposeColorChannel(secondPixel[2].bytes, (uint16_t*)buffer + 2 * 3 * 8, driver->numStrips);
  if (driver->offsetWhite != UINT8_MAX) transposeColorChannel(secondPixel[3].bytes, (uint16_t*)buffer + 3 * 3 * 8, driver->numStrips);
  if (driver->offsetWhite2 != UINT8_MAX) transposeColorChannel(secondPixel[4].bytes, (uint16_t*)buffer + 4 * 3 * 8, driver->numStrips);
}

#endif  // CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3
