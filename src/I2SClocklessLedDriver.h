/**
    @title     I2SClocklessLedDriver
    @file      I2SClocklessLedDriver.h
    @repo      https://github.com/hpwit/I2SClocklessLedDriver
    @Copyright © 2025 Yves Bazin
    @license   MIT License
    @license   For non MIT usage, commercial licenses must be purchased. Contact us for more information.
**/

/* library options
 *  IDF5.5 check will be used to track recent changes which work in IDF 5.5, maybe / probably also before but this ensures we do not break things on older versions
 *  <IDF5.5: NUMSTRIPS add this before the #include of the library this will help with the speed of the buffer calculation
 *  numStrips is a class member of I2SClocklessLedDriver, set in initled and updated when strip count changes
 *
 *  ENABLE_HARDWARE_SCROLL : to enable the HARDWARE SCROLL. Attention wjhen enabled you can use the offset  but it could mean slow when using all the pins
 *  USE_PIXELSLIB : to use tthe pixel lib library automatic functions
 */

#ifndef I2S_CLOCKLESS_DRIVER_H
#define I2S_CLOCKLESS_DRIVER_H

#pragma once

#include "freertos/FreeRTOS.h"  // #error "include FreeRTOS.h" must appear in source files before "include semphr.h"

#ifdef CONFIG_IDF_TARGET_ESP32P4
  // ESP32-P4 uses the PARLIO peripheral — no I2S/DMA headers needed.
  #include "esp_heap_caps.h"
  #include "esp_log.h"
  #include "esp_rom_sys.h"
  #include "freertos/semphr.h"
  #include "freertos/task.h"

  // PARLIO driver (requires ESP-IDF v5.1+)
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    #include "driver/parlio_tx.h"
    #define HAS_PARLIO_DRIVER 1
  #else
    #warning "ESP32-P4 PARLIO support requires ESP-IDF v5.1 or later"
    #define HAS_PARLIO_DRIVER 0
  #endif

/** Shared waveform-buffer size used by both the allocator (initTransferBuffers)
 *  and the capacity check (loadAndTranspose).
 *  Formula: max_leds × max_components × 32 ticks × max_data_width_bits / 8
 *           = 1024 × 5 × 32 × 16 / 8 = 327,680 bytes */
static constexpr uint32_t PARLIO_P4_BUFFER_BYTES = 1024u * 5u * 32u * 16u / 8u;
#else
  // IDF5.5: replace #include driver by #include esp_private
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    #include <esp_private/gpio.h>
    #include <esp_private/periph_ctrl.h>
  #else
    #include <driver/periph_ctrl.h>

    #include "driver/gpio.h"
  #endif
#endif  // CONFIG_IDF_TARGET_ESP32P4

#ifdef CONFIG_IDF_TARGET_ESP32S3

  #define GDMA_OUT_INT_CLR_REG(i) (DR_REG_GDMA_BASE + 0x74 + (192 * i))
  #define GDMA_OUT_INT_ENA_REG(i) (DR_REG_GDMA_BASE + 0x70 + (192 * i))
  #define GDMA_OUT_INT_ST_REG(i) (DR_REG_GDMA_BASE + 0x6c + (192 * i))
  #include "esp_check.h"
  #include "esp_err.h"
  #include "esp_log.h"
  #include "rom/cache.h"
  // void gdma_default_tx_isr(void *args);
  #include <stdio.h>

  #include "esp_heap_caps.h"
  #include "freertos/semphr.h"

  // #include "esp32-hal-log.h"//
  #include <esp_private/gdma.h>
  #include <hal/dma_types.h>
  #include <hal/gdma_types.h>
  #include <hal/gpio_hal.h>
  #include <soc/gdma_channel.h>
  #include <soc/lcd_cam_struct.h>
  #include <stdbool.h>

  #include "freertos/task.h"
  // #include "hal/gpio_ll.h"
  #include <hal/gdma_hal.h>

  #include "esp_log.h"
  #include "esp_rom_gpio.h"
  #include "hal/gdma_ll.h"
  #include "soc/gdma_periph.h"
  #include "soc/periph_defs.h"
  #include "soc/soc_caps.h"
  // #endif
  #ifdef OVER_CLOCK_MAX
    #define CLOCK_DIV_NUM 4
    #define CLOCK_DIV_A 20
    #define CLOCK_DIV_B 9
  #endif
  #ifdef OVERCLOCK_1MHZ
    #define CLOCK_DIV_NUM 5
    #define CLOCK_DIV_A 1
    #define CLOCK_DIV_B 0
  #endif
  #ifdef OVERCLOCK_1_1MHZ
    #define CLOCK_DIV_NUM 4
    #define CLOCK_DIV_A 8
    #define CLOCK_DIV_B 4
  #endif
  #ifndef CLOCK_DIV_NUM
    #define CLOCK_DIV_NUM 6
    #define CLOCK_DIV_A 4
    #define CLOCK_DIV_B 1
  #endif

typedef struct {
  int divNum;
  int divA;
  int divB;
} clock_speed;

// defined in .cpp (not needed for physical driver ...)
extern clock_speed clock1123Khz;
extern clock_speed clock1111Khz;
extern clock_speed clock1000Khz;
extern clock_speed clock800Khz;

  #define WS2812_DMA_DESCRIPTOR_BUFFER_MAX_SIZE (576 * 2)

#elif CONFIG_IDF_TARGET_ESP32
  #include <rom/ets_sys.h>
  #include <stdio.h>

  #include <cstring>

  #include "esp_heap_caps.h"
  #include "freertos/semphr.h"
  #include "freertos/task.h"
  #include "rom/lldesc.h"
  #include "soc/gpio_sig_map.h"
  #include "soc/i2s_reg.h"
  #include "soc/i2s_struct.h"
  #include "soc/io_mux_reg.h"
  #include "soc/soc.h"
  // #include "esp32-hal-log.h"

  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    #include "esp_rom_sys.h"
    #include "hal/gpio_ll.h"
    #include "rom/gpio.h"
    #include "soc/gpio_struct.h"
  #endif

#endif

#include "esp_log.h"
#include "helper.h"
#include "math.h"

#ifndef SNAKEPATTERN
  #define SNAKEPATTERN 1
#endif

#ifndef ALTERNATEPATTERN
  #define ALTERNATEPATTERN 1
#endif

#define I2S_DEVICE 0

#define AAA (0x00AA00AAL)
#define CC (0x0000CCCCL)
#define FF (0xF0F0F0F0L)
#define FF2 (0x0F0F0F0FL)

#ifndef MIN
  #define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef HARDWARESPRITES
  #define HARDWARESPRITES 0
#endif

#if HARDWARESPRITES == 1
  #include "HardwareSprite.h"
#endif

#ifdef USE_PIXELSLIB
  #include "pixelslib.h"
#else
  #include "pixeltypes.h"
#endif

#include "framebuffer.h"

#ifdef __HARDWARE_MAP
  #define _LEDMAPPING
#endif
#ifdef __SOFTWARE_MAP
  #define _LEDMAPPING
#endif
#ifdef __HARDWARE_MAP_PROGMEM
  #define _LEDMAPPING
#endif
// #define FULL_DMA_BUFFER

#define MAX_PINS 20  // maximum number of pins supported, 🌙 was 16, set to 20, okay?

typedef union {
  uint8_t bytes[16];
  uint32_t shorts[8];
  uint32_t raw[2];
} Lines;

#ifdef CONFIG_IDF_TARGET_ESP32S3
static uint8_t signalsID[MAX_PINS] = {
    LCD_DATA_OUT0_IDX, LCD_DATA_OUT1_IDX, LCD_DATA_OUT2_IDX, LCD_DATA_OUT3_IDX, LCD_DATA_OUT4_IDX, LCD_DATA_OUT5_IDX, LCD_DATA_OUT6_IDX, LCD_DATA_OUT7_IDX, LCD_DATA_OUT8_IDX, LCD_DATA_OUT9_IDX, LCD_DATA_OUT10_IDX, LCD_DATA_OUT11_IDX, LCD_DATA_OUT12_IDX, LCD_DATA_OUT13_IDX, LCD_DATA_OUT14_IDX, LCD_DATA_OUT15_IDX,

};
static gdma_channel_handle_t dmaChan;
#endif

class I2SClocklessLedDriver;

struct OffsetDisplay {
  int offsetx;
  int offsety;
  int panelHeight;
  int panelWidth;
};

// static const char *TAG = "I2SClocklessLedDriver";
#undef TAG
#define TAG "🐸"

#ifdef CONFIG_IDF_TARGET_ESP32S3
static bool interruptHandler(gdma_channel_handle_t dmaChan, gdma_event_data_t* eventData, void* userData);
#else
static void interruptHandler(void* arg);
#endif

static void transpose16x1Noinline2(unsigned char* a, uint16_t* b, uint8_t numStrips);

/*
#ifdef ENABLE_HARDWARE_SCROLL
    static void loadAndTranspose(uint8_t *ledt, int led_per_strip, uint8_t num_stripst, OffsetDisplay offdisp, uint16_t *buffer, int ledtodisp, uint8_t *mapg, uint8_t *mapr, uint8_t *mapb, uint8_t
*mapw, uint8_t driver->channelsPerLight, uint8_t pr, uint8_t pg, uint8_t pb); #else static void loadAndTranspose(uint8_t *ledt, uint16_t *sizes, uint8_t num_stripst, uint16_t *buffer, int ledtodisp, uint8_t *mapr, uint8_t *mapg,
uint8_t *mapb, uint8_t *mapw, uint8_t driver->channelsPerLight, uint8_t pr, uint8_t pg, uint8_t pb); #endif
*/

