/**
 * @file image_decoder.c
 * 
 * Decode received Jpg image grabbed from @ref wireless_stuff
 */

#include "image_decoder.h"

#include "data_common.h"
#include "debug_tools_conf.h"
#include "display_osd.h"
#include "image_decoder.h"
#include "memory_model/memory_model.h"
#include "wireless/wireless_main.h"

//
#include <sdkconfig.h>
//
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/timers.h>
//
#include <esp_attr.h>
#include <esp_timer.h>
//
#include <assert.h>
#include <stdint.h>
#include <string.h>


// ----------------------------------------------------------------------
// Definitions, type & enum declaration



// ----------------------------------------------------------------------
// FreeRTOS Variables

#define STACK_WORDS_SIZE_FOR_TASK_IMG_DECODER (2048)
#define PRIORITY_LEVEL_FOR_TASK_IMG_DECODER   (2)
#define PINNED_CORE_FOR_TASK_IMG_DECODER      (0)
const char* assigned_name_for_task_img_decoder = "img_decoder";
TaskHandle_t xImgDecoderTaskHandler = NULL;
StaticTask_t xImgDecoderTaskControlBlock;
StackType_t xImgDecoderStack[STACK_WORDS_SIZE_FOR_TASK_IMG_DECODER];

// From Decoder to Printer task
#define IMG_CHUNKS_QUEUE_SIZE (IMG_CHUNKS_NUM)
QueueHandle_t xImgChunksQueueHandler = NULL;
StaticQueue_t xImgChunksQueueControlBlock;
uint32_t xImgChunksQueueStorage[IMG_CHUNKS_QUEUE_SIZE];

// in ms or each 1sec.
#define FRAME_COUNTER_TIMEOUT (1000)
TimerHandle_t xFrameCounterTimer = NULL;
StaticTimer_t xFrameCounterTimerControlBlock;


// ----------------------------------------------------------------------
// Variables

uint32_t ulAvgFrameTime = 0UL;
uint32_t ulAvgFPS = 0UL;

uint32_t ulFramesCount = 0;
uint32_t ulFrameTimeCount = 0;

uint8_t ucImageMemoryPool[IMAGE_MEMORY_UNPACK_POOL_SIZE] __attribute__((aligned(4)));

uint32_t ulInputImageDataOffset = 0UL;
uint8_t* pucInputImageDataPtr = NULL;

uint32_t ulImageChunkOffset = 0;
JpgMagicChunk_t xJpgMagicChunks[IMG_CHUNKS_NUM];


// ----------------------------------------------------------------------
// Static functions declaration

static uint32_t jd_input(JDEC* jdec, uint8_t* buf, uint32_t len);

static uint32_t jd_output(JDEC* jdec, void* bitmap, JRECT* jrect);

static void process_received_image(void);

static void init_image_decoder_rtos(void);

static void vFrameCounterTimer(void);

static void vImageProcessorTask(void* pvArg);

// ----------------------------------------------------------------------
// Static functions

static uint32_t IRAM_ATTR
jd_input(JDEC* jdec, uint8_t* buf, uint32_t len)
{
	if((ulInputImageDataOffset + len) > IMG_JPG_FILE_MAX_SIZE)
	{
		len = IMG_JPG_FILE_MAX_SIZE - ulInputImageDataOffset;
	}

	if(buf)
	{
		memcpy(buf, &pucInputImageDataPtr[ulInputImageDataOffset], len);
	}
	ulInputImageDataOffset += len;

	return len;
}


static uint32_t IRAM_ATTR
jd_output(JDEC* jdec, void* bitmap, JRECT* jrect)
{
#ifdef JD_CHUNK_DECODE_TIME_DBG_PROFILER
	profile_point(profile_point_end, JD_CHUNK_DECODE_TIME_DBG_PROFILER_POINT_ID);
#endif
#ifdef JD_OUTPUT_DBG_PROFILER
	profile_point(profile_point_start, JD_OUTPUT_DBG_PROFILER_POINT_ID);
#endif

	JpgMagicChunk_t* pxJpgMagicChunk = NULL;

#ifndef DECODER_QUEUE_NO_SKIPS
	// if(uxQueueSpacesAvailable(xImgChunksQueueHandler))
#endif
	{
		pxJpgMagicChunk = &xJpgMagicChunks[ulImageChunkOffset];
		pxJpgMagicChunk->usPosX = jrect->left + IMG_FRAME_OFFSET_X;
		pxJpgMagicChunk->usPosY = jrect->top + IMG_FRAME_OFFSET_Y;
		pxJpgMagicChunk->usW = jrect->right + 1 - jrect->left;
		pxJpgMagicChunk->usH = jrect->bottom + 1 - jrect->top;
		pxJpgMagicChunk->usPixels = (pxJpgMagicChunk->usW * pxJpgMagicChunk->usH);

		if(pxJpgMagicChunk->usPixels <= IMG_CHUCK_BITMAP_BUFF_SIZE)
		{
			memcpy(&pxJpgMagicChunk->usBitmapBuf[0], bitmap, pxJpgMagicChunk->usPixels * sizeof(uint16_t));

#ifdef DECODER_QUEUE_NO_SKIPS
			xQueueSend(xImgChunksQueueHandler, &ulImageChunkOffset, portMAX_DELAY);
#else
			xQueueSend(xImgChunksQueueHandler, &ulImageChunkOffset, 0);
#endif

			// vImgChunkStartDraw();
			ulImageChunkOffset = (ulImageChunkOffset + 1) & IMG_CHUNKS_MASK;
		}
	}

#ifdef JD_OUTPUT_DBG_PROFILER
	profile_point(profile_point_end, JD_OUTPUT_DBG_PROFILER_POINT_ID);
#endif
#ifdef JD_CHUNK_DECODE_TIME_DBG_PROFILER
	profile_point(profile_point_start, JD_CHUNK_DECODE_TIME_DBG_PROFILER_POINT_ID);
#endif

	return 1;
}


