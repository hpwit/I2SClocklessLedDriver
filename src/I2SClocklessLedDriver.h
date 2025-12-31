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
 *  >=IDF5.5: NUM_STRIPS is a global variable (note the _ !), set in initled and update if number of strips change
 *
 *  ENABLE_HARDWARE_SCROLL : to enable the HARDWARE SCROLL. Attention wjhen enabled you can use the offset  but it could mean slow when using all the pins
 *  USE_PIXELSLIB : to use tthe pixel lib library automatic functions
 */

#ifndef __I2S_CLOCKLESS_DRIVER_H
#define __I2S_CLOCKLESS_DRIVER_H

#pragma once

#include "freertos/FreeRTOS.h"  // #error "include FreeRTOS.h" must appear in source files before "include semphr.h"

// IDF5.5: replace #include driver by #include esp_private
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  // esp_private needed for gpio_iomux_out (see below)
  #include <esp_private/gpio.h>
  #include <esp_private/periph_ctrl.h>

// NUM_LEDS_PER_STRIP not needed for physical driver as we set the __delay dynamically (Virtual driver check if it can be a variable)
// plus NUM_LEDS_PER_STRIP can not always be set before .h is loaded (it is loaded when .cpp is loaded)
// same for NUMSTRIPS, set as global variable (as needed by global function transpose16x1_noinline2)
// setGlobalNumStrips sets NUM_STRIPS dynamically
// setShowDelay sets (__delay) dynamically
#else
  #include <driver/periph_ctrl.h>

  #include "driver/gpio.h"

  #ifndef NUM_LEDS_PER_STRIP
    #pragma message "NUM_LEDS_PER_STRIP not defined, using default 256"
    #define NUM_LEDS_PER_STRIP 256
  #endif

  #define __delay (((NUM_LEDS_PER_STRIP * 125 * 8 * _nb_components) / 100000) + 1)  // used in waitDisplay

  #ifndef NUMSTRIPS
    #define NUMSTRIPS 16
  #endif

#endif

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
  int div_num;
  int div_a;
  int div_b;
} clock_speed;

// defined in .cpp (not needed for physical driver ...)
extern clock_speed clock_1123KHZ;
extern clock_speed clock_1111KHZ;
extern clock_speed clock_1000KHZ;
extern clock_speed clock_800KHZ;

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
  #include "hardwareSprite.h"
#endif

#ifdef USE_PIXELSLIB
  #include "pixelslib.h"
#else
  #include "___pixeltypes.h"
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

// IDF5.5: __NB_DMA_BUFFER is #define to allow changing it at runtime
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)
  #ifndef __NB_DMA_BUFFER
    #define __NB_DMA_BUFFER 6
  #endif
#endif

#define MAX_PINS 20  // maximum number of pins supported, was 16, set to 20, okay?

typedef union {
  uint8_t bytes[16];
  uint32_t shorts[8];
  uint32_t raw[2];
} Lines;

#ifdef CONFIG_IDF_TARGET_ESP32S3
static uint8_t signalsID[MAX_PINS] = {
    LCD_DATA_OUT0_IDX, LCD_DATA_OUT1_IDX, LCD_DATA_OUT2_IDX,  LCD_DATA_OUT3_IDX,  LCD_DATA_OUT4_IDX,  LCD_DATA_OUT5_IDX,  LCD_DATA_OUT6_IDX,  LCD_DATA_OUT7_IDX,
    LCD_DATA_OUT8_IDX, LCD_DATA_OUT9_IDX, LCD_DATA_OUT10_IDX, LCD_DATA_OUT11_IDX, LCD_DATA_OUT12_IDX, LCD_DATA_OUT13_IDX, LCD_DATA_OUT14_IDX, LCD_DATA_OUT15_IDX,

};
static gdma_channel_handle_t dma_chan;
#endif

class I2SClocklessLedDriver;

struct OffsetDisplay {
  int offsetx;
  int offsety;
  int panel_height;
  int panel_width;
};

// static const char *TAG = "I2SClocklessLedDriver";
#undef TAG
#define TAG "🐸"

#ifdef CONFIG_IDF_TARGET_ESP32S3
static bool _I2SClocklessLedDriverinterruptHandler(gdma_channel_handle_t dma_chan, gdma_event_data_t* event_data, void* user_data);
#else
static void _I2SClocklessLedDriverinterruptHandler(void* arg);
#endif

static void transpose16x1_noinline2(unsigned char* A, uint16_t* B);

/*
#ifdef ENABLE_HARDWARE_SCROLL
    static void loadAndTranspose(uint8_t *ledt, int led_per_strip, uint8_t num_stripst, OffsetDisplay offdisp, uint16_t *buffer, int ledtodisp, uint8_t *mapg, uint8_t *mapr, uint8_t *mapb, uint8_t
*mapw, int nbcomponents, int pg, int pr, int pb); #else static void loadAndTranspose(uint8_t *ledt, uint16_t *sizes, uint8_t num_stripst, uint16_t *buffer, int ledtodisp, uint8_t *mapg, uint8_t *mapr,
uint8_t *mapb, uint8_t *mapw, int nbcomponents, int pg, int pr, int pb); #endif
*/

static void loadAndTranspose(I2SClocklessLedDriver* driver);

enum colorarrangment {
  ORDER_GRBW,
  ORDER_RGB,
  ORDER_RBG,
  ORDER_GRB,
  ORDER_GBR,
  ORDER_BRG,
  ORDER_BGR,
};

enum displayMode {
  NO_WAIT,
  WAIT,
  LOOP,
  LOOP_INTERUPT,
};
/*
int MOD(int a, int b)
{

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
  uint32_t T0;
  uint32_t T1;
  uint32_t T2;

  // compileled
  uint8_t f1;
  uint8_t f2;
  uint8_t f3;
};

// IDF5.5: __NB_DMA_BUFFER is variable to allow changing it at runtime (defined in .cpp)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
extern uint8_t __NB_DMA_BUFFER;
extern uint8_t NUM_STRIPS;
#endif

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
  uint8_t __green_map[256];
  uint8_t __blue_map[256];
  uint8_t __red_map[256];
  uint8_t __white_map[256];
  uint8_t _brightness;
  float _gammar, _gammab, _gammag, _gammaw;
  intr_handle_t _gI2SClocklessDriver_intr_handle;
  volatile xSemaphoreHandle I2SClocklessLedDriver_sem = NULL;
  volatile xSemaphoreHandle I2SClocklessLedDriver_semSync = NULL;
  volatile xSemaphoreHandle I2SClocklessLedDriver_semDisp = NULL;
  volatile xSemaphoreHandle I2SClocklessLedDriver_waitDisp = NULL;
  volatile int dmaBufferActive = 0;
  volatile bool wait;
  displayMode __displayMode;
  displayMode __defaultdisplayMode;
  volatile int ledToDisplay;
  volatile int ledToDisplay_out;
  OffsetDisplay _offsetDisplay, _defaultOffsetDisplay;
  // volatile int oo=0;
  uint8_t *leds, *saveleds;
  int startleds;
  int linewidth;
  // uint8_t dmaBufferCount = __NB_DMA_BUFFER;  // we use two buffers
  volatile bool transpose = false;

  volatile uint8_t num_strips;
  volatile uint16_t num_led_per_strip;
  volatile uint16_t total_leds;
  // int clock_pin;
  uint8_t p_r, p_g, p_b, p_w;
  int i2s_base_pin_index;
  uint8_t nb_components;  // channels per LED
  uint16_t stripSize[MAX_PINS];
  uint16_t (*mapLed)(uint16_t led);

  bool isVirtualDriver = false;  // prepare for virtual driver integration

// IDF5.5: driver class: __delay is variable to allow changing it at runtime
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  TickType_t __delay = 0;
#endif

#ifdef __HARDWARE_MAP
  uint16_t* _hmap;
  volatile uint16_t* _hmapoff;
  void setHmap(uint16_t* map) { _hmap = map; }
#endif

#ifdef __HARDWARE_MAP_PROGMEM
  const uint16_t* _hmap;
  volatile uint16_t _hmapoff;

  void setHmap(const uint16_t* map) { _hmap = map; }
#endif

  void setMapLed(uint16_t (*newMapLed)(uint16_t led)) { mapLed = newMapLed; }

  /*
   This flag is used when using the NO_WAIT mode
   */
  volatile bool isDisplaying = false;
  volatile bool isWaiting = false;
  volatile bool __enableDriver = true;
  volatile bool framesync = false;
  volatile bool wasWaitingtofinish = false;
  volatile int counti;

  I2SClocklessLedDriver() {};

  void setPins(uint8_t* Pins) {
#ifdef CONFIG_IDF_TARGET_ESP32
    for (int i = 0; i < num_strips; i++) {
      PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[Pins[i]], PIN_FUNC_GPIO);
      gpio_set_direction((gpio_num_t)Pins[i], (gpio_mode_t)GPIO_MODE_DEF_OUTPUT);
      gpio_matrix_out(Pins[i], deviceBaseIndex[I2S_DEVICE] + i + 8, false, false);
    }
