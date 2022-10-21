#include "display_osd.h"

#include "data_common.h"
#include "debug_tools_conf.h"
#include "image_decoder.h"
#include "memory_model/memory_model.h"
#include "pins_definitions.h"
#include "wireless/wireless_main.h"
//
#define LGFX_ESP_WROVER_KIT
#define LGFX_USE_V1
#include "../../libs/LovyanGFX/src/LovyanGFX.hpp"
#include "lgfx_ili9341_s3.hpp"
#include "lgfx_ssd1306_s3.hpp"

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

#define STACK_WORDS_SIZE_FOR_TASK_IMG_CHUNK_DRAW (2048)
#define PRIORITY_LEVEL_FOR_TASK_IMG_CHUNK_DRAW   (1)
#define PINNED_CORE_FOR_TASK_IMG_CHUNK_DRAW      (1)
const char* assigned_name_for_task_img_chunk_draw = "img_chunk_draw";
TaskHandle_t xImgChunkDrawTaskHandler = NULL;
StaticTask_t xImgChunkDrawTaskControlBlock;
StackType_t xImgChunkDrawStack[STACK_WORDS_SIZE_FOR_TASK_IMG_CHUNK_DRAW];


#define STACK_WORDS_SIZE_FOR_TASK_IMG_OSD_DRAW (2048)
#define PRIORITY_LEVEL_FOR_TASK_IMG_OSD_DRAW   (1)
#define PINNED_CORE_FOR_TASK_IMG_OSD_DRAW      (1)
const char* assigned_name_for_task_img_osd_draw = "img_osd_draw";
TaskHandle_t xImgOsdDrawTaskHandler = NULL;
StaticTask_t xImgOsdDrawTaskControlBlock;
StackType_t xImgOsdDrawStack[STACK_WORDS_SIZE_FOR_TASK_IMG_OSD_DRAW];

// in ms or each 500ms
#define OSD_UPDATE_TIMEOUT (500)
TimerHandle_t xOsdUpdateTimer = NULL;
StaticTimer_t xOsdUpdateTimerControlBlock;

// ----------------------------------------------------------------------
// Variables

// Temporal storage for OSD text when converting to string
static char gui_text_buf[128];

// Bunch of flags which reduce CPU cycles usage if there is nothing updated
bool sRedrawDataRate = false;
bool sRedrawChannelNumber = true;
bool sRedrawRTT = false;
bool sRedrawTxPower = true;
bool sRedrawRSSi = false;

// ----------------------------
// Each class for each screen
LGFX_ILI9341_S3 tft;
LGFX_SSD1306_S3 tft_oled;


// ----------------------------------------------------------------------
// Static functions declaration

/**
 * @brief Callback function which is called when any value has been updated
 *        in @ref memory_model
 * 
 * @param xDataId Type of variable what was updated
 */
static void vOsdUpdateCallback(memory_model_types_t xDataId);

/**
 * @brief Unblock @ref ''vImgOsdDrawTask'' and redraw screen with new values (if any)
 */
static void vOsdStartDraw(void);

/**
 * @brief Block OSD update on screen until external signal is provided
 * 
 * @param xTicksToSync how much time to waite before forced redraw of the OSD
 * @retval see @ref ''ulTaskNotifyTake''
 * 
 * @note Call only inside ''vImgOsdDrawTask''
 */
static uint32_t ulOsdSyncDraw(TickType_t xTicksToSync);

/**
 * @brief 
 * 
 * @param xTicksToSync How much time to wait before forced redraw
 * @retval see @ref ''ulTaskNotifyTake''
 * 
 * @note Call only inside ''vImgChunkDrawTask''
 */
// static uint32_t ulImgChunkSyncDraw(TickType_t xTicksToSync);

/**
 * @brief Convert and draw variables
 */
static void draw_osd_screen(void);

/**
 * @brief Draw decoded chunk of from @ref image_decoder
 * 
 * @param pxJpgMagicChunk Pointer to image data.
 */
static void draw_img_chunk(JpgMagicChunk_t* pxJpgMagicChunk);

/**
 * @brief Draw text placeholders
 */
static void draw_gui(void);

/**
 * @brief
 */
static void init_display_tft(void);

/**
 * @brief
 */
static void init_display_oled(void);

/**
 * @brief
 */
