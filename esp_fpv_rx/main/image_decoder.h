#ifndef _IMAGE_DECODER_H
#define _IMAGE_DECODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tjpg_decoder/tjpgd.h"

//
#include <sdkconfig.h>
//
#include <freertos/FreeRTOS.h>


// ----------------------------------------------------------------------
// Definitions, type & enum declaration

// TODO: BD-0002 replace magic number for IMG chunk size.
#define IMG_CHUCK_BITMAP_BUFF_SIZE (256)
typedef struct
{
	uint16_t usPosX;
	uint16_t usPosY;
	uint16_t usPixels;
	uint16_t usW;
	uint16_t usH;
	uint16_t usBitmapBuf[IMG_CHUCK_BITMAP_BUFF_SIZE];
} JpgMagicChunk_t;


// This values for 240x240(280)
// Note, Y=20 offset only applied for ST7789v2
#define IMG_CHUNK_POS_X_OFS (20)
#define IMG_CHUNK_POS_Y_OFS (0)

// Amount of small decoded blocks of image provided from jd_output decoder
// Be careful, each chunk use sizeof JpgMagicChunk_t
#define IMG_CHUNKS_NUM  (32)
#define IMG_CHUNKS_MASK (IMG_CHUNKS_NUM - 1)

// Do not change this, it is the minimum size in bytes of the workspace needed by the decoder
#define IMAGE_MEMORY_UNPACK_POOL_SIZE (TJPGD_WORKSPACE_SIZE)

// If it doesn't matter if single decoded chunk will be lost
// Do not stop Decoder!
// #define DECODER_QUEUE_NO_SKIPS


// ----------------------------------------------------------------------
// Variables
// FIXME: BD-0022 Remove ANY global variable outside of module!

extern uint32_t ulAvgFrameTime;
extern uint32_t ulAvgFPS;

// ----------------------------------------------------------------------
// Accessors functions

JpgMagicChunk_t *pxImageDecoderGetMagicChunk(void);

// uint32_t ulImageDecoderGetChunk(void);
BaseType_t xImageDecoderChunksAvailable(void);

void vImageProcessorStartDecode(void);

// ----------------------------------------------------------------------
// Core functions

void init_image_decoder(void);


#ifdef __cplusplus
}
#endif

#endif /* _IMAGE_DECODER_H */