#elif CONFIG_IDF_TARGET_ESP32S3
    for (int i = 0; i < num_strips; i++) {
      esp_rom_gpio_connect_out_signal(Pins[i], signalsID[i], false, false);
        // gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[Pins[i]], PIN_FUNC_GPIO);
        // gpio_hal_func_sel(GPIO_PIN_MUX_REG[Pins[i]], PIN_FUNC_GPIO);

  // IDF5.5: setPins: use gpio_iomux_output instead of gpio_iomux_out suppress warning, ready for idf 6, see https://github.com/espressif/esp-idf/issues/17052
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
      gpio_iomux_output((gpio_num_t)Pins[i], PIN_FUNC_GPIO);
  #else
      gpio_iomux_out(Pins[i], PIN_FUNC_GPIO, false);
  #endif

      gpio_set_drive_capability((gpio_num_t)Pins[i], GPIO_DRIVE_CAP_3);
    }
#endif
  }

  // Corrected = 255 * (Image/255)^(1/2.2).

  void setBrightness(int brightness) {
    _brightness = brightness;
    float tmp;
    for (int i = 0; i < 256; i++) {
      tmp = powf((float)i / 255, 1 / _gammag);
      __green_map[i] = (uint8_t)(tmp * brightness);
      tmp = powf((float)i / 255, 1 / _gammab);
      __blue_map[i] = (uint8_t)(tmp * brightness);
      tmp = powf((float)i / 255, 1 / _gammar);
      __red_map[i] = (uint8_t)(tmp * brightness);
      tmp = powf((float)i / 255, 1 / _gammaw);
      __white_map[i] = (uint8_t)(tmp * brightness);
    }
  }

  void setGamma(float gammar, float gammab, float gammag, float gammaw) {
    _gammag = gammag;
    _gammar = gammar;
    _gammaw = gammaw;
    _gammab = gammab;
    setBrightness(_brightness);
  }

  void setGamma(float gammar, float gammab, float gammag) {
    _gammag = gammag;
    _gammar = gammar;
    _gammab = gammab;
    setBrightness(_brightness);
  }

  void i2sInit() {
#ifdef CONFIG_IDF_TARGET_ESP32S3
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
    // LCD_CAM.lcd_clock.lcd_clkm_div_num = 50;   //_clockspeed.div_num; // 1st stage 1:250 divide
    // LCD_CAM.lcd_clock.lcd_clkm_div_a = 1;      //_clockspeed.div_a;     // 0/1 fractional divide
    // LCD_CAM.lcd_clock.lcd_clkm_div_b = 0;      // Ò_clockspeed.div_b;

    LCD_CAM.lcd_clock.lcd_clkm_div_num = 50;  //_clockspeed.div_num; // 1st stage 1:250 divide
    LCD_CAM.lcd_clock.lcd_clkm_div_a = 1;     //_clockspeed.div_a;     // 0/1 fractional divide
    LCD_CAM.lcd_clock.lcd_clkm_div_b = 0;     // Ò_clockspeed.div_b;
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

  // IDF5.5: i2sInit: .isr_cache_safe=true results in Cache disabled but cached memory region accessed crash
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    gdma_channel_alloc_config_t dma_chan_config = {.sibling_chan = NULL, .direction = GDMA_CHANNEL_DIRECTION_TX, .flags = {.reserve_sibling = 0}};
    // .isr_cache_safe= true}};
    gdma_new_ahb_channel(&dma_chan_config, &dma_chan);  // note: s3 uses this, P4 uses gdma_new_axi_channel
  #else
    gdma_channel_alloc_config_t dma_chan_config = {.sibling_chan = NULL, .direction = GDMA_CHANNEL_DIRECTION_TX, .flags = {.reserve_sibling = 0, .isr_cache_safe = true}};
    gdma_new_channel(&dma_chan_config, &dma_chan);
  #endif
    gdma_connect(dma_chan, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0));
    gdma_strategy_config_t strategy_config = {.owner_check = false, .auto_update_desc = false};
    gdma_apply_strategy(dma_chan, &strategy_config);
    /*
    gdma_transfer_ability_t ability = {
        .psram_trans_align = 64,
        //.sram_trans_align = 64,
    };
    gdma_set_transfer_ability(dma_chan, &ability);
*/
    // Enable DMA transfer callback
    gdma_tx_event_callbacks_t tx_cbs = {.on_trans_eof = _I2SClocklessLedDriverinterruptHandler, .on_descr_err = NULL};
    gdma_register_tx_event_callbacks(dma_chan, &tx_cbs, this);
    // esp_intr_disable((*dma_chan).intr);
    LCD_CAM.lcd_user.lcd_start = 0;
#elif CONFIG_IDF_TARGET_ESP32
    int interruptSource;
    if (I2S_DEVICE == 0) {
      i2s = &I2S0;
      periph_module_enable(PERIPH_I2S0_MODULE);
      interruptSource = ETS_I2S0_INTR_SOURCE;
      i2s_base_pin_index = I2S0O_DATA_OUT0_IDX;
    } else {
      i2s = &I2S1;
      periph_module_enable(PERIPH_I2S1_MODULE);
      interruptSource = ETS_I2S1_INTR_SOURCE;
      i2s_base_pin_index = I2S1O_DATA_OUT0_IDX;
    }

    i2sReset();
    i2sReset_DMA();
    i2sReset_FIFO();
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
    esp_err_t e =
        esp_intr_alloc(interruptSource, ESP_INTR_FLAG_INTRDISABLED | ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM, &_I2SClocklessLedDriverinterruptHandler, this, &_gI2SClocklessDriver_intr_handle);