static void init_osd_rtos(void);

/**
 * @brief
 */
static void vOsdUpdaterTimer(TimerHandle_t xTimer);

/**
 * @brief
 */
static void vImgChunkDrawTask(void* pvArg);

/**
 * @brief
 */
static void vImgOsdDrawTask(void* pvArg);


// ----------------------------------------------------------------------
// Static functions

static void IRAM_ATTR
vOsdUpdateCallback(memory_model_types_t xDataId)
{
	switch(xDataId)
	{
	case MEMORY_MODEL_WIFI_SCAN_CHANNEL: {
		vOsdStartDraw();
		break;
	}

	case MEMORY_MODEL_WIFI_CURRENT_CHANNEL: {
		sRedrawChannelNumber = true;
		break;
	}

	case MEMORY_MODEL_WIFI_RTT_VALUE: {
		sRedrawRTT = true;
		break;
	}

	case MEMORY_MODEL_DATA_RX_RATE: {
		sRedrawDataRate = true;
		break;
	}

	case MEMORY_MODEL_WIFI_RX_RSSI: {
		sRedrawRSSi = true;
		break;
	}

	case MEMORY_MODEL_WIFI_TX_POWER_1:
	case MEMORY_MODEL_WIFI_TX_POWER_2: {
		sRedrawTxPower = true;
		break;
	}

	default:
		break;
	}
}


static void
vOsdStartDraw(void)
{
	xTaskNotifyGive(xImgOsdDrawTaskHandler);
}

static uint32_t
ulOsdSyncDraw(TickType_t xTicksToSync)
{
	return ulTaskNotifyTake(pdTRUE, xTicksToSync);
}

// static uint32_t
// ulImgChunkSyncDraw(TickType_t xTicksToSync)
// {
// 	return ulTaskNotifyTake(pdTRUE, xTicksToSync);
// }


static void IRAM_ATTR
draw_osd_screen(void)
{
#ifdef OSD_DRAW_TIME_DBG_PROFILER
	profile_point(profile_point_start, OSD_DRAW_TIME_DBG_PROFILER_POINT_ID);
#endif

	if(sRedrawRSSi)
	{
		sRedrawRSSi = false;

		sprintf(&gui_text_buf[0], "%d", ulMemoryModelGet(MEMORY_MODEL_WIFI_RX_RSSI));
		tft_oled.drawString(&gui_text_buf[0], TEXT_POS_X_FOR_RSSI, TEXT_POS_Y_FOR_RSSI);
	}

	if(sRedrawTxPower)
	{
		sRedrawTxPower = false;

		sprintf(&gui_text_buf[0], "%02u", ulMemoryModelGet(MEMORY_MODEL_WIFI_TX_POWER_1));
		tft_oled.drawString(&gui_text_buf[0], TEXT_POS_X_FOR_TX_POWER_1, TEXT_POS_Y_FOR_TX_POWER_1);

		sprintf(&gui_text_buf[0], "%02u", ulMemoryModelGet(MEMORY_MODEL_WIFI_TX_POWER_2));
		tft_oled.drawString(&gui_text_buf[0], TEXT_POS_X_FOR_TX_POWER_2, TEXT_POS_Y_FOR_TX_POWER_2);
	}

	if(sRedrawRTT)
	{
		sRedrawRTT = false;

		sprintf(&gui_text_buf[0], "%04u", ulMemoryModelGet(MEMORY_MODEL_WIFI_RTT_VALUE));
		tft_oled.drawString(&gui_text_buf[0], TEXT_POS_X_FOR_RTT, TEXT_POS_Y_FOR_RTT);
	}

	if(sRedrawDataRate)
	{
		sRedrawDataRate = false;

		// Convert to kB/s
		sprintf(&gui_text_buf[0], "%03u", ulMemoryModelGet(MEMORY_MODEL_DATA_RX_RATE) / 1024);
		tft_oled.drawString(&gui_text_buf[0], TEXT_POS_X_FOR_DATA_RATE, TEXT_POS_Y_FOR_DATA_RATE);
	}

	if(sRedrawChannelNumber)
	{
		sRedrawChannelNumber = false;

		sprintf(&gui_text_buf[0], "%02u", ulMemoryModelGet(MEMORY_MODEL_WIFI_CURRENT_CHANNEL));
		tft_oled.drawString(&gui_text_buf[0], TEXT_POS_X_FOR_CHANNEL, TEXT_POS_Y_FOR_CHANNEL);
	}

	sprintf(&gui_text_buf[0], "%02u", ulAvgFPS);
	tft_oled.drawString(&gui_text_buf[0], TEXT_POS_X_FOR_FPS, TEXT_POS_Y_FOR_FPS);

	sprintf(&gui_text_buf[0], "%03u", ulAvgFrameTime);
	tft_oled.drawString(&gui_text_buf[0], TEXT_POS_X_FOR_FRAME_TIME, TEXT_POS_Y_FOR_FRAME_TIME);

#ifdef OSD_DRAW_TIME_DBG_PROFILER
	profile_point(profile_point_end, OSD_DRAW_TIME_DBG_PROFILER_POINT_ID);
#endif
}

