#pragma once
// #include "_pixelslib.h"
#define _NB_FRAME 2

class frameBuffer {
 public:
  Pixel* frames[_NB_FRAME];
  uint8_t displayframe;
  uint8_t writingframe;
  frameBuffer(int num_led) {
    writingframe = 0;
    displayframe = 0;
    /*
     * we create the frames
     * to add the logic if the memory is not enough
     */
    for (int i = 0; i < _NB_FRAME; i++) {
      frames[i] = (Pixel*)calloc(num_led, sizeof(Pixel));
      if (!frames[i]) {
        printf("no memory\n");
        // Consider freeing previously allocated frames here
        for (int j = 0; j < i; j++) {
          free(frames[j]);
          frames[j] = nullptr;
        }
        return;
      }
    }
  }

  Pixel& operator[](int i) { return *(frames[writingframe] + i); }
  uint8_t* getFrametoDisplay() {
    uint8_t* tmp = (uint8_t*)frames[writingframe];
    switchFrame();
    return tmp;
  }
  void switchFrame() {
    writingframe = (writingframe + 1) % _NB_FRAME;
    // displayframe=
  }
};