#endif
    // -- Create a semaphore to block execution until all the controllers are done

    if (I2SClocklessLedDriver_sem == NULL) {
      I2SClocklessLedDriver_sem = xSemaphoreCreateBinary();
    }

    if (I2SClocklessLedDriver_semSync == NULL) {
      I2SClocklessLedDriver_semSync = xSemaphoreCreateBinary();
    }
    if (I2SClocklessLedDriver_semDisp == NULL) {
      I2SClocklessLedDriver_semDisp = xSemaphoreCreateBinary();
    }
  }

  void initDMABuffers() {
/*
DMABuffersTampon[0] = allocateDMABuffer(nb_components * 8 * 2 * 3); // the buffers for the
DMABuffersTampon[1] = allocateDMABuffer(nb_components * 8 * 2 * 3);
DMABuffersTampon[2] = allocateDMABuffer(nb_components * 8 * 2 * 3);
DMABuffersTampon[3] = allocateDMABuffer(nb_components * 8 * 2 * 3 * 4);

putdefaultones((uint16_t *)DMABuffersTampon[0]->buffer);
putdefaultones((uint16_t *)DMABuffersTampon[1]->buffer);
*/

// IDF5.5: initDMABuffers: use heap_caps so it can be freed and reallocated, S3 can have it in PSRAM, D0-wrover not!
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  #ifdef CONFIG_IDF_TARGET_ESP32S3
    DMABuffersTampon = (I2SClocklessLedDriverDMABuffer**)heap_caps_calloc_prefer(__NB_DMA_BUFFER + 2, sizeof(I2SClocklessLedDriverDMABuffer*), 2, MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT);
  #elif CONFIG_IDF_TARGET_ESP32  // d0-wrover crashes with memory region error if set in PSRAM
    DMABuffersTampon = (I2SClocklessLedDriverDMABuffer**)heap_caps_calloc_prefer(__NB_DMA_BUFFER + 2, sizeof(I2SClocklessLedDriverDMABuffer*), 2, MALLOC_CAP_DEFAULT, MALLOC_CAP_DEFAULT);
  #endif
#endif

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32
    for (int i = 0; i < __NB_DMA_BUFFER + 1; i++) {
      DMABuffersTampon[i] = allocateDMABuffer(nb_components * 8 * 2 * 3);
    }
    DMABuffersTampon[__NB_DMA_BUFFER + 1] = allocateDMABuffer(nb_components * 8 * 2 * 3 * 4);

    for (int i = 0; i < __NB_DMA_BUFFER; i++) {
      putdefaultones((uint16_t*)DMABuffersTampon[i]->buffer);
    }
#endif

#ifdef FULL_DMA_BUFFER
    /*
     We do create n+2 buffers
     the first buffer is to be sure that everything is 0
     the last one is to put back the I2S at 0 the last bufffer is longer because when using the loop display mode the time between two frames needs to be longh enough.
     */
    DMABuffersTransposed = (I2SClocklessLedDriverDMABuffer**)malloc(sizeof(I2SClocklessLedDriverDMABuffer*) * (num_led_per_strip + 2));
    for (int i = 0; i < num_led_per_strip + 2; i++) {
      if (i < num_led_per_strip + 1)
        DMABuffersTransposed[i] = allocateDMABuffer(nb_components * 8 * 2 * 3);
      else
        DMABuffersTransposed[i] = allocateDMABuffer(nb_components * 8 * 2 * 3 * 4);
      if (i < num_led_per_strip) DMABuffersTransposed[i]->descriptor.eof = 0;
      if (i) {
        DMABuffersTransposed[i - 1]->descriptor.qe.stqe_next = &(DMABuffersTransposed[i]->descriptor);
        if (i < num_led_per_strip + 1) {
          putdefaultones((uint16_t*)DMABuffersTransposed[i]->buffer);
        }
      }
    }
#endif
  }

