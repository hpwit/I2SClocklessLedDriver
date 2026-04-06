
#ifdef USE_PIXELSLIB
#ifdef USE_FASTLED
  #include "FastLED.h"
#endif

#ifdef ARDUINO
#include "Arduino.h"
#else
#include "stdint.h"
#endif

#define OUT_OF_BOUND -12

#ifdef COLOR_RGBW

/** Single RGBW LED pixel. raw[] gives direct byte access; named fields (red/green/blue/white) give semantic access. */
struct Pixel {
  union {
    uint8_t raw[4];
    struct {
      uint8_t red;
      uint8_t green;
      uint8_t blue;
      uint8_t white;
    };
  };

  inline Pixel(uint8_t r, uint8_t g, uint8_t b, uint8_t w) __attribute__((always_inline)) : red(r), green(g), blue(b), white(w) {
    // brigthness =0xE0 |(br&31);
  }

  inline Pixel(uint8_t r, uint8_t g, uint8_t b) __attribute__((always_inline)) : red(r), green(g), blue(b) {
    white = MIN(red, green);
    white = MIN(white, blue);
    red = red - white;
    green = green - white;
    blue = blue - white;
  }

  inline Pixel() __attribute__((always_inline)) : red(0), green(0), blue(0), white(0) {}

  #ifdef USE_FASTLED
  inline Pixel& operator=(const CRGB& rhs) __attribute__((always_inline)) {
    red = rhs.r;
    green = rhs.g;
    blue = rhs.b;
    white = MIN(red, green);
    white = MIN(white, blue);
    red = red - white;
    green = green - white;
    blue = blue - white;
    return *this;
  }
  #endif

  inline Pixel(const Pixel& rhs) __attribute__((always_inline)) {
    // brigthness=rhs.brigthness;
    red = rhs.red;
    green = rhs.green;
    blue = rhs.blue;
    white = rhs.white;
  }
  inline Pixel& operator=(const uint32_t colorcode) __attribute__((always_inline)) {
    // rgb colorg;
    red = (colorcode >> 24) & 0xFF;
    green = (colorcode >> 16) & 0xFF;
    blue = (colorcode >> 8) & 0xFF;
    white = colorcode & 0xFF;
    return *this;
  }
};
#else

/** Single RGB LED pixel. raw[] gives direct byte access; named fields give semantic access. */
struct Pixel {
  union {
    uint8_t raw[3];
    struct {
      uint8_t red;
      uint8_t green;
      uint8_t blue;
    };
  };

  inline Pixel(uint8_t r, uint8_t g, uint8_t b) __attribute__((always_inline)) : red(r), green(g), blue(b) {
    // brigthness =0xE0 |(br&31);
  }

  inline Pixel() __attribute__((always_inline)) : red(0), green(0), blue(0) {}

  #ifdef USE_FASTLED
  inline Pixel& operator=(const CRGB& rhs) __attribute__((always_inline)) {
    red = rhs.r;
    green = rhs.g;
    blue = rhs.b;
    return *this;
  }
  #endif

  inline Pixel(const Pixel& rhs) __attribute__((always_inline)) {
    // brigthness=rhs.brigthness;
    red = rhs.red;
    green = rhs.green;
    blue = rhs.blue;
  }
  inline Pixel& operator=(const uint32_t colorcode) __attribute__((always_inline)) {
    // rgb colorg;
    red = (colorcode >> 16) & 0xFF;
    green = (colorcode >> 8) & 0xFF;
    blue = (colorcode >> 0) & 0xFF;
    return *this;
  }
};
#endif

enum class LedDirection { FORWARD, BACKWARD, MAP };

/**
 * Pixels — a view over a contiguous LED byte buffer, optionally spanning
 * multiple strips.
 *
 * Ownership:
 *   - Constructors that take a uint16_t* sizes array allocate their own buffer
 *     (localLedPointer = true) and free it in the destructor.
 *   - Constructors that take an external Pixel* pointer do NOT own the buffer.
 *   - The copy constructor performs a shallow copy — the copy does not own the
 *     buffer or the arguments block.
 */
class Pixels {
 public:
  inline Pixels() __attribute__((always_inline)) {}
  Pixels& operator=(const Pixels&) = delete;
  inline Pixels(const Pixels& rhs) __attribute__((always_inline)) {
    pixelSize = rhs.pixelSize;
    direction = rhs.direction;
    numStrips = rhs.numStrips;
    for (int i = 0; i < numStrips; i++) {
      sizes[i] = rhs.sizes[i];
    }
    ledpointer = rhs.ledpointer;
    mapFunction = nullptr;  // Don't copy mapping - caller must re-set if needed
    arguments = nullptr;
    // localArguments remains false (default)

    // parent=rhs.parent;
  }
  Pixels(int size, Pixel* ledpoi) { initPixelsImpl(size, ledpoi, LedDirection::FORWARD, this); }

  Pixels(int size, Pixel* ledpoi, LedDirection direction) { initPixelsImpl(size, ledpoi, direction, this); }

  void initPixelsImpl(int size, Pixel* ledpoi, LedDirection direction, Pixels* pib) {
    pib->pixelSize = size;
    pib->ledpointer = ledpoi;
    pib->numStrips = 0;
    pib->direction = direction;
    //  pib->nb_child=0;
  }

  Pixels(uint16_t numLedPerStrip, uint8_t numStrips) {
    if (numStrips > 16) numStrips = 16;
    uint16_t sizes[16];
    for (int i = 0; i < numStrips; i++) {
      sizes[i] = numLedPerStrip;
    }
    initPixelsImpl(sizes, numStrips, LedDirection::FORWARD, this);
  }

