#pragma once
// #include "_pixelslib.h"
#define _NB_FRAME 2

/**
 * frameBuffer — a simple double-buffer wrapper over a Pixel array.
 *
 * operator[] writes into the current writing frame.
 * getFrametoDisplay() returns a pointer to the writing frame and advances
 * the write index, so the caller can hand the returned pointer to showPixels().
 * Check valid() before use — construction fails silently if calloc returns NULL.
 */
class frameBuffer {
 public:
  Pixel* frames[_NB_FRAME];
  uint8_t displayframe;
  uint8_t writingframe;
  frameBuffer(int num_led) {
    writingframe = 0;
    displayframe = 0;
    for (int i = 0; i < _NB_FRAME; i++) {
      frames[i] = nullptr;
    }
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

  ~frameBuffer() {
    for (int i = 0; i < _NB_FRAME; i++) {
      free(frames[i]);
      frames[i] = nullptr;
    }
  }

  // Non-copyable, non-movable due to raw pointer ownership
  frameBuffer(const frameBuffer&) = delete;
  frameBuffer& operator=(const frameBuffer&) = delete;
  frameBuffer(frameBuffer&&) = delete;
  frameBuffer& operator=(frameBuffer&&) = delete;

  bool valid() { return frames[writingframe] != nullptr; }
  Pixel& operator[](int i) {
    if (!frames[writingframe]) return _offPixel;
    return *(frames[writingframe] + i);
  }
  uint8_t* getFrametoDisplay() {
    if (!frames[writingframe]) return nullptr;
    uint8_t* tmp = (uint8_t*)frames[writingframe];
    switchFrame();
    return tmp;
  }
  void switchFrame() {
    writingframe = (writingframe + 1) % _NB_FRAME;
    // displayframe=
  }

 private:
  Pixel _offPixel{};
};