static void IRAM_ATTR
draw_img_chunk(JpgMagicChunk_t* pxJpgMagicChunk)
{
#ifdef IMG_CHUNK_DRAW_DBG_PROFILER
	profile_point(profile_point_start, IMG_CHUNK_DRAW_DBG_PROFILER_POINT_ID);
#endif
	// tft.startWrite();

	// tft.setAddrWindow(pxJpgMagicChunk->usPosX, pxJpgMagicChunk->usPosY, pxJpgMagicChunk->usW, pxJpgMagicChunk->usH);
	// tft.pushPixelsDMA((uint16_t*)&pxJpgMagicChunk->usBitmapBuf[0], pxJpgMagicChunk->usPixels);

	tft.setWindow(pxJpgMagicChunk->usPosX,
	              pxJpgMagicChunk->usPosY,
	              pxJpgMagicChunk->usPosX + pxJpgMagicChunk->usW - 1,
	              pxJpgMagicChunk->usPosY + pxJpgMagicChunk->usH - 1);

	tft.writePixels((uint16_t*)&pxJpgMagicChunk->usBitmapBuf[0], pxJpgMagicChunk->usPixels);

	// tft.endWrite();

#ifdef IMG_CHUNK_DRAW_DBG_PROFILER
	profile_point(profile_point_end, IMG_CHUNK_DRAW_DBG_PROFILER_POINT_ID);
#endif
}

static void
draw_gui(void)
{
	// Signal strength from the transmitter
	tft_oled.drawString(TEXT_FOR_RSSI, 0, TEXT_POS_Y_FOR_RSSI);
	// Round time trip
	tft_oled.drawString(TEXT_FOR_RTT, 0, TEXT_POS_Y_FOR_RTT);
	// Data rate in kB/s
	tft_oled.drawString(TEXT_FOR_DATA_RATE, 0, TEXT_POS_Y_FOR_DATA_RATE);
	// Receiver transmit power
	tft_oled.drawString(TEXT_FOR_TX_POWER_1, 0, TEXT_POS_Y_FOR_TX_POWER_1);
	// Transmitter transmit power
	tft_oled.drawString(TEXT_FOR_TX_POWER_2, 0, TEXT_POS_Y_FOR_TX_POWER_2);
	// Channel number
	tft_oled.drawString(TEXT_FOR_CHANNEL, 0, TEXT_POS_Y_FOR_CHANNEL);
	// Frames per second
	tft_oled.drawString(TEXT_FOR_FPS, 0, TEXT_POS_Y_FOR_FPS);
	// Frame time
	tft_oled.drawString(TEXT_FOR_FRAME_TIME, 0, TEXT_POS_Y_FOR_FRAME_TIME);
}

static void
init_display_tft(void)
{
	tft.init();
	tft.setSwapBytes(false);
	tft.setColorDepth(16);
	tft.setRotation(1);
	tft.clear(TFT_BLACK);

	// --------------------------
	// Draw basic OSD GUI
	tft.drawRect(2, 2, tft.width() - 2, tft.height() - 2, TFT_WHITE);
	tft.drawRect(38, 8, 244, 180, TFT_WHITE);
}

static void
init_display_oled(void)
{
	tft_oled.init();
	tft_oled.clear();
	tft_oled.setFont(&fonts::AsciiFont8x16);
	tft_oled.setRotation(3);
	// tft_oled.setTextSize(2);

	tft_oled.drawString("Check...", 0, 0);
	tft_oled.drawString("Ch:", 0, 20);
}