#ifndef CONFIG_IDF_TARGET_ESP32P4
static void loadAndTranspose(I2SClocklessLedDriver* driver);
#endif

#include "colorarrangement.h"

enum DisplayMode {
  NO_WAIT,
  WAIT,
  LOOP,
  LOOP_INTERUPT,
};
/*
int MOD(in
    if (a < 0)
    {
        if (-a % b == 0)
            return 0;
        else
            return b - (-a) % b;
    }
    else
        return a % b;
}
*/

struct LedTiming {
  // led timing
  uint32_t t0;
  uint32_t t1;
  uint32_t t2;

  // compileled
  uint8_t f1;
  uint8_t f2;
  uint8_t f3;
};

/**
 * I2SClocklessLedDriver — parallel LED strip driver for ESP32 / ESP32-S3.
 *
 * Drives up to 16 WS2812/WS2813/WS2815 (RGB) or SK6812 (RGBW) strips in
 * parallel using the I2S peripheral + DMA.  The CPU is not involved during
 * transmission; in FULL_DMA_BUFFER + LOOP mode the hardware runs entirely
 * autonomously.
 *
 * Typical usage:
 *   1. Call initled() once to configure pins, strip count, and colour order.
 *   2. Fill the leds[] byte array (R,G,B per pixel, strips laid out sequentially).
 *   3. Call showPixels() to push the frame.
 */
class I2SClocklessLedDriver {
#ifdef CONFIG_IDF_TARGET_ESP32
  struct I2SClocklessLedDriverDMABuffer {
    lldesc_t descriptor;
    uint8_t* buffer;
  };

  const int deviceBaseIndex[2] = {I2S0O_DATA_OUT0_IDX, I2S1O_DATA_OUT0_IDX};
  const int deviceClockIndex[2] = {I2S0O_BCK_OUT_IDX, I2S1O_BCK_OUT_IDX};
  const int deviceWordSelectIndex[2] = {I2S0O_WS_OUT_IDX, I2S1O_WS_OUT_IDX};
  const periph_module_t deviceModule[2] = {PERIPH_I2S0_MODULE, PERIPH_I2S1_MODULE};
#endif

 public:
#ifdef CONFIG_IDF_TARGET_ESP32
  i2s_dev_t* i2s;
#endif
  uint8_t* redMap = nullptr;
  uint8_t* greenMap = nullptr;
  uint8_t* blueMap = nullptr;
  uint8_t* whiteMap = nullptr;
  uint8_t* white2Map = nullptr;
  uint8_t brightness = 255;
  float gammar = 1.0f, gammag = 1.0f, gammab = 1.0f, gammaw = 1.0f, gammaw2 = 1.0f;
  bool extractWhiteFromRGB = true;  // 🌙
  bool initErrorOccurred = false;   // Set to true on any allocation failure; reset at start of initLedImpl()
  bool initSuccess = false;         // Set to true at the end of initLedImpl() only if all allocations succeeded
  intr_handle_t intrHandle = nullptr;
  volatile SemaphoreHandle_t sem = NULL;
  volatile SemaphoreHandle_t semSync = NULL;
  volatile SemaphoreHandle_t semDisp = NULL;
  volatile SemaphoreHandle_t waitDisp = NULL;
  volatile uint8_t dmaBufferActive = 0;
  volatile bool wait = false;
  DisplayMode displayMode = NO_WAIT;
  DisplayMode defaultDisplayMode = NO_WAIT;
  volatile uint16_t ledToDisplay = 0;
  volatile uint16_t ledToDisplayOut = 0;
  OffsetDisplay offsetDisplay = {0, 0, 0, 0};
  OffsetDisplay defaultOffsetDisplay = {0, 0, 0, 0};
  // volatile int oo=0;
  uint8_t *leds = nullptr, *saveleds = nullptr;
  uint16_t linewidth = 0;
  uint8_t nbDmaBuffer = 6;
  volatile bool transpose = false;

  volatile uint8_t numStrips = 0;
  volatile uint16_t numLedPerStrip = 0;
  volatile uint32_t totalLeds = 0;
  // int clock_pin;
  uint8_t pR = 0, pG = 1, pB = 2, pW = UINT8_MAX, pW2 = UINT8_MAX;
  int i2sBasePinIndex = 0;
  uint8_t channelsPerLight = 3;  // channels per LED
  uint16_t stripSize[MAX_PINS] = {};
  uint32_t (*mapLed)(uint32_t led) = nullptr;

  // Virtual driver — multiplexed LED strips via 74HC595 shift registers.
  // Set isVirtualDriver = true plus clockPin/latchPin before calling initled();
  // the driver will branch on this flag inside hwInit() and loadAndTranspose().
  // Currently ESP32/ESP32-S3 only; P4 support planned.
  bool isVirtualDriver = false;

  uint8_t virtualStripsPerPin = 0;  // typically 8 (one 74HC595 per physical pin); 0 = not configured
  uint8_t clockPin = 0;             // 74HC245 shift-register clock GPIO
  uint8_t latchPin = 0;             // 74HC245 shift-register latch GPIO

  TickType_t showDelay = 0;

  // GPIO pin numbers stored for the lifetime of the driver.
  // For ESP32/S3 the GPIO mux is configured once in setPins() and the hardware remembers it,
  // but storing the array here makes the driver self-contained and supports future use.
  // For P4 the pin array is also passed to the PARLIO unit on each topology change.
  uint8_t pins[MAX_PINS] = {};

  // Cumulative LED offset for each strip: firstIndexPerOutput[i] = sum of stripSize[0..i-1].
  // Populated by initLedImpl() and updateDriver().  Used by the P4 PARLIO transposition pass
  // to locate each strip's data in the flat leds[] buffer without per-LED pointer arithmetic.
  uint32_t firstIndexPerOutput[MAX_PINS] = {};

#ifdef CONFIG_IDF_TARGET_ESP32P4
  // PARLIO peripheral handle and configuration (only available in ESP-IDF v5.1+).
  #if HAS_PARLIO_DRIVER
  parlio_tx_unit_handle_t p4TxUnit = NULL;
  parlio_tx_unit_config_t p4Config = {};
  #endif

  // Ping-pong waveform buffers — allocated in initLedImpl(), freed in deleteDriver().
  uint16_t* p4Buffer1 = nullptr;
  uint16_t* p4Buffer2 = nullptr;
  uint16_t* p4BufferActive = nullptr;
#endif

#ifdef __HARDWARE_MAP
  uint32_t* hmap;
  volatile uint32_t* hmapOff;
  void setHmap(uint32_t* map) { hmap = map; }
#endif

#ifdef __HARDWARE_MAP_PROGMEM
  const uint32_t* hmap;
  volatile uint32_t hmapOff;

  void setHmap(const uint32_t* map) { hmap = map; }
#endif

  void setMapLed(uint32_t (*newMapLed)(uint32_t led)) { mapLed = newMapLed; }

  /*
   This flag is used when using the NO_WAIT mode
   */
  volatile bool isDisplaying = false;
  volatile bool isWaiting = false;
  volatile bool enableDriver = true;
  volatile bool framesync = false;
  volatile bool wasWaitingtofinish = false;

  I2SClocklessLedDriver() = default;
  I2SClocklessLedDriver(const I2SClocklessLedDriver&) = delete;
  I2SClocklessLedDriver& operator=(const I2SClocklessLedDriver&) = delete;

  ~I2SClocklessLedDriver() {
    deleteDriver();
    if (redMap) {
      free(redMap);
      redMap = nullptr;
    }
    if (greenMap) {
      free(greenMap);
      greenMap = nullptr;
    }
    if (blueMap) {
      free(blueMap);
      blueMap = nullptr;
    }
    if (whiteMap) {
      free(whiteMap);
      whiteMap = nullptr;
    }
    if (white2Map) {
      free(white2Map);
      white2Map = nullptr;
    }
  }

  void setPins(uint8_t* pinsq) {
    for (int i = 0; i < numStrips && i < MAX_PINS; i++) this->pins[i] = pinsq[i];
#ifdef CONFIG_IDF_TARGET_ESP32
    for (int i = 0; i < numStrips; i++) {
      PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pinsq[i]], PIN_FUNC_GPIO);
      gpio_set_direction((gpio_num_t)pinsq[i], (gpio_mode_t)GPIO_MODE_DEF_OUTPUT);
      gpio_matrix_out(pinsq[i], deviceBaseIndex[I2S_DEVICE] + i + 8, false, false);
    }
#elif CONFIG_IDF_TARGET_ESP32S3
    for (int i = 0; i < numStrips; i++) {
      esp_rom_gpio_connect_out_signal(pinsq[i], signalsID[i], false, false);
        // gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[pinsq[i]], PIN_FUNC_GPIO);
        // gpio_hal_func_sel(GPIO_PIN_MUX_REG[pinsq[i]], PIN_FUNC_GPIO);

  // IDF5.5: 🌙 setPins: use gpio_iomux_output instead of gpio_iomux_out suppress warning, ready for idf 6, see https://github.com/espressif/esp-idf/issues/17052
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
      gpio_iomux_output((gpio_num_t)pinsq[i], PIN_FUNC_GPIO);
  #else
      gpio_iomux_out(pinsq[i], PIN_FUNC_GPIO, false);
  #endif

      gpio_set_drive_capability((gpio_num_t)pinsq[i], GPIO_DRIVE_CAP_3);
    }
#elif CONFIG_IDF_TARGET_ESP32P4
      // P4: pin numbers stored above; the PARLIO peripheral configures GPIO routing itself.
