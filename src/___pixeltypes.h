
#ifdef USE_FASTLED
  #include "FastLED.h"
#endif

#ifdef ARDUINO
#include "Arduino.h"
#else
#include "stdint.h"
#endif

#define _OUT_OF_BOUND -12

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

enum class leddirection { FORWARD, BACKWARD, MAP };

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
  inline Pixels(const Pixels& rhs) __attribute__((always_inline)) {
    _size = rhs._size;
    _direction = rhs._direction;
    _num_strips = rhs._num_strips;
    for (int i = 0; i < _num_strips; i++) {
      _sizes[i] = rhs._sizes[i];
    }
    ledpointer = rhs.ledpointer;
    mapFunction = nullptr;  // Don't copy mapping - caller must re-set if needed
    arguments = nullptr;
    // localArguments remains false (default)

    // parent=rhs.parent;
  }
  Pixels(int size, Pixel* ledpoi) { __Pixels(size, ledpoi, leddirection::FORWARD, this); }

  Pixels(int size, Pixel* ledpoi, leddirection direction) { __Pixels(size, ledpoi, direction, this); }

  void __Pixels(int size, Pixel* ledpoi, leddirection direction, Pixels* pib) {
    pib->_size = size;
    pib->ledpointer = ledpoi;
    pib->_num_strips = 0;
    pib->_direction = direction;
    //  pib->nb_child=0;
  }

  Pixels(uint16_t num_led_per_strip, uint8_t num_strips) {
    if (num_strips > 16) num_strips = 16;
    uint16_t sizes[16];
    for (int i = 0; i < num_strips; i++) {
      sizes[i] = num_led_per_strip;
    }
    __Pixels(sizes, num_strips, leddirection::FORWARD, this);
  }

  Pixels(uint16_t* sizes, uint8_t num_strips) { __Pixels(sizes, num_strips, leddirection::FORWARD, this); }

  Pixels(uint16_t* sizes, uint8_t num_strips, leddirection direction) { __Pixels(sizes, num_strips, direction, this); }
  void __Pixels(uint16_t* sizes, uint8_t num_strips, leddirection direction, Pixels* pib) {
    if (num_strips > 16) num_strips = 16;  // Clamp to array size
    int size = 0;
    for (int i = 0; i < num_strips; i++) {
      size += sizes[i];
      pib->_sizes[i] = sizes[i];
    }

    pib->_num_strips = num_strips;

    ledpointer = (Pixel*)calloc(size, sizeof(Pixel));
    localLedPointer = true;
    if (ledpointer == NULL) {
      pib->_size = 0;
    } else {
      pib->_size = size;
    }
    pib->_direction = direction;
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
    if (_size == 0 || ledpointer == nullptr) return offPixel;
    switch (_direction) {
    case (leddirection::FORWARD):

      return *(ledpointer + i % _size);
      break;

    case (leddirection::BACKWARD):

      return *(ledpointer + (_size - i % (_size)-1));
      break;

    case (leddirection::MAP):
      if (mapFunction) {
        int offset = mapFunction(i, arguments);
        // printf("%d %d\n",i,offset);
        if (offset == _OUT_OF_BOUND) {
          return offPixel;
        } else
          return *(ledpointer + (offset % _size));
      }

      else
        return *(ledpointer);
      break;
    default:
      return *(ledpointer);
      break;
    }
  }

  void copy(Pixels ori) { copy(ori, leddirection::FORWARD); }

  void copy(Pixels ori, leddirection dir) {
    leddirection ledd = _direction;
    if (_direction == leddirection::MAP) ledd = leddirection::FORWARD;
    for (int i = 0; i < ori._size; i++) {
      if (ledd == dir) {
        (*this)[i] = ori[i];
      } else {
        (*this)[i] = ori[ori._size - i % (ori._size) - 1];
      }
    }
  }

  Pixels getStrip(uint8_t num_strip, leddirection direction) {
    if (_num_strips == 0 || num_strip >= _num_strips) {
      // Return empty Pixels object
      Pixels empty;
      empty._size = 0;
      empty._num_strips = 0;
      empty._direction = direction;
      empty.ledpointer = nullptr;
      return empty;
    } else {
      uint32_t off = 0;
      for (int i = 0; i < num_strip; i++) {
        off += _sizes[i];
      }

      return Pixels(_sizes[num_strip], ledpointer + off, direction);
    }
  }

  Pixels getStrip(int num_strip) { return getStrip(num_strip, leddirection::FORWARD); }

  uint16_t* getLengths() { return _sizes; }

  uint8_t getNumStrip() { return _num_strips; }
  uint8_t* getPixels() { return (uint8_t*)ledpointer; }
  void clear() { memset(ledpointer, 0, _size * sizeof(Pixel)); }

  Pixels createSubset(int start, int length) { return createSubset(start, length, leddirection::FORWARD); }

  Pixels createSubset(int start, leddirection direction) {
    if (start < 0) start = 0;
    if (start > _size) start = _size;
    return Pixels(_size - start, ledpointer + start, direction);
  }

  Pixels createSubset(int start, int length, leddirection direction) {
    if (start < 0) start = 0;
    if (start > _size) start = _size;
    int remaining = _size - start;
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
  size_t _size = 0;
  uint16_t _sizes[16];
  uint8_t _num_strips = 0;
  leddirection _direction = leddirection::FORWARD;
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