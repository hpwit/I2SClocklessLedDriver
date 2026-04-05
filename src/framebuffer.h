#pragma once
// #include "_pixelslib.h"
#define NB_FRAME 2

/**
 * FrameBuffer — a simple double-buffer wrapper over a Pixel array.
 *
 * operator[] writes into the current writing frame.
 * getFrametoDisplay() returns a pointer to the writing frame and advances
 * the write index, so the caller can hand the returned pointer to showPixels().
 * Check valid() before use — construction fails silently if calloc returns NULL.
 */
class FrameBuffer {
 public:
  Pixel* frames[NB_FRAME];
  uint8_t displayframe;
  uint8_t writingframe;
  FrameBuffer(int numLed) {
    writingframe = 0;
    displayframe = 0;
    for (int i = 0; i < NB_FRAME; i++) {
      frames[i] = nullptr;
    }
    /*
     * we create the frames
     * to add the logic if the memory is not enough
     */
    for (int i = 0; i < NB_FRAME; i++) {
      frames[i] = (Pixel*)calloc(numLed, sizeof(Pixel));
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

  ~FrameBuffer() {
    for (int i = 0; i < NB_FRAME; i++) {
      free(frames[i]);
      frames[i] = nullptr;
    }
  }

  // Non-copyable, non-movable due to raw pointer ownership
  FrameBuffer(const FrameBuffer&) = delete;
  FrameBuffer& operator=(const FrameBuffer&) = delete;
  FrameBuffer(FrameBuffer&&) = delete;
  FrameBuffer& operator=(FrameBuffer&&) = delete;

  bool valid() { return frames[writingframe] != nullptr; }
  Pixel& operator[](int i) {
    if (!frames[writingframe]) return offPixel;
    return *(frames[writingframe] + i);
  }
  uint8_t* getFrametoDisplay() {
    if (!frames[writingframe]) return nullptr;
    uint8_t* tmp = reinterpret_cast<uint8_t*>(frames[writingframe]);
    switchFrame();
    return tmp;
  }
  void switchFrame() {
    writingframe = (writingframe + 1) % NB_FRAME;
    // displayframe=
  }

 private:
  Pixel offPixel{};
};