static void
init_osd_rtos(void)
{
	xOsdUpdateTimer = xTimerCreateStatic("xOsdUpdateTimer",
	                                     pdMS_TO_TICKS(OSD_UPDATE_TIMEOUT),
	                                     pdTRUE,
	                                     NULL,
	                                     (TimerCallbackFunction_t)(vOsdUpdaterTimer),
	                                     &xOsdUpdateTimerControlBlock);
	assert(xOsdUpdateTimer);


	xImgOsdDrawTaskHandler = xTaskCreateStaticPinnedToCore((TaskFunction_t)(vImgOsdDrawTask),
	                                                       assigned_name_for_task_img_osd_draw,
	                                                       STACK_WORDS_SIZE_FOR_TASK_IMG_OSD_DRAW,
	                                                       NULL,
	                                                       PRIORITY_LEVEL_FOR_TASK_IMG_OSD_DRAW,
	                                                       xImgOsdDrawStack,
	                                                       &xImgOsdDrawTaskControlBlock,
	                                                       (BaseType_t)PINNED_CORE_FOR_TASK_IMG_OSD_DRAW);
	assert(xImgOsdDrawTaskHandler);

	xImgChunkDrawTaskHandler = xTaskCreateStaticPinnedToCore((TaskFunction_t)(vImgChunkDrawTask),
	                                                         assigned_name_for_task_img_chunk_draw,
	                                                         STACK_WORDS_SIZE_FOR_TASK_IMG_CHUNK_DRAW,
	                                                         NULL,
	                                                         PRIORITY_LEVEL_FOR_TASK_IMG_CHUNK_DRAW,
	                                                         xImgChunkDrawStack,
	                                                         &xImgChunkDrawTaskControlBlock,
	                                                         (BaseType_t)PINNED_CORE_FOR_TASK_IMG_CHUNK_DRAW);
	assert(xImgChunkDrawTaskHandler);
}

// ----------------------------------------------------------------------
// Accessors functions

void
vImgChunkStartDraw(void)
{
	xTaskNotifyGive(xImgChunkDrawTaskHandler);
}


// ----------------------------------------------------------------------
// FreeRTOS functions

static void
vOsdUpdaterTimer(TimerHandle_t xTimer)
{
	(void)xTimer;

	// Tell main GUI thread to update OSD
	vOsdStartDraw();

#ifdef OSD_UPDATE_TIME_DBG_PRINTOUT
	async_printf(async_print_type_u32, assigned_name_for_task_img_osd_draw, 0);
#endif
}

static void
vImgChunkDrawTask(void* pvArg)
{
	(void)pvArg;

	task_sync_get_bits(TASK_SYNC_EVENT_BIT_IMG_CHUNK_DRAW);

#ifdef TASK_START_EVENT_DBG_PRINTOUT
	async_printf(async_print_type_str, assigned_name_for_task_img_chunk_draw, 0);
#endif

	for(;;)
	{
		// if(ulImgChunkSyncDraw(portMAX_DELAY))
		{
			draw_img_chunk(pxImageDecoderGetMagicChunk());
		}
	}
}

static void
vImgOsdDrawTask(void* pvArg)
{
	(void)pvArg;

	task_sync_get_bits(TASK_SYNC_EVENT_BIT_IMG_OSD_DRAW);

	do
	{
		if(ulOsdSyncDraw(1))
		{
			sprintf(&gui_text_buf[0], "%u", ulMemoryModelGet(MEMORY_MODEL_WIFI_SCAN_CHANNEL));
			tft_oled.drawString(&gui_text_buf[0], 32, 20);
		}
	} while(!xImageDecoderChunksAvailable());

	tft_oled.clear();
	draw_gui();
	xTimerStart(xOsdUpdateTimer, 0UL);

	for(;;)
	{
		if(ulOsdSyncDraw(portMAX_DELAY))
		{
			draw_osd_screen();
		}
	}
}

// ----------------------------------------------------------------------
// Core functions

void
init_osd_stats(void)
{
	init_osd_rtos();

	assert(xMemoryModelRegisterCallback((memory_model_callback_t)vOsdUpdateCallback));
}

void
init_display(void)
{
	init_display_tft();
	init_display_oled();
}