static void IRAM_ATTR
process_received_image(void)
{
	JRESULT jresult = JDR_OK;
	JDEC jdec;

	ulInputImageDataOffset = 0UL;

#ifdef JD_DECODE_DBG_PROFILER
	profile_point(profile_point_start, JD_DECODE_DBG_PROFILER_POINT_ID);
#endif

	// Analyze input data
	jresult = jd_prepare(&jdec, jd_input, &ucImageMemoryPool[0], IMAGE_MEMORY_UNPACK_POOL_SIZE, 0);

	if(JDR_OK == jresult)
	{
		jd_decomp(&jdec, jd_output);
	}

#ifdef JD_DECODE_DBG_PROFILER
	profile_point(profile_point_end, JD_DECODE_DBG_PROFILER_POINT_ID);
#endif
}


static void
init_image_decoder_rtos(void)
{
	xFrameCounterTimer = xTimerCreateStatic("xFrameCounterTimer",
	                                        pdMS_TO_TICKS(FRAME_COUNTER_TIMEOUT),
	                                        pdTRUE,
	                                        NULL,
	                                        (TimerCallbackFunction_t)(vFrameCounterTimer),
	                                        &xFrameCounterTimerControlBlock);
	assert(xFrameCounterTimer);


	xImgChunksQueueHandler = xQueueCreateStatic(
	    IMG_CHUNKS_QUEUE_SIZE, sizeof(uint32_t), (uint8_t*)(&xImgChunksQueueStorage[0]), &xImgChunksQueueControlBlock);
	assert(xImgChunksQueueHandler);


	xImgDecoderTaskHandler = xTaskCreateStaticPinnedToCore((TaskFunction_t)(vImageProcessorTask),
	                                                       assigned_name_for_task_img_decoder,
	                                                       STACK_WORDS_SIZE_FOR_TASK_IMG_DECODER,
	                                                       NULL,
	                                                       PRIORITY_LEVEL_FOR_TASK_IMG_DECODER,
	                                                       xImgDecoderStack,
	                                                       &xImgDecoderTaskControlBlock,
	                                                       (BaseType_t)PINNED_CORE_FOR_TASK_IMG_DECODER);
	assert(xImgDecoderTaskHandler);
}

// ----------------------------------------------------------------------
// Accessors functions

JpgMagicChunk_t* IRAM_ATTR
pxImageDecoderGetMagicChunk(void)
{
	uint32_t ulChunkOffset = 0;
	xQueueReceive(xImgChunksQueueHandler, &ulChunkOffset, portMAX_DELAY);
	return &xJpgMagicChunks[ulChunkOffset];
}


BaseType_t IRAM_ATTR
xImageDecoderChunksAvailable(void)
{
	return (uxQueueSpacesAvailable(xImgChunksQueueHandler) == IMG_CHUNKS_QUEUE_SIZE) ? pdFALSE : pdTRUE;
}

void
vImageProcessorStartDecode(void)
{
	xTaskNotifyGive(xImgDecoderTaskHandler);
}


// ----------------------------------------------------------------------
// FreeRTOS functions

static void
vFrameCounterTimer(void)
{
	ulAvgFPS = ulFramesCount;
	ulAvgFrameTime = (ulAvgFPS) ? (ulFrameTimeCount / ulAvgFPS) : 0;

	ulFrameTimeCount = 0;
	ulFramesCount = 0;
}

static void
vImageProcessorTask(void* pvArg)
{
	(void)pvArg;
	int64_t fr_start = 0, fr_end = 0;

	task_sync_get_bits(TASK_SYNC_EVENT_BIT_IMG_PROCESS);

	xTimerStart(xFrameCounterTimer, 0UL);

#ifdef TASK_START_EVENT_DBG_PRINTOUT
	async_printf(async_print_type_str, assigned_name_for_task_img_decoder, 0);
#endif

	for(;;)
	{
		// Wait until new image is received.
		if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY))
		{
			pucInputImageDataPtr = pucWirelessTakeCurrentRxBuffer();
			// Tell to Transmitter: "JPG is accepted, now send the next frame"
			xWirelessSendEvent(W_MSG_EVENT_FRAME_RECEIVED);

			fr_start = esp_timer_get_time();
			process_received_image();
			fr_end = esp_timer_get_time();

			// Accumulate FPS and Frame time
			ulFrameTimeCount += (uint32_t)((fr_end - fr_start) / 1000);
			++ulFramesCount;

#ifdef IMAGE_DECODE_TIME_DBG_PRINTOUT
			// How much time did decoding take
			async_printf(async_print_type_u32, "ulFrameTimeCount: %ums\n", ulFrameTimeCount);
#endif
		}
	}
}


// ----------------------------------------------------------------------
// Core functions

void
init_image_decoder(void)
{
	init_image_decoder_rtos();
}