#ifdef FULL_DMA_BUFFER

  void stopDisplayLoop() { DMABuffersTransposed[num_led_per_strip + 1]->descriptor.qe.stqe_next = 0; }

  void showPixelsFromBuffer() { showPixelsFromBuffer(NO_WAIT); }

  void showPixelsFromBuffer(displayMode dispmode) {
    /*
     We cannot launch twice when in loopmode
     */
    if (__displayMode == LOOP && isDisplaying) {
      ESP_LOGE(TAG, "The loop mode is activated execute stopDisplayLoop() first");
      return;
    }
    /*
     We wait for the display to be stopped before launching a new one
     */

    __displayMode = dispmode;
    isWaiting = false;
    if (dispmode == LOOP or dispmode == LOOP_INTERUPT) {
      DMABuffersTransposed[num_led_per_strip + 1]->descriptor.qe.stqe_next = &(DMABuffersTransposed[0]->descriptor);
    }
    transpose = false;
    // wasWaitingtofinish = true;
    //  Serial.printf(" was:%d\n",wasWaitingtofinish);
    i2sStart(DMABuffersTransposed[0]);

    if (dispmode == WAIT) {
      isWaiting = true;
      if (I2SClocklessLedDriver_sem == NULL) I2SClocklessLedDriver_sem = xSemaphoreCreateBinary();
      if (xSemaphoreTake(I2SClocklessLedDriver_sem, pdMS_TO_TICKS(100)) == pdFALSE) {
        ESP_LOGE("TAG", "I2SClocklessLedDriver_sem wait too long");
        xSemaphoreTake(I2SClocklessLedDriver_sem, portMAX_DELAY);
      }
    }
  }

  void showPixelsFirstTranspose(OffsetDisplay offdisp) {
    _offsetDisplay = offdisp;
    showPixelsFirstTranspose();
    _offsetDisplay = _defaultOffsetDisplay;
  }

  void showPixelsFirstTranspose(OffsetDisplay offdisp, uint8_t* temp_leds) {
    _offsetDisplay = offdisp;
    showPixelsFirstTranspose(temp_leds);
    _offsetDisplay = _defaultOffsetDisplay;
  }

  void showPixelsFirstTranspose(uint8_t* new_leds) {
    uint8_t* tmp_leds;
    //  Serial.println("on entre");
    if (isDisplaying == true && __displayMode == NO_WAIT) {
      // Serial.println("we are here in trs");
      wasWaitingtofinish = true;
      tmp_leds = new_leds;
      if (I2SClocklessLedDriver_waitDisp == NULL) I2SClocklessLedDriver_waitDisp = xSemaphoreCreateCounting(10, 0);
      if (xSemaphoreTake(I2SClocklessLedDriver_waitDisp, pdMS_TO_TICKS(100)) == pdFALSE) {
        ESP_LOGE("TAG", "I2SClocklessLedDriver_waitDisp wait too long");
        xSemaphoreTake(I2SClocklessLedDriver_waitDisp, portMAX_DELAY);
      }
    }
    leds = new_leds;
    showPixelsFirstTranspose();
    // leds = tmp_leds;
  }

  void showPixelsFirstTranspose() { showPixelsFirstTranspose(NO_WAIT); }
  void showPixelsFirstTranspose(displayMode dispmode) {
    // Serial.println("on entrre");
    transpose = false;
    if (leds == NULL) {
      ESP_LOGE(TAG, "no led");
      return;
    }
    if (isDisplaying == true && dispmode == NO_WAIT) {
      Serial.println("we are here");
      wasWaitingtofinish = true;
      if (I2SClocklessLedDriver_waitDisp == NULL) I2SClocklessLedDriver_waitDisp = xSemaphoreCreateCounting(10, 0);
      if (xSemaphoreTake(I2SClocklessLedDriver_waitDisp, pdMS_TO_TICKS(100)) == pdFALSE) {
        ESP_LOGE("TAG", "I2SClocklessLedDriver_waitDisp wait too long");
        xSemaphoreTake(I2SClocklessLedDriver_waitDisp, portMAX_DELAY);
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
    /*Lines secondPixel[nb_components];
    for (int j = 0; j < num_led_per_strip; j++)
    {
        uint8_t *poli = leds + ledToDisplay * nb_components;
        for (int i = 0; i < num_strips; i++)
        {

            secondPixel[p_g].bytes[i] = __green_map[*(poli + 1)];
            secondPixel[p_r].bytes[i] = __red_map[*(poli + 0)];
            secondPixel[p_b].bytes[i] = __blue_map[*(poli + 2)];
            if (p_w != UINT8_MAX)
                secondPixel[p_w].bytes[i] = __white_map[*(poli + 3)];
            //#endif
            poli += num_led_per_strip * nb_components;
        }
        ledToDisplay++;
        transpose16x1_noinline2(secondPixel[0].bytes, (uint16_t *)DMABuffersTransposed[j + 1]->buffer);
        transpose16x1_noinline2(secondPixel[1].bytes, (uint16_t *)DMABuffersTransposed[j + 1]->buffer + 3 * 8);
        transpose16x1_noinline2(secondPixel[2].bytes, (uint16_t *)DMABuffersTransposed[j + 1]->buffer + 2 * 3 * 8);
        if (p_w != UINT8_MAX)
            transpose16x1_noinline2(secondPixel[3].bytes, (uint16_t *)DMABuffersTransposed[j + 1]->buffer + 3 * 3 * 8);
    }*/
    for (int j = 0; j < num_led_per_strip; j++) {
      ledToDisplay = j;
      dmaBufferActive = j + 1;
      loadAndTranspose(this);
      /*
      #ifdef ENABLE_HARDWARE_SCROLL
      loadAndTranspose(leds, num_led_per_strip, num_strips, _offsetDisplay, (uint16_t *)DMABuffersTransposed[j + 1]->buffer, j, __green_map, __red_map, __blue_map, __white_map, nb_components, p_g,
      p_r, p_b); #else loadAndTranspose(leds, stripSize, num_strips, (uint16_t *)DMABuffersTransposed[j+1]->buffer, j, __green_map, __red_map, __blue_map, __white_map, nb_components, p_g, p_r, p_b);
      #endif
      */
    }
  }

  void setPixelinBufferByStrip(int stripNumber, int posOnStrip, uint8_t red, uint8_t green, uint8_t blue) {
    uint8_t W = 0;
    if (p_w != UINT8_MAX) {
      W = MIN(red, green);
      W = MIN(W, blue);
      red = red - W;
      green = green - W;
      blue = blue - W;
    }
    setPixelinBufferByStrip(stripNumber, posOnStrip, red, green, blue, W);
  }

  void setPixelinBufferByStrip(int stripNumber, int posOnStrip, uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
    uint16_t mask = ~(1 << stripNumber);
    uint8_t colors[3];
    colors[p_g] = __green_map[green];
    colors[p_r] = __red_map[red];
    colors[p_b] = __blue_map[blue];
    uint16_t* B = (uint16_t*)DMABuffersTransposed[posOnStrip + 1]->buffer;
    // printf("nb c:%d\n",nb_components);
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
    if (p_w != UINT8_MAX) {
      B += 3 * 8;
      y = __white_map[white];
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

  void setPixelinBuffer(uint32_t pos, uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
    int stripNumber = -1;
    int total = 0;
    int posOnStrip = pos;
    if (pos > total_leds - 1) {
      printf("Position out of bound %d > %d\n", pos, total_leds - 1);
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

  void setPixelinBuffer(uint32_t pos, uint8_t red, uint8_t green, uint8_t blue) {
    uint8_t W = 0;
    if (p_w != UINT8_MAX) {
      W = MIN(red, green);
      W = MIN(W, blue);
      red = red - W;
      green = green - W;
      blue = blue - W;
    }

    setPixelinBuffer(pos, red, green, blue, W);
  }

  void initled(uint8_t* Pinsq, uint8_t num_strips, uint16_t num_led_per_strip) { initled(NULL, Pinsq, num_strips, num_led_per_strip); }
  void waitSync() {
    I2SClocklessLedDriver_semSync = xSemaphoreCreateBinary();
    if (xSemaphoreTake(I2SClocklessLedDriver_semSync, pdMS_TO_TICKS(100)) == pdFALSE) {
      ESP_LOGE("TAG", "I2SClocklessLedDriver_semSync wait too long");
      xSemaphoreTake(I2SClocklessLedDriver_semSync, portMAX_DELAY);
    }
  }
#endif
  void setPixel(uint32_t pos, uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
    uint8_t* offset = leds + (pos << 2);  // faster than doing * 4
    *(offset) = red;
    *(++offset) = green;
    *(++offset) = blue;
    *(++offset) = white;
  }

  void setPixel(uint32_t pos, uint8_t red, uint8_t green, uint8_t blue) {
    if (nb_components == 3) {
      uint8_t* offset = leds + (pos << 1) + pos;
      *(offset) = red;
      *(++offset) = green;
      *(++offset) = blue;
    } else {
      /*
          Code to transform RBG into RGBW thanks to @Jonathanese https://github.com/Jonathanese/NodeMCUPoleDriver/blob/master/LED_Framework.cpp
      */
      uint8_t W = MIN(red, green);
      W = MIN(W, blue);
      red = red - W;
      green = green - W;
      blue = blue - W;
      setPixel(pos, red, green, blue, W);
    }
  }

  OffsetDisplay getDefaultOffset() { return _defaultOffsetDisplay; }

  void waitDisplay() {
    if (isDisplaying == true) {
      wasWaitingtofinish = true;
      ESP_LOGD(TAG, "already displaying... wait");
      if (I2SClocklessLedDriver_waitDisp == NULL) {
        I2SClocklessLedDriver_waitDisp = xSemaphoreCreateCounting(10, 0);
      }
      const TickType_t xDelay = __delay;
      xSemaphoreTake(I2SClocklessLedDriver_waitDisp, xDelay);
    }
    isDisplaying = true;
  }

  void showPixels(displayMode dispmode, uint8_t* new_leds, OffsetDisplay offdisp) {
    waitDisplay();
    _offsetDisplay = offdisp;
    leds = new_leds;
    __displayMode = dispmode;
    __showPixels();
  }
  void showPixels(uint8_t* new_leds, OffsetDisplay offdisp) {
    waitDisplay();
    _offsetDisplay = offdisp;
    leds = new_leds;
    __displayMode = WAIT;
    __showPixels();
    // _offsetDisplay = _defaultOffsetDisplay;
  }

  void showPixels(OffsetDisplay offdisp) {
    waitDisplay();
    _offsetDisplay = offdisp;
    leds = saveleds;

    __displayMode = WAIT;
    __showPixels();
    // _offsetDisplay = _defaultOffsetDisplay;
  }

  void showPixels(uint8_t* newleds) {
    waitDisplay();
    leds = newleds;
    __displayMode = WAIT;
    _offsetDisplay = _defaultOffsetDisplay;
    __showPixels();
  }

  void showPixels() {
    if (!__enableDriver) return;
    waitDisplay();
    leds = saveleds;
    _offsetDisplay = _defaultOffsetDisplay;
    __displayMode = WAIT;
    __showPixels();
  }

  void showPixels(displayMode dispmode, uint8_t* newleds) {
    waitDisplay();
    _offsetDisplay = _defaultOffsetDisplay;
    leds = newleds;
    __displayMode = dispmode;
    __showPixels();
    // leds = tmp_leds;
  }

  void showPixels(displayMode dispmode) {
    waitDisplay();
    leds = saveleds;
    _offsetDisplay = _defaultOffsetDisplay;
    __displayMode = dispmode;
    __showPixels();
  }

  void __showPixels() {
    if (!__enableDriver) {
      return;
    }
#ifdef __HARDWARE_MAP
    _hmapoff = _hmap;

#endif
#ifdef __HARDWARE_MAP_HARDWARE
    _hmapoff = 0;

#endif

    if (leds == NULL) {
      ESP_LOGE(TAG, "no leds buffer defined");
      return;
    }
    ledToDisplay = 0;
    transpose = true;
#ifdef CONFIG_IDF_TARGET_ESP32
    for (int buff_num = 0; buff_num < __NB_DMA_BUFFER - 1; buff_num++) {
      DMABuffersTampon[buff_num]->descriptor.qe.stqe_next = &(DMABuffersTampon[buff_num + 1]->descriptor);
    }

    DMABuffersTampon[__NB_DMA_BUFFER - 1]->descriptor.qe.stqe_next = &(DMABuffersTampon[0]->descriptor);
    DMABuffersTampon[__NB_DMA_BUFFER]->descriptor.qe.stqe_next = &(DMABuffersTampon[0]->descriptor);
    DMABuffersTampon[__NB_DMA_BUFFER + 1]->descriptor.qe.stqe_next = 0;

#elif CONFIG_IDF_TARGET_ESP32S3
    for (int buff_num = 0; buff_num < __NB_DMA_BUFFER - 1; buff_num++) {
      DMABuffersTampon[buff_num]->next = DMABuffersTampon[buff_num + 1];
    }
    DMABuffersTampon[__NB_DMA_BUFFER - 1]->next = DMABuffersTampon[0];
    DMABuffersTampon[__NB_DMA_BUFFER]->next = DMABuffersTampon[0];
    DMABuffersTampon[__NB_DMA_BUFFER + 1]->next = DMABuffersTampon[__NB_DMA_BUFFER + 1];
#endif
#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32

    ledToDisplay = 0;
    dmaBufferActive = 0;
    for (int num_buff = 0; num_buff < __NB_DMA_BUFFER - 1; num_buff++) {
      loadAndTranspose(this);
      dmaBufferActive = dmaBufferActive + 1;
      ledToDisplay = ledToDisplay + 1;
    }
    ledToDisplay = ledToDisplay - 1;
    dmaBufferActive = __NB_DMA_BUFFER - 1;
    ledToDisplay_out = 0;
    isDisplaying = true;
    i2sStart(DMABuffersTampon[__NB_DMA_BUFFER]);

    if (__displayMode == WAIT) {
      isWaiting = true;
      if (I2SClocklessLedDriver_sem == NULL) I2SClocklessLedDriver_sem = xSemaphoreCreateBinary();
      if (xSemaphoreTake(I2SClocklessLedDriver_sem, pdMS_TO_TICKS(100)) == pdFALSE) {
        ESP_LOGE("TAG", "I2SClocklessLedDriver_sem wait too long");
        xSemaphoreTake(I2SClocklessLedDriver_sem, portMAX_DELAY);
      }
    } else {
      isWaiting = false;
      isDisplaying = true;
    }
#endif
  }

  Pixel* strip(int stripNum) {
    Pixel* l = (Pixel*)leds;
    // Serial.printf(" strip %d\n",stripNum);

    for (int i = 0; i < (stripNum % num_strips); i++) {
      // Serial.printf("     strip %d\n",stripSize[i]);
      l = l + stripSize[i];
    }
    return l;
  }

  uint16_t maxLength(uint16_t* sizes, uint8_t num_strips) {
    uint16_t max = 0;
    for (int i = 0; i < num_strips; i++) {
      if (max < sizes[i]) {
        max = sizes[i];
      }
    }
    return max;
  }

#ifdef USE_PIXELSLIB
  void initled(Pixels pix, uint8_t* Pinsq) { initled((uint8_t*)pix.getPixels(), Pinsq, pix.getLengths(), pix.getNumStrip()); }
#endif
  // initled with default color arrangement GRB
  void initled(uint8_t* leds, uint8_t* Pinsq, uint16_t* sizes, uint8_t num_strips) {
    total_leds = 0;
    for (int i = 0; i < num_strips; i++) {
      this->stripSize[i] = sizes[i];
      total_leds += sizes[i];
    }
    uint16_t maximum = maxLength(sizes, num_strips);
    // Serial.printf("maximum %d\n",maximum);
    ESP_LOGV(TAG, "maximum leds %d", maximum);
    nb_components = 3;
    p_r = 1;
    p_g = 0;
    p_b = 2;
    p_w = UINT8_MAX;
    __initled(leds, Pinsq, num_strips, maximum);
  }

  // initled with custom color arrangement
  void initled(uint8_t* leds, uint8_t* Pinsq, uint16_t* sizes, uint8_t num_strips, uint8_t nb_components, uint8_t p_r, uint8_t p_g, uint8_t p_b, uint8_t p_w = UINT8_MAX) {
    total_leds = 0;
    for (int i = 0; i < num_strips; i++) {
      this->stripSize[i] = sizes[i];
      total_leds += sizes[i];
    }
    uint16_t maximum = maxLength(sizes, num_strips);
    // Serial.printf("maximum %d\n",maximum);
    ESP_LOGD(TAG, "maximum leds %d", maximum);
    this->nb_components = nb_components;
    this->p_r = p_r;
    this->p_g = p_g;
    this->p_b = p_b;
    this->p_w = p_w;
    __initled(leds, Pinsq, num_strips, maximum);
  }

  void initled(uint8_t* leds, uint8_t* Pinsq, uint8_t num_strips, uint16_t num_led_per_strip) {
    for (int i = 0; i < num_strips; i++) {
      this->stripSize[i] = num_led_per_strip;
    }
    initled(leds, Pinsq, this->stripSize, num_strips);
  }

  void initled(uint8_t* leds, uint8_t* Pinsq, uint16_t* sizes, uint8_t num_strips, colorarrangment cArr) {
    total_leds = 0;
    for (int i = 0; i < num_strips; i++) {
      this->stripSize[i] = sizes[i];
      total_leds += sizes[i];
    }
    uint16_t maximum = maxLength(sizes, num_strips);

    p_w = UINT8_MAX;
    switch (cArr) {
    case ORDER_RGB:
      nb_components = 3;
      p_r = 0;
      p_g = 1;
      p_b = 2;
      break;
    case ORDER_RBG:
      nb_components = 3;
      p_r = 0;
      p_g = 2;
      p_b = 1;
      break;
    case ORDER_GRB:
      nb_components = 3;
      p_r = 1;
      p_g = 0;
      p_b = 2;
      break;
    case ORDER_GBR:
      nb_components = 3;
      p_r = 2;
      p_g = 0;
      p_b = 1;
      break;
    case ORDER_BRG:
      nb_components = 3;
      p_r = 1;
      p_g = 2;
      p_b = 0;
      break;
    case ORDER_BGR:
      nb_components = 3;
      p_r = 2;
      p_g = 1;
      p_b = 0;
      break;
    case ORDER_GRBW:
      nb_components = 4;
      p_r = 1;
      p_g = 0;
      p_b = 2;
      p_w = 3;
      break;
    }
    __initled(leds, Pinsq, num_strips, maximum);
  }

  void initled(uint8_t* leds, uint8_t* Pinsq, uint8_t num_strips, uint16_t num_led_per_strip, colorarrangment cArr) {
    for (int i = 0; i < num_strips; i++) {
      this->stripSize[i] = num_led_per_strip;
    }
    initled(leds, Pinsq, this->stripSize, num_strips, cArr);
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
    int offset2 = 0;
    for (int leddisp = 0; leddisp < num_led_per_strip; leddisp++) {
      int offset = 0;
      for (int i = 0; i < num_strips; i++) {
        if (leddisp < stripSize[i]) {
          // ESP_LOGE(TAG,"%d :%d",leddisp+offset,mapLed(leddisp+offset));
          _hmap[offset2] = mapLed(leddisp + offset) * nb_components;
          offset += stripSize[i];
          offset2++;
        }
      }
    }
#endif
  }

// IDF5.5: call setGlobalNumStrips and setShowDelay if num_strips resp. num_led_per_strip is changed after initled
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  void setGlobalNumStrips() { NUM_STRIPS = num_strips; }
  void setShowDelay() { __delay = (((num_led_per_strip * 125 * 8 * nb_components) / 100000) + 1); }
#endif

  void __initled(uint8_t* leds, uint8_t* Pinsq, uint8_t num_strips, uint16_t num_led_per_strip) {
    _gammab = 1;
    _gammar = 1;
    _gammag = 1;
    _gammaw = 1;
    startleds = 0;
    this->leds = leds;
    this->saveleds = leds;
    this->num_led_per_strip = num_led_per_strip;
    _offsetDisplay.offsetx = 0;
    _offsetDisplay.offsety = 0;
    _offsetDisplay.panel_width = num_led_per_strip;
    _offsetDisplay.panel_height = 9999;
    _defaultOffsetDisplay = _offsetDisplay;
    linewidth = num_led_per_strip;
    this->num_strips = num_strips;
    // this->dmaBufferCount = dmaBufferCount;//this doesn't make sense as it is no parameter

// IDF5.5: initled: call setGlobalNumStrips and setShowDelay
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    setGlobalNumStrips();
    setShowDelay();
#endif

    ESP_LOGV(TAG, "xdelay:%d", __delay);
#if HARDWARESPRITES == 1
    // Serial.println(NUM_LEDS_PER_STRIP * NBIS2SERIALPINS * 8);
    target = (uint16_t*)malloc(num_led_per_strip * num_strips * 2 + 2);
#endif

#ifdef __HARDWARE_MAP
  #ifndef __NON_HEAP
    _hmap = (uint16_t*)malloc(total_leds * 2);
  #endif
    if (!_hmap) {
      ESP_LOGE(TAG, "no memory for the hamp");
      return;
    } else {
      ESP_LOGE(TAG, "trying to map");
      /*
      for(int leddisp=0;leddisp<num_led_per_strip;leddisp++)
      {
          for (int i = 0; i < num_strips; i++)
          {
              _hmap[i+leddisp*num_strips]=mapLed(leddisp+i*num_led_per_strip)*nb_components;
          }
      }
      */
      // int offset=0;
      createhardwareMap();
    }
#endif
    setBrightness(255);
    /*
    // dmaBufferCount = 2;
    this->leds = leds;
    this->saveleds = leds;
    this->num_led_per_strip = num_led_per_strip;
    _offsetDisplay.offsetx = 0;
    _offsetDisplay.offsety = 0;
    _offsetDisplay.panel_width = num_led_per_strip;
    _offsetDisplay.panel_height = 9999;
    _defaultOffsetDisplay = _offsetDisplay;
    linewidth = num_led_per_strip;
    this->num_strips = num_strips;
    // this->dmaBufferCount = dmaBufferCount;

    // IDF5.5: initled: call setGlobalNumStrips and setShowDelay
    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
        setGlobalNumStrips();
        setShowDelay();
    #endif
    */

    setPins(Pinsq);
    i2sInit();
    initDMABuffers();
  }

// IDF5.5: updateLeds
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  // recreate dma buffers if num_strips or num_led_per_strip or dmaBuffer size changed
  void updateDriver(uint8_t* Pinsq, uint16_t* sizes, uint8_t num_strips, uint8_t dmaBuffer, uint8_t nb_components, uint8_t p_r, uint8_t p_g, uint8_t p_b, uint8_t p_w = UINT8_MAX);
  void deleteDriver();
#endif

#ifdef CONFIG_IDF_TARGET_ESP32S3
  typedef dma_descriptor_t I2SClocklessLedDriverDMABuffer;
#endif

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32
  // buffer array for the transposed leds
  I2SClocklessLedDriverDMABuffer** DMABuffersTransposed = NULL;
  // buffer array for the regular way

  // IDF5.5: initled: DMABuffersTampon dynamically allocated to allow to delete and reallocate with different __NB_DMA_BUFFER value and free memory if needed
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  I2SClocklessLedDriverDMABuffer** DMABuffersTampon = NULL;
  #else
  I2SClocklessLedDriverDMABuffer* DMABuffersTampon[__NB_DMA_BUFFER + 2];
  #endif

  I2SClocklessLedDriverDMABuffer* allocateDMABuffer(int bytes) {
    I2SClocklessLedDriverDMABuffer* b = (I2SClocklessLedDriverDMABuffer*)heap_caps_malloc(sizeof(I2SClocklessLedDriverDMABuffer), MALLOC_CAP_DMA);
    if (!b) {
      ESP_LOGE(TAG, "No more memory\n");
      return NULL;
    }

    b->buffer = (uint8_t*)heap_caps_malloc(bytes, MALLOC_CAP_DMA);
    if (!b->buffer) {
      ESP_LOGE(TAG, "No more memory\n");
      return NULL;
    }
    memset(b->buffer, 0, bytes);
  #ifdef CONFIG_IDF_TARGET_ESP32S3
    b->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
    b->dw0.size = bytes;
    b->dw0.length = bytes;
    b->dw0.suc_eof = 1;

  #elif CONFIG_IDF_TARGET_ESP32
    b->descriptor.length = bytes;
    b->descriptor.size = bytes;
    b->descriptor.owner = 1;
    b->descriptor.sosf = 1;
    b->descriptor.buf = b->buffer;
    b->descriptor.offset = 0;
    b->descriptor.empty = 0;
    b->descriptor.eof = 1;
    b->descriptor.qe.stqe_next = 0;
  #endif
    return b;
  }
#endif
  void i2sReset_DMA() {
#ifdef CONFIG_IDF_TARGET_ESP32
    (&I2S0)->lc_conf.out_rst = 1;
    (&I2S0)->lc_conf.out_rst = 0;
#endif
  }

  void i2sReset_FIFO() {
#ifdef CONFIG_IDF_TARGET_ESP32
    (&I2S0)->conf.tx_fifo_reset = 1;
    (&I2S0)->conf.tx_fifo_reset = 0;
#endif
  }
  /*
      void   i2sStop()
      {

          esp_intr_disable(_gI2SClocklessDriver_intr_handle);

  ets_delay_us(16);
          (&I2S0)->conf.tx_start = 0;
          while( (&I2S0)->conf.tx_start ==1){}
           i2sReset();

               isDisplaying =false;


          if(  wasWaitingtofinish == true)
          {

                 wasWaitingtofinish = false;
                    xSemaphoreGive(I2SClocklessLedDriver_waitDisp);

          }


      } */

  void putdefaultones(uint16_t* buffer) {
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
#ifdef CONFIG_IDF_TARGET_ESP32S3
    for (int i = 0; i < nb_components * 8; i++) {
      buffer[i * 3 + 0] = 0xffff;
      // buffer[i * 6 + 2] = 0xffff;
    }
#elif CONFIG_IDF_TARGET_ESP32
    for (int i = 0; i < nb_components * 8 / 2; i++) {
      buffer[i * 6 + 1] = 0xffff;
      buffer[i * 6 + 2] = 0xffff;
    }
#endif
  }

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32
  void i2sStart(I2SClocklessLedDriverDMABuffer* startBuffer) {
  #ifdef CONFIG_IDF_TARGET_ESP32S3

    LCD_CAM.lcd_user.lcd_start = 0;
    gdma_reset(dma_chan);
    LCD_CAM.lcd_user.lcd_dout = 1;    // Enable data out
    LCD_CAM.lcd_user.lcd_update = 1;  // Update registers
    LCD_CAM.lcd_misc.lcd_afifo_reset = 1;

    //    memset(startBuffer->buffer,0,WS2812_DMA_DESCRIPTOR_BUFFER_MAX_SIZE);
    gdma_start(dma_chan, (intptr_t)startBuffer);  // Start DMA w/updated descriptor(s)
    // esp_intr_enable(dma_chan->intr);
    // vTaskDelay(1);                         // Must 'bake' a moment before...
    LCD_CAM.lcd_user.lcd_start = 1;
  #elif CONFIG_IDF_TARGET_ESP32
    i2sReset();
    framesync = false;
    counti = 0;

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
    esp_intr_enable(_gI2SClocklessDriver_intr_handle);

    // We start the I2S
    (&I2S0)->conf.tx_start = 1;
  #endif
    // Set the mode to indicate that we've started
    isDisplaying = true;
  }
#endif

  void IRAM_ATTR i2sReset() {
#ifdef CONFIG_IDF_TARGET_ESP32S3
    gdma_reset(dma_chan);
    LCD_CAM.lcd_misc.lcd_afifo_reset = 1;
#elif CONFIG_IDF_TARGET_ESP32
    const unsigned long lc_conf_reset_flags = I2S_IN_RST_M | I2S_OUT_RST_M | I2S_AHBM_RST_M | I2S_AHBM_FIFO_RST_M;
    (&I2S0)->lc_conf.val |= lc_conf_reset_flags;
    (&I2S0)->lc_conf.val &= ~lc_conf_reset_flags;
    const uint32_t conf_reset_flags = I2S_RX_RESET_M | I2S_RX_FIFO_RESET_M | I2S_TX_RESET_M | I2S_TX_FIFO_RESET_M;
    (&I2S0)->conf.val |= conf_reset_flags;
    (&I2S0)->conf.val &= ~conf_reset_flags;
#endif
  }

  // static void IRAM_ATTR interruptHandler(void *arg);
};
static void IRAM_ATTR i2sStop(I2SClocklessLedDriver* cont) {
#ifdef CONFIG_IDF_TARGET_ESP32S3

  // gdma_disconnect(dma_chan);
  LCD_CAM.lcd_user.lcd_start = 0;

  while (LCD_CAM.lcd_user.lcd_start) {
  }
  gdma_stop(dma_chan);
  ets_delay_us(16);  // for sk6812
                     // esp_intr_disable(dma_chan->intr);
#elif CONFIG_IDF_TARGET_ESP32
  esp_intr_disable(cont->_gI2SClocklessDriver_intr_handle);

  ets_delay_us(16);
  (&I2S0)->conf.tx_start = 0;
  while ((&I2S0)->conf.tx_start == 1) {
  }
#endif
  cont->i2sReset();

  cont->isDisplaying = false;

  if (cont->wasWaitingtofinish == true) {
    cont->wasWaitingtofinish = false;
    xSemaphoreGive(cont->I2SClocklessLedDriver_waitDisp);
  }
  if (cont->isWaiting) {
    // printf("on debloqu\n");
    xSemaphoreGive(cont->I2SClocklessLedDriver_sem);
  }
}

#ifdef CONFIG_IDF_TARGET_ESP32S3

static IRAM_ATTR bool _I2SClocklessLedDriverinterruptHandler(gdma_channel_handle_t dma_chan, gdma_event_data_t* event_data, void* user_data) {
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
  I2SClocklessLedDriver* cont = (I2SClocklessLedDriver*)user_data;

  if (!cont->__enableDriver) {
    // cont->i2sStop(cont);
    i2sStop(cont);
    return true;
  }

  cont->framesync = !cont->framesync;

  // cont->ledToDisplay_in[cont->ledToDisplay_out]=cont->ledToDisplay+1;
  // cont->ledToDisplay_inbuffer[cont->ledToDisplay_out]=cont->dmaBufferActive;

  if (cont->transpose) {
    cont->ledToDisplay = cont->ledToDisplay + 1;
    if (cont->ledToDisplay < cont->num_led_per_strip) {
      loadAndTranspose(cont);

      if (cont->ledToDisplay_out == (cont->num_led_per_strip - __NB_DMA_BUFFER))  // here it's not -1 because it takes time top have the change into account and it reread the buufer
      {
        cont->DMABuffersTampon[(cont->dmaBufferActive) % __NB_DMA_BUFFER]->next = (cont->DMABuffersTampon[__NB_DMA_BUFFER + 1]);
        // cont->ledToDisplay_inbufferfor[cont->ledToDisplay_out]=cont->dmaBufferActive;
      }

      cont->dmaBufferActive = (cont->dmaBufferActive + 1) % __NB_DMA_BUFFER;
    }
    cont->ledToDisplay_out = cont->ledToDisplay_out + 1;
    if (cont->ledToDisplay >= cont->num_led_per_strip + __NB_DMA_BUFFER + 1) {
      i2sStop(cont);
    }
  } else {
    if (cont->framesync) {
      portBASE_TYPE HPTaskAwoken = 0;
      xSemaphoreGiveFromISR(cont->I2SClocklessLedDriver_semSync, &HPTaskAwoken);
      if (HPTaskAwoken == pdTRUE) portYIELD_FROM_ISR();
    }
  }
  return true;
}
#elif CONFIG_IDF_TARGET_ESP32
static void IRAM_ATTR _I2SClocklessLedDriverinterruptHandler(void* arg) {
  #ifdef DO_NOT_USE_INTERUPT
  REG_WRITE(I2S_INT_CLR_REG(0), (REG_READ(I2S_INT_RAW_REG(0)) & 0xffffffc0) | 0x3f);
  return;
  #else
  I2SClocklessLedDriver* cont = (I2SClocklessLedDriver*)arg;

  if (!cont->__enableDriver) {
    REG_WRITE(I2S_INT_CLR_REG(0), (REG_READ(I2S_INT_RAW_REG(0)) & 0xffffffc0) | 0x3f);
    // ((I2SClocklessLedDriver *)arg)->i2sStop();
    i2sStop(cont);
    return;
  }
  if (GET_PERI_REG_BITS(I2S_INT_ST_REG(I2S_DEVICE), I2S_OUT_EOF_INT_ST_S, I2S_OUT_EOF_INT_ST_S)) {
    cont->framesync = !cont->framesync;

    if (((I2SClocklessLedDriver*)arg)->transpose) {
      cont->ledToDisplay = cont->ledToDisplay + 1;  //++ gives volatile warning
      if (cont->ledToDisplay < cont->num_led_per_strip) {
        loadAndTranspose(cont);

        if (cont->ledToDisplay_out == cont->num_led_per_strip - __NB_DMA_BUFFER)  // here it's not -1 because it takes time top have the change into account and it reread the buufer
        {
          cont->DMABuffersTampon[(cont->dmaBufferActive) % __NB_DMA_BUFFER]->descriptor.qe.stqe_next = &(cont->DMABuffersTampon[__NB_DMA_BUFFER + 1]->descriptor);
        }
        cont->dmaBufferActive = (cont->dmaBufferActive + 1) % __NB_DMA_BUFFER;
      }
      cont->ledToDisplay_out = cont->ledToDisplay_out + 1;  //++ gives volatile warning
    } else {
      if (cont->framesync) {
        portBASE_TYPE HPTaskAwoken = 0;
        xSemaphoreGiveFromISR(cont->I2SClocklessLedDriver_semSync, &HPTaskAwoken);
        if (HPTaskAwoken == pdTRUE) portYIELD_FROM_ISR();
      }
    }
  }

  if (GET_PERI_REG_BITS(I2S_INT_ST_REG(I2S_DEVICE), I2S_OUT_TOTAL_EOF_INT_ST_S, I2S_OUT_TOTAL_EOF_INT_ST_S)) {
    // ((I2SClocklessLedDriver *)arg)->i2sStop();
    i2sStop(cont);
    if (cont->isWaiting) {
      portBASE_TYPE HPTaskAwoken = 0;
      xSemaphoreGiveFromISR(cont->I2SClocklessLedDriver_sem, &HPTaskAwoken);
      if (HPTaskAwoken == pdTRUE) portYIELD_FROM_ISR();
    }
  }
  REG_WRITE(I2S_INT_CLR_REG(0), (REG_READ(I2S_INT_RAW_REG(0)) & 0xffffffc0) | 0x3f);
  #endif
}
#endif
static void IRAM_ATTR transpose16x1_noinline2(unsigned char* A, uint16_t* B) {
  uint32_t x, y, x1, y1, t;

  y = *(unsigned int*)(A);

// IDF5.5: transpose16x1_noinline2: use NUM_STRIPS global variable, else #define NUMSTRIPS
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  // use NUM_STRIPS global variable

  if (NUM_STRIPS > 4) {
    x = *(unsigned int*)(A + 4);

    // pre-transform x
    t = (x ^ (x >> 7)) & AAA;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & CC;
    x = x ^ t ^ (t << 14);
  } else
    x = 0;

  if (NUM_STRIPS > 8)
    y1 = *(unsigned int*)(A + 8);
  else
    y1 = 0;

  if (NUM_STRIPS > 12) {
    // pre-transform x
    x1 = *(unsigned int*)(A + 12);
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

  if (NUM_STRIPS > 8) {
    t = (y1 ^ (y1 >> 7)) & AAA;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & CC;
    y1 = y1 ^ t ^ (t << 14);
  }
#else
    // use #define NUMSTRIPS

  #if NUMSTRIPS > 4
  x = *(unsigned int*)(A + 4);
  #else
  x = 0;
  #endif

  #if NUMSTRIPS > 8
  y1 = *(unsigned int*)(A + 8);
  #else
  y1 = 0;
  #endif
  #if NUMSTRIPS > 12
  x1 = *(unsigned int*)(A + 12);
  #else
  x1 = 0;
  #endif

    // pre-transform x
  #if NUMSTRIPS > 4
  t = (x ^ (x >> 7)) & AAA;
  x = x ^ t ^ (t << 7);
  t = (x ^ (x >> 14)) & CC;
  x = x ^ t ^ (t << 14);
  #endif
  #if NUMSTRIPS > 12
  t = (x1 ^ (x1 >> 7)) & AAA;
  x1 = x1 ^ t ^ (t << 7);
  t = (x1 ^ (x1 >> 14)) & CC;
  x1 = x1 ^ t ^ (t << 14);
  #endif
  // pre-transform y
  t = (y ^ (y >> 7)) & AAA;
  y = y ^ t ^ (t << 7);
  t = (y ^ (y >> 14)) & CC;
  y = y ^ t ^ (t << 14);
  #if NUMSTRIPS > 8
  t = (y1 ^ (y1 >> 7)) & AAA;
  y1 = y1 ^ t ^ (t << 7);
  t = (y1 ^ (y1 >> 14)) & CC;
  y1 = y1 ^ t ^ (t << 14);
  #endif
#endif

  // final transform
  t = (x & FF) | ((y >> 4) & FF2);
  y = ((x << 4) & FF) | (y & FF2);
  x = t;

  t = (x1 & FF) | ((y1 >> 4) & FF2);
  y1 = ((x1 << 4) & FF) | (y1 & FF2);
  x1 = t;

#ifdef CONFIG_IDF_TARGET_ESP32S3
  *((uint16_t*)(B + 1)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
  *((uint16_t*)(B + 4)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
  *((uint16_t*)(B + 7)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
  *((uint16_t*)(B + 10)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
  *((uint16_t*)(B + 13)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
  *((uint16_t*)(B + 16)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
  *((uint16_t*)(B + 19)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
  *((uint16_t*)(B + 22)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));

#elif CONFIG_IDF_TARGET_ESP32

  *((uint16_t*)(B)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
  *((uint16_t*)(B + 5)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
  *((uint16_t*)(B + 6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
  *((uint16_t*)(B + 11)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
  *((uint16_t*)(B + 12)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
  *((uint16_t*)(B + 17)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
  *((uint16_t*)(B + 18)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
  *((uint16_t*)(B + 23)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif
}

static void IRAM_ATTR loadAndTranspose(I2SClocklessLedDriver* driver)  // uint8_t *ledt, uint16_t *sizes, uint8_t num_stripst, uint16_t *buffer, int ledtodisp, uint8_t *mapg, uint8_t *mapr, uint8_t
                                                                       // *mapb, uint8_t *mapw, int nbcomponents, int pg, int pr, int pb)
{
  // cont->leds, cont->stripSize, cont->num_strips, (uint16_t *)cont->DMABuffersTampon[cont->dmaBufferActive]->buffer, cont->ledToDisplay, cont->__green_map, cont->__red_map, cont->__blue_map,
  // cont->__white_map, cont->nb_components, cont->p_g, cont->p_r, cont->p_b);
  int nbcomponents = driver->nb_components;
  Lines secondPixel[nbcomponents];
  uint16_t* buffer;
#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32
  if (driver->transpose)
    buffer = (uint16_t*)driver->DMABuffersTampon[driver->dmaBufferActive]->buffer;
  else
    buffer = (uint16_t*)driver->DMABuffersTransposed[driver->dmaBufferActive]->buffer;
#endif

  // uint16_t led_tmp = driver->ledToDisplay;
#ifdef __HARDWARE_MAP
    // led_tmp=driver->ledToDisplay*driver->num_strips;
#endif
  memset(secondPixel, 0, sizeof(secondPixel));
#ifdef _LEDMAPPING
  // #ifdef __SOFTWARE_MAP
  uint8_t* poli;
    // #endif
#else
  uint8_t* poli = driver->leds + driver->ledToDisplay * nbcomponents;
#endif
  for (int i = 0; i < driver->num_strips; i++) {
    if (driver->ledToDisplay < driver->stripSize[i]) {
#ifdef _LEDMAPPING
  #ifdef __SOFTWARE_MAP
      poli = driver->leds + driver->mapLed(led_tmp) * nbcomponents;
  #endif
  #ifdef __HARDWARE_MAP
      poli = driver->leds + *(driver->_hmapoff);
  #endif
  #ifdef __HARDWARE_MAP_PROGMEM
      poli = driver->leds + pgm_read_word_near(driver->_hmap + driver->_hmapoff);
  #endif
#endif
      secondPixel[driver->p_g].bytes[i] = driver->__green_map[*(poli + 1)];
      secondPixel[driver->p_r].bytes[i] = driver->__red_map[*(poli + 0)];
      secondPixel[driver->p_b].bytes[i] = driver->__blue_map[*(poli + 2)];
      if (p_w != UINT8_MAX) secondPixel[driver->p_w].bytes[i] = driver->__white_map[*(poli + 3)];
#ifdef __HARDWARE_MAP
      driver->_hmapoff++;
#endif
#ifdef __HARDWARE_MAP_PROGMEM
      driver->_hmapoff++;
#endif
    }
#ifdef _LEDMAPPING
  #ifdef __SOFTWARE_MAP
    led_tmp += driver->stripSize[i];
  #endif
#else
    poli += driver->stripSize[i] * nbcomponents;
#endif
  }

  transpose16x1_noinline2(secondPixel[0].bytes, (uint16_t*)buffer);
  transpose16x1_noinline2(secondPixel[1].bytes, (uint16_t*)buffer + 3 * 8);
  transpose16x1_noinline2(secondPixel[2].bytes, (uint16_t*)buffer + 2 * 3 * 8);
  if (p_w != UINT8_MAX) transpose16x1_noinline2(secondPixel[3].bytes, (uint16_t*)buffer + 3 * 3 * 8);
}

#endif