#endif
  }

  // Corrected = 255 * (Image/255)^(1/2.2).

  /** Sets global brightness (0–255) and recomputes gamma lookup tables. */
  void setBrightness(uint8_t brightness) {
    this->brightness = brightness;

    // Allocate LUTs if not already allocated
    if (!redMap) {
      redMap = (uint8_t*)malloc(256);
      if (!redMap) {
        ESP_LOGE(TAG, "Failed to allocate redMap!");
        initErrorOccurred = true;
        initSuccess = false;
        return;
      }
    }

    if (!greenMap) {
      greenMap = (uint8_t*)malloc(256);
      if (!greenMap) {
        ESP_LOGE(TAG, "Failed to allocate greenMap!");
        initErrorOccurred = true;
        initSuccess = false;
        return;
      }
    }

    if (!blueMap) {
      blueMap = (uint8_t*)malloc(256);
      if (!blueMap) {
        ESP_LOGE(TAG, "Failed to allocate blueMap!");
        initErrorOccurred = true;
        initSuccess = false;
        return;
      }
    }

    if (pW != UINT8_MAX) {
      if (!whiteMap) {
        whiteMap = (uint8_t*)malloc(256);
        if (!whiteMap) {
          ESP_LOGE(TAG, "Failed to allocate whiteMap!");
          initErrorOccurred = true;
          initSuccess = false;
          return;
        }
      }
    } else {
      free(whiteMap);
      whiteMap = nullptr;
    }

    if (pW2 != UINT8_MAX) {
      if (!white2Map) {
        white2Map = (uint8_t*)malloc(256);
        if (!white2Map) {
          ESP_LOGE(TAG, "Failed to allocate white2Map!");
          initErrorOccurred = true;
          initSuccess = false;
          return;
        }
      }
    } else {
      free(white2Map);
      white2Map = nullptr;
    }

    // Fill LUTs with gamma-corrected values
    float tmp;
    for (int i = 0; i < 256; i++) {
      tmp = powf((float)i / 255.0f, 1.0f / gammar);
      redMap[i] = (uint8_t)(tmp * (float)brightness);
      tmp = powf((float)i / 255.0f, 1.0f / gammag);
      greenMap[i] = (uint8_t)(tmp * (float)brightness);
      tmp = powf((float)i / 255.0f, 1.0f / gammab);
      blueMap[i] = (uint8_t)(tmp * (float)brightness);
      if (whiteMap) {
        tmp = powf((float)i / 255.0f, 1.0f / gammaw);
        whiteMap[i] = (uint8_t)(tmp * (float)brightness);
      }
      if (white2Map) {
        tmp = powf((float)i / 255.0f, 1.0f / gammaw2);
        white2Map[i] = (uint8_t)(tmp * (float)brightness);
      }
    }
  }

  /** Sets per-channel gamma correction (applied on top of brightness). RGBW variant; gammaw2 defaults to gammaw1. */
  void setGamma(float gammar, float gammag, float gammab, float gammaw1, float gammaw2 = -1) {
    this->gammar = gammar;
    this->gammag = gammag;
    this->gammab = gammab;
    this->gammaw = gammaw1;
    this->gammaw2 = (gammaw2 < 0) ? gammaw1 : gammaw2;
    setBrightness(brightness);
  }

  /** Sets per-channel gamma correction for RGB strips. */
  void setGamma(float gammar, float gammag, float gammab) {
    this->gammar = gammar;
    this->gammag = gammag;
    this->gammab = gammab;
    setBrightness(brightness);
  }

  /** Apply brightness/gamma LUTs, white extraction, and channel reorder for one pixel.
   *  src[0..channelsPerLight-1] — raw input in (R,G,B[,W[,W2]]) storage order.
   *  dst[0..channelsPerLight-1] — mapped output in wire order (pR/pG/pB/pW/pW2 indices). */
  inline void rgbwBufferMapping(const uint8_t* src, uint8_t* dst) const {
    uint8_t red = src[0];    // raw R from input buffer
    uint8_t green = src[1];  // raw G from input buffer
    uint8_t blue = src[2];   // raw B from input buffer
    if (pW != UINT8_MAX) {
      uint8_t white = src[3];  // raw W (or extract from RGB)
      if (extractWhiteFromRGB && !white) {
        white = MIN(MIN(red, green), blue);  // extract white component
        red -= white;                        // remove white from RGB channels
        green -= white;
        blue -= white;
      }
      dst[pW] = whiteMap[white];                           // apply white LUT to wire order
      if (pW2 != UINT8_MAX) dst[pW2] = white2Map[src[4]];  // apply white2 LUT if present
    }
    dst[pR] = redMap[red];      // apply red LUT to wire order position
    dst[pG] = greenMap[green];  // apply green LUT to wire order position
    dst[pB] = blueMap[blue];    // apply blue LUT to wire order position
  }

  void hwInit() {
#ifdef CONFIG_IDF_TARGET_ESP32
    int interruptSource;
    if (I2S_DEVICE == 0) {
      i2s = &I2S0;
      periph_module_enable(PERIPH_I2S0_MODULE);
      interruptSource = ETS_I2S0_INTR_SOURCE;
      i2sBasePinIndex = I2S0O_DATA_OUT0_IDX;
    } else {
      i2s = &I2S1;
      periph_module_enable(PERIPH_I2S1_MODULE);
      interruptSource = ETS_I2S1_INTR_SOURCE;
      i2sBasePinIndex = I2S1O_DATA_OUT0_IDX;
    }

    i2sReset();
    i2sResetDma();
    i2sResetFifo();
    i2s->conf.tx_right_first = 0;

    // -- Set parallel mode
    i2s->conf2.val = 0;
    i2s->conf2.lcd_en = 1;
    i2s->conf2.lcd_tx_wrx2_en = 1;  // 0 for 16 or 32 parallel output
    i2s->conf2.lcd_tx_sdx2_en = 0;  // HN

    // -- Set up the clock rate and sampling
    i2s->sample_rate_conf.val = 0;
    i2s->sample_rate_conf.tx_bits_mod = 16;  // Number of parallel bits/pins
    i2s->clkm_conf.val = 0;

    i2s->clkm_conf.clka_en = 0;

    // add the capability of going a bit faster
    i2s->clkm_conf.clkm_div_a = 3;     // CLOCK_DIVIDER_A;
    i2s->clkm_conf.clkm_div_b = 1;     // CLOCK_DIVIDER_B;
    i2s->clkm_conf.clkm_div_num = 33;  // CLOCK_DIVIDER_N;

    i2s->fifo_conf.val = 0;
    i2s->fifo_conf.tx_fifo_mod_force_en = 1;
    i2s->fifo_conf.tx_fifo_mod = 1;   // 16-bit single channel data
    i2s->fifo_conf.tx_data_num = 32;  // 32; // fifo length
    i2s->fifo_conf.dscr_en = 1;       // fifo will use dma
    i2s->sample_rate_conf.tx_bck_div_num = 1;
    i2s->conf1.val = 0;
    i2s->conf1.tx_stop_en = 0;
    i2s->conf1.tx_pcm_bypass = 1;

    i2s->conf_chan.val = 0;
    i2s->conf_chan.tx_chan_mod = 1;  // Mono mode, with tx_msb_right = 1, everything goes to right-channel

    i2s->timing.val = 0;
    i2s->int_ena.val = 0;
    /*
    // -- Allocate i2s interrupt
    SET_PERI_REG_BITS(I2S_INT_ENA_REG(I2S_DEVICE), I2S_OUT_EOF_INT_ENA_V,1, I2S_OUT_EOF_INT_ENA_S);
    SET_PERI_REG_BITS(I2S_INT_ENA_REG(I2S_DEVICE), I2S_OUT_TOTAL_EOF_INT_ENA_V, 1, I2S_OUT_TOTAL_EOF_INT_ENA_S);
    SET_PERI_REG_BITS(I2S_INT_ENA_REG(I2S_DEVICE), I2S_OUT_TOTAL_EOF_INT_ENA_V, 1, I2S_OUT_TOTAL_EOF_INT_ENA_S);
    */
    // Free any previously registered interrupt before registering a new one.
    // hwInit() is called once from initLedImpl(); if deleteDriver()+initled() is used,
    // this guard frees the old handle so esp_intr_alloc() does not leak it.
    if (intrHandle != nullptr) {
      esp_intr_free(intrHandle);
      intrHandle = nullptr;
    }
    esp_err_t e = esp_intr_alloc(interruptSource, ESP_INTR_FLAG_INTRDISABLED | ESP_INTR_FLAG_LEVEL3, &interruptHandler, this, &intrHandle);  // 🌙 | ESP_INTR_FLAG_IRAM removed to avoid Cache Disabled but Cached Memory Region Accessed
    if (e != ESP_OK) {
      ESP_LOGE(TAG, "hwInit: esp_intr_alloc failed: %s", esp_err_to_name(e));
      intrHandle = nullptr;
      initErrorOccurred = true;
      return;
    }
#elif CONFIG_IDF_TARGET_ESP32S3
    periph_module_enable(PERIPH_LCD_CAM_MODULE);
    periph_module_reset(PERIPH_LCD_CAM_MODULE);

    // Reset LCD bus
    LCD_CAM.lcd_user.lcd_reset = 1;
    esp_rom_delay_us(100);

    LCD_CAM.lcd_clock.clk_en = 1;              // Enable peripheral clock
    LCD_CAM.lcd_clock.lcd_clk_sel = 2;         // XTAL_CLK source
    LCD_CAM.lcd_clock.lcd_ck_out_edge = 0;     // PCLK low in 1st half cycle
    LCD_CAM.lcd_clock.lcd_ck_idle_edge = 0;    // PCLK low idle
    LCD_CAM.lcd_clock.lcd_clk_equ_sysclk = 0;  // PCLK = CLK / (CLKCNT_N+1)

    // original settings
    // LCD_CAM.lcd_clock.lcd_clkm_div_num = 50;   //_clockspeed.divNum; // 1st stage 1:250 divide
    // LCD_CAM.lcd_clock.lcd_clkm_div_a = 1;      //_clockspeed.divA;     // 0/1 fractional divide
    // LCD_CAM.lcd_clock.lcd_clkm_div_b = 0;      // Ò_clockspeed.divB;

    LCD_CAM.lcd_clock.lcd_clkm_div_num = 50;  //_clockspeed.divNum; // 1st stage 1:250 divide
    LCD_CAM.lcd_clock.lcd_clkm_div_a = 1;     //_clockspeed.divA;     // 0/1 fractional divide
    LCD_CAM.lcd_clock.lcd_clkm_div_b = 0;     // Ò_clockspeed.divB;
    LCD_CAM.lcd_clock.lcd_clkcnt_n = 1;       //

    LCD_CAM.lcd_ctrl.lcd_rgb_mode_en = 0;     // i8080 mode (not RGB)
    LCD_CAM.lcd_rgb_yuv.lcd_conv_bypass = 0;  // Disable RGB/YUV converter
    LCD_CAM.lcd_misc.lcd_next_frame_en = 0;   // Do NOT auto-frame
    LCD_CAM.lcd_data_dout_mode.val = 0;       // No data delays
    LCD_CAM.lcd_user.lcd_always_out_en = 1;   // Enable 'always out' mode
    LCD_CAM.lcd_user.lcd_8bits_order = 0;     // Do not swap bytes
    LCD_CAM.lcd_user.lcd_bit_order = 0;       // Do not reverse bit order
    LCD_CAM.lcd_user.lcd_byte_order = 0;
    LCD_CAM.lcd_user.lcd_2byte_en = 1;        // 8-bit data mode
    LCD_CAM.lcd_user.lcd_dummy = 0;           // Dummy phase(s) @ LCD start
    LCD_CAM.lcd_user.lcd_dummy_cyclelen = 0;  // 1 dummy phase
    LCD_CAM.lcd_user.lcd_cmd = 0;             // No command at LCD start
    LCD_CAM.lcd_misc.lcd_bk_en = 1;
  // -- Create a semaphore to block execution until all the controllers are done

  // IDF5.5: 🌙 hwInit: .isr_cache_safe=true results in Cache disabled but cached memory region accessed crash
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    gdma_channel_alloc_config_t dmaChanConfig = {.sibling_chan = NULL, .direction = GDMA_CHANNEL_DIRECTION_TX, .flags = {.reserve_sibling = 0}};
    // .isr_cache_safe= true}};
    esp_err_t err = gdma_new_ahb_channel(&dmaChanConfig, &dmaChan);  // note: s3 uses this, P4 uses gdma_new_axi_channel
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "hwInit: gdma_new_ahb_channel failed: %s", esp_err_to_name(err));
      initErrorOccurred = true;
      return;
    }
  #else
    gdma_channel_alloc_config_t dmaChanConfig = {.sibling_chan = NULL, .direction = GDMA_CHANNEL_DIRECTION_TX, .flags = {.reserve_sibling = 0, .isr_cache_safe = true}};
    esp_err_t err = gdma_new_channel(&dmaChanConfig, &dmaChan);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "hwInit: gdma_new_channel failed: %s", esp_err_to_name(err));
      initErrorOccurred = true;
      return;
    }
  #endif

    err = gdma_connect(dmaChan, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0));
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "hwInit: gdma_connect failed: %s", esp_err_to_name(err));
      gdma_del_channel(dmaChan);
      dmaChan = nullptr;
      initErrorOccurred = true;
      return;
    }

    gdma_strategy_config_t strategyConfig = {.owner_check = false, .auto_update_desc = false};
    err = gdma_apply_strategy(dmaChan, &strategyConfig);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "hwInit: gdma_apply_strategy failed: %s", esp_err_to_name(err));
      gdma_del_channel(dmaChan);
      dmaChan = nullptr;
      initErrorOccurred = true;
      return;
    }

    /*
    gdma_transfer_ability_t ability = {
        .psram_trans_align = 64,
        //.sram_trans_align = 64,
    };
    gdma_set_transfer_ability(dmaChan, &ability);
*/
    // Enable DMA transfer callback
    gdma_tx_event_callbacks_t txCbs = {.on_trans_eof = interruptHandler, .on_descr_err = NULL};
    err = gdma_register_tx_event_callbacks(dmaChan, &txCbs, this);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "hwInit: gdma_register_tx_event_callbacks failed: %s", esp_err_to_name(err));
      gdma_del_channel(dmaChan);
      dmaChan = nullptr;
      initErrorOccurred = true;
      return;
    }
    // esp_intr_disable((*dmaChan).intr);
    LCD_CAM.lcd_user.lcd_start = 0;
