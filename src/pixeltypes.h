
#ifdef USE_FASTLED
    #include "FastLED.h"
#endif
#ifdef RGBW

struct Pixel {
    union {
        uint8_t raw[3];
        struct 
        {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
            
        };
        
    };

    inline Pixel(uint8_t r, uint8_t g,uint8_t b) __attribute__((always_inline))
    :red(r),green(g),blue(b)
{
    //brigthness =0xE0 |(br&31);
}

	inline Pixel() __attribute__((always_inline))
    {

    }

       
#ifdef USE_FASTLED
inline Pixel &operator= (const CRGB& rhs) __attribute__((always_inline))
    {
        red = rhs.r;
        green = rhs.g;
        blue = rhs.b;
        return *this;
    }
   #endif

inline Pixel (const Pixel& rhs) __attribute__((always_inline))
     {
         //brigthness=rhs.brigthness;
         red=rhs.red;
         green=rhs.green;
         blue=rhs.blue;
     }
     inline Pixel& operator= (const uint32_t colorcode) __attribute__((always_inline))
    {
       // rgb colorg; 
        red = (colorcode >> 16) & 0xFF;
        green = (colorcode >>  8) & 0xFF;
        blue = (colorcode >>  0) & 0xFF;
        return *this;
    }
        

};
#else

struct Pixel {
    union {
        uint8_t raw[3];
        struct 
        {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
            
        };
        
    };

    inline Pixel(uint8_t r, uint8_t g,uint8_t b) __attribute__((always_inline))
    :red(r),green(g),blue(b)
{
    //brigthness =0xE0 |(br&31);
}

	inline Pixel() __attribute__((always_inline))
    {

    }

       
#ifdef USE_FASTLED
inline Pixel &operator= (const CRGB& rhs) __attribute__((always_inline))
    {
        red = rhs.r;
        green = rhs.g;
        blue = rhs.b;
        return *this;
    }
   #endif

inline Pixel (const Pixel& rhs) __attribute__((always_inline))
     {
         //brigthness=rhs.brigthness;
         red=rhs.red;
         green=rhs.green;
         blue=rhs.blue;
     }
     inline Pixel& operator= (const uint32_t colorcode) __attribute__((always_inline))
    {
       // rgb colorg; 
        red = (colorcode >> 16) & 0xFF;
        green = (colorcode >>  8) & 0xFF;
        blue = (colorcode >>  0) & 0xFF;
        return *this;
    }
        

};
#endif