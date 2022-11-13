#ifndef _CAMERA_H
#define _CAMERA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------
// Definitions, type & enum declaration

// In cse if unlimited FPS is not an option you need
// stable and reliable framerate, enable this feature
// #define ENABLE_CAMERA_USE_FPS_LIMIT

// Maximum FPS cap
#define CAMERA_USE_FPS_LIMIT_FPS (17)

#ifdef ENABLE_CAMERA_USE_FPS_LIMIT
// Calculate amount of time what needed ta wait for the requested FPS cap
#define CAMERA_USE_FPS_LIMIT_TIME ((1000) / CAMERA_USE_FPS_LIMIT_FPS)
#endif

// 16k for QVGA is pretty enougth
#define IMG_JPG_FILE_MAX_SIZE (16 * 1024)

// ----------------------------------------------------------------------
// Accessors functions

/**
 * @brief
 * 
 * @param
 */
void vCameraSetLEDState(uint32_t ulState);

/**
 * @brief
 * 
 */
void vEnableForcedFrameUpdate(void);

/**
 * @brief
 * 
 */
void vResetForcedFrameUpdate(void);

/**
 * @brief
 * 
 */
void vStartNewFrame(void);

// ----------------------------------------------------------------------
// Core functions
/**
 * @brief Do the initial setup of the Camera and create FreeRTOS stuff
 * 
 * @note If PSRAM is used, Camera will be initialised with usage of it.
 */
void init_camera(void);

#ifdef __cplusplus
}
#endif

#endif /* _CAMERA_H */