  Pixels(uint16_t* sizes, uint8_t numStrips) { initPixelsImpl(sizes, numStrips, LedDirection::FORWARD, this); }

  Pixels(uint16_t* sizes, uint8_t numStrips, LedDirection direction) { initPixelsImpl(sizes, numStrips, direction, this); }
  void initPixelsImpl(uint16_t* sizes, uint8_t numStrips, LedDirection direction, Pixels* pib) {
    if (numStrips > 16) numStrips = 16;  // Clamp to array size
    int size = 0;
    for (int i = 0; i < numStrips; i++) {
      size += sizes[i];
      pib->sizes[i] = sizes[i];
    }

    pib->numStrips = numStrips;

    ledpointer = (Pixel*)calloc(size, sizeof(Pixel));
    localLedPointer = true;
    if (ledpointer == NULL) {
      pib->pixelSize = 0;
      pib->numStrips = 0;
    } else {
      pib->pixelSize = size;
    }
    pib->direction = direction;
  }

  ~Pixels() {
    if (localArguments && arguments != nullptr) {
      free(arguments);
      arguments = nullptr;
    }
    if (localLedPointer && ledpointer) {
      free(ledpointer);
      ledpointer = nullptr;
    }
  }

  Pixel& operator[](int i) {
    if (pixelSize == 0 || ledpointer == nullptr) return offPixel;
    switch (direction) {
    case (LedDirection::FORWARD):

      return *(ledpointer + i % pixelSize);
      break;

    case (LedDirection::BACKWARD):

      return *(ledpointer + (pixelSize - i % (pixelSize)-1));
      break;

    case (LedDirection::MAP):
      if (mapFunction) {
        int offset = mapFunction(i, arguments);
        // printf("%d %d\n",i,offset);
        if (offset == OUT_OF_BOUND) {
          return offPixel;
        } else
          return *(ledpointer + (offset % pixelSize));
      }

      else
        return *(ledpointer);
      break;
    default:
      return *(ledpointer);
      break;
    }
  }

  void copy(Pixels& ori) { copy(ori, LedDirection::FORWARD); }

  void copy(Pixels& ori, LedDirection dir) {
    LedDirection ledd = direction;
    if (direction == LedDirection::MAP) ledd = LedDirection::FORWARD;
    for (int i = 0; i < ori.pixelSize; i++) {
      if (ledd == dir) {
        (*this)[i] = ori[i];
      } else {
        (*this)[i] = ori[ori.pixelSize - i % (ori.pixelSize) - 1];
      }
    }
  }

  Pixels getStrip(uint8_t numStrip, LedDirection direction = LedDirection::FORWARD) {
    if (numStrips == 0 || numStrip >= numStrips) {
      // Return empty Pixels object
      Pixels empty;
      empty.pixelSize = 0;
      empty.numStrips = 0;
      empty.direction = direction;
      empty.ledpointer = nullptr;
      return empty;
    } else {
      uint32_t off = 0;
      for (int i = 0; i < numStrip; i++) {
        off += sizes[i];
      }

      return Pixels(sizes[numStrip], ledpointer + off, direction);
    }
  }

  uint16_t* getLengths() { return sizes; }

  uint8_t getNumStrip() { return numStrips; }
  uint8_t* getPixels() { return reinterpret_cast<uint8_t*>(ledpointer); }
  void clear() { memset(ledpointer, 0, pixelSize * sizeof(Pixel)); }  // NOLINT(bugprone-undefined-memory-manipulation)

  Pixels createSubset(int start, int length) { return createSubset(start, length, LedDirection::FORWARD); }

  Pixels createSubset(int start, LedDirection direction) {
    if (ledpointer == nullptr || pixelSize <= 0) return Pixels{};
    if (start < 0) start = 0;
    if (start > pixelSize) start = pixelSize;
    return Pixels(pixelSize - start, ledpointer + start, direction);
  }

  Pixels createSubset(int start, int length, LedDirection direction) {
    if (ledpointer == nullptr || pixelSize <= 0) return Pixels{};
    if (start < 0) start = 0;
    if (start > pixelSize) start = pixelSize;
    int remaining = pixelSize - start;
    if (remaining == 0) return Pixels(0, ledpointer + start, direction);
    if (length <= 0) length = remaining;  // Default to all remaining     
    if (length > remaining) length = remaining;

    return Pixels(length, ledpointer + start, direction);
  }
  /*
      Pixels getParent()
      {
          return *parent;
      }

      Pixels * getChild(int i)
      {

          return children[i%nb_child];
      }
      */
  inline void setMapFunction(int (*fptr)(int i, void* args), void* args, int size) {
    mapFunction = fptr;
    if (localArguments && arguments != NULL) free(arguments);
    arguments = (void*)malloc(size);
    if (arguments == NULL) {
      mapFunction = nullptr;  // Can't use mapping without arguments
      localArguments = false;
      return;
    }
    memcpy(arguments, args, size);
    localArguments = true;
  }

 private:
  bool localLedPointer = false;
  bool localArguments = false;
  Pixel* ledpointer = nullptr;
  int pixelSize = 0;
  uint16_t sizes[16];
  uint8_t numStrips = 0;
  LedDirection direction = LedDirection::FORWARD;
  // int nb_child;
  //  Pixels *parent;
  void* arguments = nullptr;
  // Pixels **children;
  int (*mapFunction)(int i, void* args) = nullptr;
  /*
   * this is the pixel to retuen when out of bound
   */
  Pixel offPixel;
};
#endif