#elif CONFIG_IDF_TARGET_ESP32P4
    // P4: no semaphore setup needed (PARLIO setup deferred to initTransferBuffers())
    return;
#endif
    // -- Create a semaphore to block execution until all the controllers are done

    if (sem == NULL) {
      sem = xSemaphoreCreateBinary();
    }

    if (semSync == NULL) {
      semSync = xSemaphoreCreateBinary();
    }
    if (semDisp == NULL) {
      semDisp = xSemaphoreCreateBinary();
    }
  }

  void initTransferBuffers() {
    /*
    transferBuffers[0] = allocateDMABuffer(channelsPerLight * 8 * 2 * 3); // the buffers for the
    transferBuffers[1] = allocateDMABuffer(channelsPerLight * 8 * 2 * 3);
    transferBuffers[2] = allocateDMABuffer(channelsPerLight * 8 * 2 * 3);
    transferBuffers[3] = allocateDMABuffer(channelsPerLight * 8 * 2 * 3 * 4);

    putdefaultones((uint16_t *)transferBuffers[0]->buffer);
    putdefaultones((uint16_t *)transferBuffers[1]->buffer);
    */

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3
  #ifdef CONFIG_IDF_TARGET_ESP32  // d0-wrover crashes with memory region error if set in PSRAM
    transferBuffers = (I2SClocklessLedDriverDMABuffer**)heap_caps_calloc_prefer(nbDmaBuffer + 2, sizeof(I2SClocklessLedDriverDMABuffer*), 2, MALLOC_CAP_DEFAULT, MALLOC_CAP_DEFAULT);
  #elif CONFIG_IDF_TARGET_ESP32S3
    transferBuffers = (I2SClocklessLedDriverDMABuffer**)heap_caps_calloc_prefer(nbDmaBuffer + 2, sizeof(I2SClocklessLedDriverDMABuffer*), 2, MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT);
  #endif
    if (!transferBuffers) {
      ESP_LOGE(TAG, "Failed to allocate transferBuffers!");
      initErrorOccurred = true;
      return;
    }

    for (int i = 0; i < nbDmaBuffer + 1; i++) {
      transferBuffers[i] = allocateDMABuffer(channelsPerLight * 8 * 2 * 3);
    }
    transferBuffers[nbDmaBuffer + 1] = allocateDMABuffer(channelsPerLight * 8 * 2 * 3 * 4);

    for (int i = 0; i < nbDmaBuffer; i++) {
      putdefaultones((uint16_t*)transferBuffers[i]->buffer);
    }

#elif CONFIG_IDF_TARGET_ESP32P4
    // Allocate ping-pong waveform buffers for PARLIO DMA.
    if (!p4Buffer1) {
      p4Buffer1 = (uint16_t*)heap_caps_calloc_prefer(PARLIO_P4_BUFFER_BYTES, 1, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_CACHE_ALIGNED, MALLOC_CAP_DMA);
      if (!p4Buffer1) {
        ESP_LOGE(TAG, "initTransferBuffers: failed to allocate p4Buffer1 — out of memory");
        initErrorOccurred = true;
        return;
      }
    }
    if (!p4Buffer2) {
      p4Buffer2 = (uint16_t*)heap_caps_calloc_prefer(PARLIO_P4_BUFFER_BYTES, 1, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_CACHE_ALIGNED, MALLOC_CAP_DMA);
      if (!p4Buffer2) {
        ESP_LOGE(TAG, "initTransferBuffers: failed to allocate p4Buffer2 — out of memory");
        heap_caps_free(p4Buffer1);
        p4Buffer1 = nullptr;
        p4BufferActive = nullptr;
        initErrorOccurred = true;
        return;
      }
    }
    p4BufferActive = p4Buffer1;

  // Configure (or reconfigure) the PARLIO TX unit.
  #if !HAS_PARLIO_DRIVER
    ESP_LOGE(TAG, "PARLIO driver not available — ESP-IDF v5.1+ required for ESP32-P4 support");
    initErrorOccurred = true;
    return;
  #else

    {
      uint8_t outputs = numStrips;
      if (outputs > SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH) {
        ESP_LOGE(TAG, "initTransferBuffers: numStrips (%u) exceeds SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH (%u)", outputs, SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH);
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
        ESP_LOGE(TAG, "hwInit: configuration requires %u bytes, but only %u are allocated", (unsigned)required_bytes, (unsigned)PARLIO_P4_BUFFER_BYTES);
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

      if (p4TxUnit != NULL) {
        esp_err_t err;
        if ((err = parlio_tx_unit_wait_all_done(p4TxUnit, portMAX_DELAY)) != ESP_OK) ESP_LOGE(TAG, "initTransferBuffers: parlio_tx_unit_wait_all_done failed: %s", esp_err_to_name(err));
        if ((err = parlio_tx_unit_disable(p4TxUnit)) != ESP_OK) ESP_LOGE(TAG, "initTransferBuffers: parlio_tx_unit_disable failed: %s", esp_err_to_name(err));
        if ((err = parlio_del_tx_unit(p4TxUnit)) != ESP_OK) ESP_LOGE(TAG, "initTransferBuffers: parlio_del_tx_unit failed: %s", esp_err_to_name(err));
        p4TxUnit = NULL;
      }

      esp_err_t err;
      if ((err = parlio_new_tx_unit(&p4Config, &p4TxUnit)) != ESP_OK) {
        ESP_LOGE(TAG, "initTransferBuffers: parlio_new_tx_unit failed: %s", esp_err_to_name(err));
        p4TxUnit = NULL;
        initErrorOccurred = true;
        return;
      }
      if ((err = parlio_tx_unit_enable(p4TxUnit)) != ESP_OK) {
        ESP_LOGE(TAG, "initTransferBuffers: parlio_tx_unit_enable failed: %s", esp_err_to_name(err));
        parlio_del_tx_unit(p4TxUnit);
        p4TxUnit = NULL;
        initErrorOccurred = true;
        return;
      }

      ESP_LOGI(TAG, "PARLIO configured (%u outputs, %u LEDs/output)", (unsigned)outputs, (unsigned)max_leds);
    }
  #endif
    return;  // P4 done; skip S3/ESP32 DMA buffer setup below
#endif

#ifdef FULL_DMA_BUFFER
    /*
     We do create n+2 buffers
     the first buffer is to be sure that everything is 0
     the last one is to put back the I2S at 0 the last bufffer is longer because when using the loop display mode the time between two frames needs to be longh enough.
     */
    dmaBuffersTransposed = (I2SClocklessLedDriverDMABuffer**)malloc(sizeof(I2SClocklessLedDriverDMABuffer*) * (numLedPerStrip + 2));
    for (int i = 0; i < numLedPerStrip + 2; i++) {
      if (i < numLedPerStrip + 1)
        dmaBuffersTransposed[i] = allocateDMABuffer(channelsPerLight * 8 * 2 * 3);
      else
        dmaBuffersTransposed[i] = allocateDMABuffer(channelsPerLight * 8 * 2 * 3 * 4);
      if (i < numLedPerStrip) dmaBuffersTransposed[i]->descriptor.eof = 0;
      if (i) {
        dmaBuffersTransposed[i - 1]->descriptor.qe.stqe_next = &(dmaBuffersTransposed[i]->descriptor);
        if (i < numLedPerStrip + 1) {
          putdefaultones((uint16_t*)dmaBuffersTransposed[i]->buffer);
        }
      }
    }
#endif
  }

#ifdef FULL_DMA_BUFFER

  /** Stops the LOOP display mode started by showPixelsFromBuffer(LOOP). */
  void stopDisplayLoop() { dmaBuffersTransposed[numLedPerStrip + 1]->descriptor.qe.stqe_next = 0; }

  /** Displays the pre-transposed DMA buffer without re-transposing the leds[] array. Non-blocking. */
  void showPixelsFromBuffer() { showPixelsFromBuffer(NO_WAIT); }

  /**
   * Displays the pre-transposed DMA buffer.
   * dispmode == LOOP: the DMA descriptor ring loops forever (CPU-free); call stopDisplayLoop() to stop.
   * dispmode == NO_WAIT / WAIT: single-shot.
   */
  void showPixelsFromBuffer(DisplayMode dispmode) {
    /*
     We cannot launch twice when in loopmode
     */
    if (displayMode == LOOP && isDisplaying) {
      ESP_LOGE(TAG, "The loop mode is activated execute stopDisplayLoop() first");
      return;
    }
    /*
     We wait for the display to be stopped before launching a new one
     */

    displayMode = dispmode;
    isWaiting = false;
    if (dispmode == LOOP or dispmode == LOOP_INTERUPT) {
      dmaBuffersTransposed[numLedPerStrip + 1]->descriptor.qe.stqe_next = &(dmaBuffersTransposed[0]->descriptor);
    }
    transpose = false;
    // wasWaitingtofinish = true;
    //  Serial.printf(" was:%d\n",wasWaitingtofinish);
    hwStart();

    if (dispmode == WAIT) {
      isWaiting = true;
      if (sem == NULL) sem = xSemaphoreCreateBinary();
      if (xSemaphoreTake(sem, pdMS_TO_TICKS(500)) == pdFALSE) {
        ESP_LOGW(TAG, "sem wait too long");
        xSemaphoreTake(sem, portMAX_DELAY);
      }
    }
  }

  void showPixelsFirstTranspose(OffsetDisplay offdisp) {
    offsetDisplay = offdisp;
    showPixelsFirstTranspose();
    offsetDisplay = defaultOffsetDisplay;
  }

  void showPixelsFirstTranspose(OffsetDisplay offdisp, uint8_t* temp_leds) {
    offsetDisplay = offdisp;
    showPixelsFirstTranspose(temp_leds);
    offsetDisplay = defaultOffsetDisplay;
  }

  void showPixelsFirstTranspose(uint8_t* newLeds) {
    uint8_t* tmp_leds;
    //  Serial.println("on entre");
    if (isDisplaying == true && displayMode == NO_WAIT) {
      // Serial.println("we are here in trs");
      wasWaitingtofinish = true;
      tmp_leds = newLeds;
      if (waitDisp == NULL) waitDisp = xSemaphoreCreateCounting(10, 0);
      if (xSemaphoreTake(waitDisp, pdMS_TO_TICKS(500)) == pdFALSE) {
        ESP_LOGW(TAG, "waitDisp wait too long");
        xSemaphoreTake(waitDisp, portMAX_DELAY);
      }
    }
    leds = newLeds;
    showPixelsFirstTranspose();
    // leds = tmp_leds;
  }

  void showPixelsFirstTranspose() { showPixelsFirstTranspose(NO_WAIT); }
  void showPixelsFirstTranspose(DisplayMode dispmode) {
    // Serial.println("on entrre");
    transpose = false;
    if (leds == NULL) {
      ESP_LOGE(TAG, "no led");
      return;
    }
    if (isDisplaying == true && dispmode == NO_WAIT) {
      // Serial.println("we are here");
      wasWaitingtofinish = true;
      if (waitDisp == NULL) waitDisp = xSemaphoreCreateCounting(10, 0);
      if (xSemaphoreTake(waitDisp, pdMS_TO_TICKS(500)) == pdFALSE) {
        ESP_LOGW(TAG, "waitDisp wait too long");
        xSemaphoreTake(waitDisp, portMAX_DELAY);
      }
    }
    // Serial.println("on dsiup");
    // dmaBufferActive=0;
    transposeAll();
    // Serial.println("end transpose");
    showPixelsFromBuffer(dispmode);
  }

  void transposeAll() {
    ledToDisplay = 0;
    /*Lines secondPixel[channelsPerLight];
    for (int j = 0; j < numLedPerStrip; j++)
    {
        uint8_t *poli = leds + ledToDisplay * channelsPerLight;
        for (int i = 0; i < numStrips; i++)
        {
            uint8_t red = *(poli + 0);
            uint8_t green = *(poli + 1);
            uint8_t blue = *(poli + 2);
            // 🌙 extract White from RGB
            if (driver->pW != UINT8_MAX) {
              uint8_t white = *(poli + 3);
              // if white is filled, use that and do not extract rgbw
              if (driver->extractWhiteFromRGB && !white) {
                white = MIN(MIN(red, green), blue);
                red -= white;
                green -= white;
                blue -= white;
              }
              secondPixel[driver->pW].bytes[i] = driver->whiteMap[white];
              if (driver->pW2 != UINT8_MAX) {
                secondPixel[driver->pW2].bytes[i] = driver->white2Map[*(poli + 4)];
              }
            }
            secondPixel[pR].bytes[i] = redMap[red];
            secondPixel[pG].bytes[i] = greenMap[green];
            secondPixel[pB].bytes[i] = blueMap[blue];
            //#endif
            poli += numLedPerStrip * channelsPerLight;
        }
        ledToDisplay++;
        transpose16x1Noinline2(secondPixel[0].bytes, (uint16_t *)dmaBuffersTransposed[j + 1]->buffer);
        transpose16x1Noinline2(secondPixel[1].bytes, (uint16_t *)dmaBuffersTransposed[j + 1]->buffer + 3 * 8);
        transpose16x1Noinline2(secondPixel[2].bytes, (uint16_t *)dmaBuffersTransposed[j + 1]->buffer + 2 * 3 * 8);
        if (pW != UINT8_MAX)
            transpose16x1Noinline2(secondPixel[3].bytes, (uint16_t *)dmaBuffersTransposed[j + 1]->buffer + 3 * 3 * 8);
    }*/
    for (int j = 0; j < numLedPerStrip; j++) {
      ledToDisplay = j;
      dmaBufferActive = j + 1;
      loadAndTranspose(this);
      /*
      #ifdef ENABLE_HARDWARE_SCROLL
      loadAndTranspose(leds, numLedPerStrip, numStrips, offsetDisplay, (uint16_t *)dmaBuffersTransposed[j + 1]->buffer, j, redMap, greenMap, blueMap, whiteMap, channelsPerLight,
      pR, pG, pB); #else loadAndTranspose(leds, stripSize, numStrips, (uint16_t *)dmaBuffersTransposed[j+1]->buffer, j, redMap, greenMap, blueMap, whiteMap, channelsPerLight, pR, pG, pB);
      #endif
      */
    }
  }

  void setPixelinBufferByStrip(int stripNumber, int posOnStrip, uint8_t red, uint8_t green, uint8_t blue) {
    if (!initSuccess) {
      // Silent return in hot path
      return;
    }
    uint8_t white = 0;
    if (pW != UINT8_MAX) {
      white = MIN(red, green);
      white = MIN(W, blue);
      red = red - white;
      green = green - white;
      blue = blue - white;
    }
    setPixelinBufferByStrip(stripNumber, posOnStrip, red, green, blue, white);
  }

  void setPixelinBufferByStrip(int stripNumber, int posOnStrip, uint8_t red, uint8_t green, uint8_t blue, uint8_t white, uint8_t white2 = 0) {
    if (!initSuccess) {
      // Silent return in hot path
      return;
    }
    uint16_t mask = ~(1 << stripNumber);
    uint8_t colors[3];
    colors[pR] = redMap[red];
    colors[pG] = greenMap[green];
    colors[pB] = blueMap[blue];
    uint16_t* B = (uint16_t*)dmaBuffersTransposed[posOnStrip + 1]->buffer;
    // printf("channelsPerLight:%d\n",channelsPerLight);
    uint8_t y = colors[0];
    *((uint16_t*)(B)) = (*((uint16_t*)(B)) & mask) | ((uint16_t)((y & 128) >> 7) << stripNumber);
    *((uint16_t*)(B + 5)) = (*((uint16_t*)(B + 5)) & mask) | ((uint16_t)((y & 64) >> 6) << stripNumber);
    *((uint16_t*)(B + 6)) = (*((uint16_t*)(B + 6)) & mask) | ((uint16_t)((y & 32) >> 5) << stripNumber);
    *((uint16_t*)(B + 11)) = (*((uint16_t*)(B + 11)) & mask) | ((uint16_t)((y & 16) >> 4) << stripNumber);
    *((uint16_t*)(B + 12)) = (*((uint16_t*)(B + 12)) & mask) | ((uint16_t)((y & 8) >> 3) << stripNumber);
    *((uint16_t*)(B + 17)) = (*((uint16_t*)(B + 17)) & mask) | ((uint16_t)((y & 4) >> 2) << stripNumber);
    *((uint16_t*)(B + 18)) = (*((uint16_t*)(B + 18)) & mask) | ((uint16_t)((y & 2) >> 1) << stripNumber);
    *((uint16_t*)(B + 23)) = (*((uint16_t*)(B + 23)) & mask) | ((uint16_t)(y & 1) << stripNumber);

    B += 3 * 8;
    y = colors[1];
    *((uint16_t*)(B)) = (*((uint16_t*)(B)) & mask) | ((uint16_t)((y & 128) >> 7) << stripNumber);
    *((uint16_t*)(B + 5)) = (*((uint16_t*)(B + 5)) & mask) | ((uint16_t)((y & 64) >> 6) << stripNumber);
    *((uint16_t*)(B + 6)) = (*((uint16_t*)(B + 6)) & mask) | ((uint16_t)((y & 32) >> 5) << stripNumber);
    *((uint16_t*)(B + 11)) = (*((uint16_t*)(B + 11)) & mask) | ((uint16_t)((y & 16) >> 4) << stripNumber);
    *((uint16_t*)(B + 12)) = (*((uint16_t*)(B + 12)) & mask) | ((uint16_t)((y & 8) >> 3) << stripNumber);
    *((uint16_t*)(B + 17)) = (*((uint16_t*)(B + 17)) & mask) | ((uint16_t)((y & 4) >> 2) << stripNumber);
    *((uint16_t*)(B + 18)) = (*((uint16_t*)(B + 18)) & mask) | ((uint16_t)((y & 2) >> 1) << stripNumber);
    *((uint16_t*)(B + 23)) = (*((uint16_t*)(B + 23)) & mask) | ((uint16_t)(y & 1) << stripNumber);

    B += 3 * 8;
    y = colors[2];
    *((uint16_t*)(B)) = (*((uint16_t*)(B)) & mask) | ((uint16_t)((y & 128) >> 7) << stripNumber);
    *((uint16_t*)(B + 5)) = (*((uint16_t*)(B + 5)) & mask) | ((uint16_t)((y & 64) >> 6) << stripNumber);
    *((uint16_t*)(B + 6)) = (*((uint16_t*)(B + 6)) & mask) | ((uint16_t)((y & 32) >> 5) << stripNumber);
    *((uint16_t*)(B + 11)) = (*((uint16_t*)(B + 11)) & mask) | ((uint16_t)((y & 16) >> 4) << stripNumber);
    *((uint16_t*)(B + 12)) = (*((uint16_t*)(B + 12)) & mask) | ((uint16_t)((y & 8) >> 3) << stripNumber);
    *((uint16_t*)(B + 17)) = (*((uint16_t*)(B + 17)) & mask) | ((uint16_t)((y & 4) >> 2) << stripNumber);
    *((uint16_t*)(B + 18)) = (*((uint16_t*)(B + 18)) & mask) | ((uint16_t)((y & 2) >> 1) << stripNumber);
    *((uint16_t*)(B + 23)) = (*((uint16_t*)(B + 23)) & mask) | ((uint16_t)(y & 1) << stripNumber);
    if (pW != UINT8_MAX) {
      B += 3 * 8;
      y = whiteMap[white];
      *((uint16_t*)(B)) = (*((uint16_t*)(B)) & mask) | ((uint16_t)((y & 128) >> 7) << stripNumber);
      *((uint16_t*)(B + 5)) = (*((uint16_t*)(B + 5)) & mask) | ((uint16_t)((y & 64) >> 6) << stripNumber);
      *((uint16_t*)(B + 6)) = (*((uint16_t*)(B + 6)) & mask) | ((uint16_t)((y & 32) >> 5) << stripNumber);
      *((uint16_t*)(B + 11)) = (*((uint16_t*)(B + 11)) & mask) | ((uint16_t)((y & 16) >> 4) << stripNumber);
      *((uint16_t*)(B + 12)) = (*((uint16_t*)(B + 12)) & mask) | ((uint16_t)((y & 8) >> 3) << stripNumber);
      *((uint16_t*)(B + 17)) = (*((uint16_t*)(B + 17)) & mask) | ((uint16_t)((y & 4) >> 2) << stripNumber);
      *((uint16_t*)(B + 18)) = (*((uint16_t*)(B + 18)) & mask) | ((uint16_t)((y & 2) >> 1) << stripNumber);
      *((uint16_t*)(B + 23)) = (*((uint16_t*)(B + 23)) & mask) | ((uint16_t)(y & 1) << stripNumber);
    }
    if (pW2 != UINT8_MAX) {
      B += 3 * 8;
      y = white2Map[white2];
      *((uint16_t*)(B)) = (*((uint16_t*)(B)) & mask) | ((uint16_t)((y & 128) >> 7) << stripNumber);
      *((uint16_t*)(B + 5)) = (*((uint16_t*)(B + 5)) & mask) | ((uint16_t)((y & 64) >> 6) << stripNumber);
      *((uint16_t*)(B + 6)) = (*((uint16_t*)(B + 6)) & mask) | ((uint16_t)((y & 32) >> 5) << stripNumber);
      *((uint16_t*)(B + 11)) = (*((uint16_t*)(B + 11)) & mask) | ((uint16_t)((y & 16) >> 4) << stripNumber);
      *((uint16_t*)(B + 12)) = (*((uint16_t*)(B + 12)) & mask) | ((uint16_t)((y & 8) >> 3) << stripNumber);
      *((uint16_t*)(B + 17)) = (*((uint16_t*)(B + 17)) & mask) | ((uint16_t)((y & 4) >> 2) << stripNumber);
      *((uint16_t*)(B + 18)) = (*((uint16_t*)(B + 18)) & mask) | ((uint16_t)((y & 2) >> 1) << stripNumber);
      *((uint16_t*)(B + 23)) = (*((uint16_t*)(B + 23)) & mask) | ((uint16_t)(y & 1) << stripNumber);
    }
  }

  /** Writes one RGBW pixel directly into the pre-transposed DMA buffer (FULL_DMA_BUFFER only). */
  void setPixelinBuffer(uint32_t pos, uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
    int stripNumber = -1;
    uint32_t total = 0;
    uint32_t posOnStrip = pos;
    if (pos > totalLeds - 1) {
      printf("Position out of bound %lu > %lu\n", pos, totalLeds - 1);
      return;
    }
    while (total <= pos) {
      stripNumber++;
      total += stripSize[stripNumber];
    }
    if (stripNumber > 0) {
      posOnStrip = -total + pos + stripSize[stripNumber];
    } else {
      posOnStrip = pos;
    }

    setPixelinBufferByStrip(stripNumber, posOnStrip, red, green, blue, white);
  }

  /** Writes one RGB pixel directly into the pre-transposed DMA buffer; derives white channel for RGBW strips. */
  void setPixelinBuffer(uint32_t pos, uint8_t red, uint8_t green, uint8_t blue) {
    uint8_t white = 0;
    if (pW != UINT8_MAX) {
      white = MIN(red, green);
      white = MIN(W, blue);
      red = red - white;
      green = green - white;
      blue = blue - white;
    }

    setPixelinBuffer(pos, red, green, blue, W);
  }

  /** Initialises the driver without an external LED buffer (buffer managed externally or unused). */
  void initled(uint8_t* pinsq, uint8_t numStrips, uint16_t numLedPerStrip, ColorArrangement cArr = ORDER_GRB) { initled(nullptr, pinsq, numStrips, numLedPerStrip, cArr); }

  /**
   * Blocks until the next DMA frame boundary (FULL_DMA_BUFFER + LOOP mode only).
   * Use before writing to the DMA buffer to avoid tearing.
   */
  void waitSync() {
    semSync = xSemaphoreCreateBinary();
    if (xSemaphoreTake(semSync, pdMS_TO_TICKS(500)) == pdFALSE) {
      ESP_LOGW(TAG, "semSync wait too long");
      xSemaphoreTake(semSync, portMAX_DELAY);
    }
  }
#endif
  /** Sets one RGBW pixel in the leds[] byte buffer. */
  void setPixel(uint32_t pos, uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
    uint8_t* offset = leds + (pos << 2);  // faster than doing * 4
    *(offset) = red;
    *(++offset) = green;
    *(++offset) = blue;
    *(++offset) = white;
  }

  /** Sets one RGB pixel in the leds[] buffer; auto-derives white channel for RGBW strips. */
  void setPixel(uint32_t pos, uint8_t red, uint8_t green, uint8_t blue) {
    if (pW == UINT8_MAX) {  // no white channel
      uint8_t* offset = leds + (pos << 1) + pos;
      *(offset) = red;
      *(++offset) = green;
      *(++offset) = blue;
    } else {
      /*
          Code to transform RBG into RGBW thanks to @Jonathanese https://github.com/Jonathanese/NodeMCUPoleDriver/blob/master/LED_Framework.cpp
      */
      uint8_t white = MIN(red, green);
      white = MIN(white, blue);
      red = red - white;
      green = green - white;
      blue = blue - white;
      setPixel(pos, red, green, blue, white);
    }
  }

  OffsetDisplay getDefaultOffset() { return defaultOffsetDisplay; }

  void waitDisplay() {
    if (isDisplaying == true) {
      wasWaitingtofinish = true;
      ESP_LOGD(TAG, "already displaying... wait");
      if (waitDisp == NULL) {
        waitDisp = xSemaphoreCreateCounting(10, 0);
      }
      const TickType_t xDelay = showDelay;
      xSemaphoreTake(waitDisp, xDelay);
    }
    isDisplaying = true;
  }

  // ── Display ─────────────────────────────────────────────────────────────────
  // All showPixels() variants call waitDisplay() internally, so a pending
  // NO_WAIT transfer is always drained before the next one starts.

  /** Pushes the current frame; optional DisplayMode (WAIT/NO_WAIT), buffer swap, and hardware scroll offset. */
  void showPixels(DisplayMode dispmode, uint8_t* newLeds, OffsetDisplay offdisp) {
    waitDisplay();
    offsetDisplay = offdisp;
    leds = newLeds;
    displayMode = dispmode;
    showPixelsImpl();
  }
  void showPixels(uint8_t* newLeds, OffsetDisplay offdisp) {
    waitDisplay();
    offsetDisplay = offdisp;
    leds = newLeds;
    displayMode = WAIT;
    showPixelsImpl();
    // offsetDisplay = defaultOffsetDisplay;
  }

  void showPixels(OffsetDisplay offdisp) {
    waitDisplay();
    offsetDisplay = offdisp;
    leds = saveleds;

    displayMode = WAIT;
    showPixelsImpl();
    // offsetDisplay = defaultOffsetDisplay;
  }

  void showPixels(uint8_t* newleds) {
    waitDisplay();
    leds = newleds;
    displayMode = WAIT;
    offsetDisplay = defaultOffsetDisplay;
    showPixelsImpl();
  }

  void showPixels() {
    if (!enableDriver) return;
    waitDisplay();
    leds = saveleds;
    offsetDisplay = defaultOffsetDisplay;
    displayMode = WAIT;
    showPixelsImpl();
  }

  void showPixels(DisplayMode dispmode, uint8_t* newleds) {
    waitDisplay();
    offsetDisplay = defaultOffsetDisplay;
    leds = newleds;
    displayMode = dispmode;
    showPixelsImpl();
    // leds = tmp_leds;
  }

  void showPixels(DisplayMode dispmode) {
    waitDisplay();
    leds = saveleds;
    offsetDisplay = defaultOffsetDisplay;
    displayMode = dispmode;
    showPixelsImpl();
  }

  /** Convenience alias for showPixels() — blocking frame push on all platforms. */
  void show() { showPixels(); }

  void showPixelsImpl() {
    if (!enableDriver) {
      isDisplaying = false;
      if (waitDisp != NULL) xSemaphoreGive(waitDisp);
      return;
    }

    if (!initSuccess) {
      isDisplaying = false;
      if (waitDisp != NULL) xSemaphoreGive(waitDisp);
      return;
    }

    if (leds == NULL) {
      ESP_LOGE(TAG, "no leds buffer defined");
      isDisplaying = false;
      if (waitDisp != NULL) xSemaphoreGive(waitDisp);
      return;
    }

#ifdef __HARDWARE_MAP
    hmapOff = hmap;

#endif
#ifdef __HARDWARE_MAP_HARDWARE
    hmapOff = 0;

#endif

    ledToDisplay = 0;
    transpose = true;
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3
  #ifdef CONFIG_IDF_TARGET_ESP32
    for (int buffNum = 0; buffNum < nbDmaBuffer - 1; buffNum++) {
      transferBuffers[buffNum]->descriptor.qe.stqe_next = &(transferBuffers[buffNum + 1]->descriptor);
    }

    transferBuffers[nbDmaBuffer - 1]->descriptor.qe.stqe_next = &(transferBuffers[0]->descriptor);
    transferBuffers[nbDmaBuffer]->descriptor.qe.stqe_next = &(transferBuffers[0]->descriptor);
    transferBuffers[nbDmaBuffer + 1]->descriptor.qe.stqe_next = 0;

  #elif CONFIG_IDF_TARGET_ESP32S3
    for (int buffNum = 0; buffNum < nbDmaBuffer - 1; buffNum++) {
      transferBuffers[buffNum]->next = transferBuffers[buffNum + 1];
    }
    transferBuffers[nbDmaBuffer - 1]->next = transferBuffers[0];
    transferBuffers[nbDmaBuffer]->next = transferBuffers[0];
    transferBuffers[nbDmaBuffer + 1]->next = transferBuffers[nbDmaBuffer + 1];
  #endif

    ledToDisplay = 0;
    dmaBufferActive = 0;
    for (int numBuff = 0; numBuff < nbDmaBuffer - 1; numBuff++) {
      loadAndTranspose(this);
      dmaBufferActive = dmaBufferActive + 1;
      ledToDisplay = ledToDisplay + 1;
    }
    ledToDisplay = ledToDisplay - 1;
    dmaBufferActive = nbDmaBuffer - 1;
    ledToDisplayOut = 0;
    isDisplaying = true;
    hwStart();

    if (displayMode == WAIT) {
      isWaiting = true;
      if (sem == NULL) sem = xSemaphoreCreateBinary();
      if (xSemaphoreTake(sem, pdMS_TO_TICKS(500)) == pdFALSE) {
        ESP_LOGW(TAG, "sem wait too long");
        xSemaphoreTake(sem, portMAX_DELAY);
      }
    } else {
      isWaiting = false;
      isDisplaying = true;
    }
#elif CONFIG_IDF_TARGET_ESP32P4
    if (loadAndTranspose()) {
      hwStart();
      hwStop();
    }
    // If loadAndTranspose fails (buffer too small) the frame is skipped silently;
    // the error was already logged inside loadAndTranspose().
    isDisplaying = false;
#endif
  }

#ifdef USE_PIXELSLIB
  Pixel* strip(int stripNum) {
    Pixel* l = reinterpret_cast<Pixel*>(leds);
    // Serial.printf(" strip %d\n",stripNum);

    for (int i = 0; i < (stripNum % numStrips); i++) {
      // Serial.printf("     strip %d\n",stripSize[i]);
      l = l + stripSize[i];
    }
    return l;
  }
#endif

  uint16_t maxLength(uint16_t* sizes, uint8_t numStrips) {
    uint16_t max = 0;
    for (int i = 0; i < numStrips; i++) {
      if (max < sizes[i]) {
        max = sizes[i];
      }
    }
    return max;
  }

#ifdef USE_PIXELSLIB
  void initled(Pixels pix, uint8_t* pinsq) { initled((uint8_t*)pix.getPixels(), pinsq, pix.getLengths(), pix.getNumStrip()); }
#endif
  bool s3patch_inclhwInit = true;
  /**
   * CANONICAL initled — the primary entry point.  All other initled() overloads
   * are convenience wrappers that translate their arguments and call this one.
   *
   * @param leds              Pointer to the LED byte buffer (channelsPerLight bytes per pixel,
   *                          strips laid out sequentially). May be nullptr if the buffer
   *                          will be set later.
   * @param pinsq             GPIO pin numbers, one per strip (array length: numStrips).
   * @param sizes             LED count per strip (array length: numStrips).
   * @param numStrips         Number of parallel strips (max MAX_PINS).
   * @param channelsPerLight  Bytes per pixel: 3 = RGB, 4 = RGBW, 5 = RGBCCT.
   * @param pR                Wire-order byte offset of the Red channel.
   * @param pG                Wire-order byte offset of the Green channel.
   * @param pB                Wire-order byte offset of the Blue channel.
   * @param pW                Wire-order byte offset of the White channel (UINT8_MAX = absent).
   * @param pW2               Wire-order byte offset of the warm White channel (UINT8_MAX = absent).
   * @param extractWhiteFromRGB  Derive white from the minimum of R/G/B and subtract.
   */
  void initled(uint8_t* leds, uint8_t* pinsq, uint16_t* sizes, uint8_t numStrips, uint8_t channelsPerLight, uint8_t pR, uint8_t pG, uint8_t pB, uint8_t pW = UINT8_MAX, uint8_t pW2 = UINT8_MAX, bool extractWhiteFromRGB = false) {
    if (pinsq == nullptr || sizes == nullptr || numStrips == 0 || numStrips > MAX_PINS) {
      ESP_LOGE(TAG, "initled: invalid args numStrips=%u sizes=%p pinsq=%p", numStrips, (void*)sizes, (void*)pinsq);
      return;
    }
    totalLeds = 0;
    for (int i = 0; i < numStrips; i++) {
      this->stripSize[i] = sizes[i];
      totalLeds += sizes[i];
    }
    uint16_t maximum = maxLength(sizes, numStrips);
    // Serial.printf("maximum %d\n",maximum);
    ESP_LOGD(TAG, "maximum leds %d", maximum);
    this->channelsPerLight = channelsPerLight;
    this->pR = pR;
    this->pG = pG;
    this->pB = pB;
    this->pW = pW;
    this->pW2 = pW2;
    this->extractWhiteFromRGB = extractWhiteFromRGB;
    initLedImpl(leds, pinsq, numStrips, maximum);
  }

  // ── Initialisation ──────────────────────────────────────────────────────────

  /**
   * Initialises the driver with variable-length strips.
   * @param leds      Pointer to the LED byte buffer (3 or 4 bytes per pixel). May be nullptr.
   * @param pinsq     Array of GPIO pin numbers, one per strip.
   * @param sizes     Array of strip lengths (one entry per strip).
   * @param numStrips Number of parallel strips (max MAX_PINS).
   * @param cArr      Colour byte order; defaults to ORDER_GRB.
   */
  /** Convenience overload — variable strip lengths, colour order via ColorArrangement enum. */
  void initled(uint8_t* leds, uint8_t* pinsq, uint16_t* sizes, uint8_t numStrips, ColorArrangement cArr = ORDER_GRB) {
    uint8_t channelsPerLight, r, g, b, w, w2;
    applyColorArrangement(cArr, channelsPerLight, r, g, b, w, w2);
    initled(leds, pinsq, sizes, numStrips, channelsPerLight, r, g, b, w, w2);
  }

  /** Initialises the driver with uniform strip lengths; cArr defaults to ORDER_GRB. */
  void initled(uint8_t* leds, uint8_t* pinsq, uint8_t numStrips, uint16_t numLedPerStrip, ColorArrangement cArr = ORDER_GRB) {
    for (int i = 0; i < numStrips; i++) {
      this->stripSize[i] = numLedPerStrip;
    }
    initled(leds, pinsq, this->stripSize, numStrips, cArr);
  }

  /*
   *
   *
   *
   *
   */

  void createhardwareMap() {
#ifdef __HARDWARE_MAP
    if (mapLed == NULL) {
      printf("no mapapig\r\n");
      return;
    }
    ESP_LOGE(TAG, "trying to map2");
    uint32_t offset2 = 0;
    for (uint32_t leddisp = 0; leddisp < numLedPerStrip; leddisp++) {
      uint32_t offset = 0;
      for (int i = 0; i < numStrips; i++) {
        if (leddisp < stripSize[i]) {
          // ESP_LOGE(TAG,"%d :%d",leddisp+offset,mapLed(leddisp+offset));
          hmap[offset2] = mapLed(leddisp + offset) * channelsPerLight;
          offset += stripSize[i];
          offset2++;
        }
      }
    }
#endif
  }

  void setShowDelay() { showDelay = (((numLedPerStrip * 125 * 8 * channelsPerLight) / 100000) + 1); }

  void initLedImpl(uint8_t* leds, uint8_t* pinsq, uint8_t numStrips, uint16_t numLedPerStrip) {
    // Reset error state so retry after a failed initled() works correctly.
    initErrorOccurred = false;
    initSuccess = false;

    gammab = 1;
    gammar = 1;
    gammag = 1;
    gammaw = 1;
    gammaw2 = 1;
    this->leds = leds;
    this->saveleds = leds;
    this->numLedPerStrip = numLedPerStrip;
    offsetDisplay.offsetx = 0;
    offsetDisplay.offsety = 0;
    offsetDisplay.panelWidth = numLedPerStrip;
    offsetDisplay.panelHeight = 9999;
    defaultOffsetDisplay = offsetDisplay;
    linewidth = numLedPerStrip;
    this->numStrips = numStrips;
    // this->dmaBufferCount = dmaBufferCount;//this doesn't make sense as it is no parameter

    // Precompute cumulative strip offsets (used by PARLIO P4 transposition; available for all targets).
    firstIndexPerOutput[0] = 0;
    for (int i = 1; i < numStrips; i++) {
      firstIndexPerOutput[i] = firstIndexPerOutput[i - 1] + stripSize[i - 1];
    }

    setShowDelay();

    ESP_LOGV(TAG, "xdelay:%d", showDelay);
#if HARDWARESPRITES == 1
    // Serial.println(NUM_LEDS_PER_STRIP * NBIS2SERIALPINS * 8);
    target = (uint16_t*)malloc(numLedPerStrip * numStrips * 2 + 2);
    if (!target) {
      ESP_LOGE(TAG, "Failed to allocate hardware sprite target buffer!");
      initErrorOccurred = true;
      return;
    }
#endif

#ifdef __HARDWARE_MAP
  #ifndef __NON_HEAP
    hmap = (uint32_t*)malloc(totalLeds * 2);
    if (!hmap) {
      ESP_LOGE(TAG, "Failed to allocate hardware map buffer!");
      initErrorOccurred = true;
      return;
    }
  #endif
    if (!hmap) {
      ESP_LOGE(TAG, "no memory for the hamp");
      initErrorOccurred = true;
      return;
    } else {
      ESP_LOGE(TAG, "trying to map");
      /*
      for(int leddisp=0;leddisp<numLedPerStrip;leddisp++)
      {
          for (int i = 0; i < numStrips; i++)
          {
              hmap[i+leddisp*numStrips]=mapLed(leddisp+i*numLedPerStrip)*channelsPerLight;
          }
      }
      */
      // int offset=0;
      createhardwareMap();
    }
#endif
    /*
    // dmaBufferCount = 2;
    this->leds = leds;
    this->saveleds = leds;
    this->numLedPerStrip = numLedPerStrip;
    offsetDisplay.offsetx = 0;
    offsetDisplay.offsety = 0;
    offsetDisplay.panelWidth = numLedPerStrip;
    offsetDisplay.panelHeight = 9999;
    defaultOffsetDisplay = offsetDisplay;
    linewidth = numLedPerStrip;
    this->numStrips = numStrips;
    // this->dmaBufferCount = dmaBufferCount;
    */

    setPins(pinsq);

    if (s3patch_inclhwInit) {
      hwInit();
      if (initErrorOccurred) {
        initSuccess = false;
        return;
      }
    }

    setBrightness(brightness);
    if (initErrorOccurred) {
      initSuccess = false;
      return;
    };  // LUT allocation failed — stop early

    initTransferBuffers();

    initSuccess = !initErrorOccurred && numStrips > 0 && numLedPerStrip > 0;
  }

  // update driver: recreate dma buffers if numStrips or numLedPerStrip or dmaBuffer size changed
  void updateDriver(uint8_t* pinsq, uint16_t* sizes, uint8_t numStrips, uint8_t dmaBuffer, uint8_t channelsPerLight, uint8_t pR, uint8_t pG, uint8_t pB, uint8_t pW = UINT8_MAX, uint8_t pW2 = UINT8_MAX, bool extractWhiteFromRGB = false);
  // delete driver when the driver is stopped
  void deleteDriver();

#ifdef CONFIG_IDF_TARGET_ESP32S3
  typedef dma_descriptor_t I2SClocklessLedDriverDMABuffer;
#endif

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3
  // buffer array for the transposed leds
  I2SClocklessLedDriverDMABuffer** dmaBuffersTransposed = NULL;
  // buffer array for the regular way

  I2SClocklessLedDriverDMABuffer** transferBuffers = NULL;

  I2SClocklessLedDriverDMABuffer* allocateDMABuffer(int bytes);
  void hwStart();
  void i2sReset();
  void putdefaultones(uint16_t* buffer);
#endif
  void i2sResetDma() {
#ifdef CONFIG_IDF_TARGET_ESP32
    (&I2S0)->lc_conf.out_rst = 1;
    (&I2S0)->lc_conf.out_rst = 0;
#endif
  }

  void i2sResetFifo() {
#ifdef CONFIG_IDF_TARGET_ESP32
    (&I2S0)->conf.tx_fifo_reset = 1;
    (&I2S0)->conf.tx_fifo_reset = 0;
#endif
  }
  /*
      void   hwStop()
      {

          esp_intr_disable(intrHandle);

  esp_rom_delay_us(16);
          (&I2S0)->conf.tx_start = 0;
          while( (&I2S0)->conf.tx_start ==1){}
           i2sReset();

               isDisplaying =false;


          if(  wasWaitingtofinish == true)
          {

                 wasWaitingtofinish = false;
                    xSemaphoreGive(waitDisp);

          }


      } */

#ifdef CONFIG_IDF_TARGET_ESP32P4
  bool loadAndTranspose();
  void hwStart();
  void hwStop();
#endif

  // static void IRAM_ATTR interruptHandler(void *arg);
};

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3
  #include "esp32-d0s3_i2s_impl.h"
#elif CONFIG_IDF_TARGET_ESP32P4 && HAS_PARLIO_DRIVER
  #include "esp32-p4_parlio_impl.h"
#endif

#endif  // I2S_CLOCKLESS_DRIVER_H
