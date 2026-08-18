#ifndef CAT_FRAMES_H
#define CAT_FRAMES_H

#include <stdint.h>

#define CAT_FRAME_WIDTH   64U
#define CAT_FRAME_HEIGHT  64U
#define CAT_FRAME_SIZE    (CAT_FRAME_WIDTH * CAT_FRAME_HEIGHT / 8U)
#define CAT_FRAME_COUNT   30U

extern const uint8_t cat_frames[CAT_FRAME_COUNT][CAT_FRAME_SIZE];

#endif
