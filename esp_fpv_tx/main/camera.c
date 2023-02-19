#include "camera.h"

#include "camera_pins.h"
#include "data_common.h"
#include "wireless/wireless_main.h"

//
#include <sdkconfig.h>
//
#include <debug_tools_esp.h>
//
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/timers.h>
//
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_attr.h>
#include <esp_camera.h>
#include <esp_timer.h>
//
#include <assert.h>
#include <stdint.h>
#include <string.h>


// ----------------------------------------------------------------------
// Definitions, type & enum declaration

// Maximum time before connection is treated as "Lost" and Timer will give forced Ack
// To start sending new frames
// Trigger pseudo software watchdog every 80 ms
#define FORCE_FRAME_UPDATE_TIMER_TIMEOUT (80)
// Amount of time needed to scan channels for the best one (1 minute)
#define FORCE_FRAME_UPDATE_ON_START_TIMER_TIMEOUT (60000)

// ----------------------------------------------------------------------
// FreeRTOS Variables

#define STACK_WORDS_SIZE_FOR_TASK_CAMERA (2048)
#define PRIORITY_LEVEL_FOR_TASK_CAMERA   (1)
#define PINNED_CORE_FOR_TASK_CAMERA      (1)
const char* assigned_name_for_task_camera = "CameraTask";
TaskHandle_t xCameraTaskHandler = NULL;
StaticTask_t xCameraTaskControlBlock;
StackType_t xCameraStack[STACK_WORDS_SIZE_FOR_TASK_CAMERA];


TimerHandle_t xForceFrameUpdateTimer = NULL;
StaticTimer_t xForceFrameUpdateTimerControlBlock;

//
#define MAX_NEW_FRAMES_TO_START (5)
SemaphoreHandle_t xFrameStartCounterHandler = NULL;
StaticSemaphore_t xFrameStartCounterControlBlock;

// ----------------------------------------------------------------------
// Variables

static uint16_t usDataOffsetExtra = 0;

static uint8_t ucImageData[IMG_JPG_FILE_MAX_SIZE];
static uint16_t usImageDataSize = 0;

static BaseType_t xFirstFrameHeaderSync = pdTRUE;

/// DMA always trigger callback function, but this flag allow to copy
/// AND transfer data over WiFi
static volatile BaseType_t xTakeFrame = pdFALSE;

// ----------------------------
// clang-format off
static const camera_config_t xCamConfig_no_psram = {
	.pin_pwdn     = PWDN_GPIO_NUM,
	.pin_reset    = RESET_GPIO_NUM,
	.pin_xclk     = XCLK_GPIO_NUM,
	.pin_sscb_sda = SIOD_GPIO_NUM,
	.pin_sscb_scl = SIOC_GPIO_NUM,

	.pin_d7    = Y9_GPIO_NUM,
	.pin_d6    = Y8_GPIO_NUM,
	.pin_d5    = Y7_GPIO_NUM,
	.pin_d4    = Y6_GPIO_NUM,
	.pin_d3    = Y5_GPIO_NUM,
	.pin_d2    = Y4_GPIO_NUM,
	.pin_d1    = Y3_GPIO_NUM,
	.pin_d0    = Y2_GPIO_NUM,
	.pin_vsync = VSYNC_GPIO_NUM,
	.pin_href  = HREF_GPIO_NUM,
	.pin_pclk  = PCLK_GPIO_NUM,

	.xclk_freq_hz = 20000000,
	.ledc_timer   = LEDC_TIMER_0,
	.ledc_channel = LEDC_CHANNEL_0,

	/*
	* FRAMESIZE_QVGA   - 320 x 240 - q=30
	* FRAMESIZE_HQVGA  - 240 x 176 - q=20
	* FRAMESIZE_QCIF   - 176 x 144 - q=10
	* FRAMESIZE_QQVGA2 - 128 x 160 - q=10
	* FRAMESIZE_QQVGA  - 160 × 120 - q=5
	*/
	.pixel_format = PIXFORMAT_JPEG,
	// .frame_size   = FRAMESIZE_QVGA,
	.frame_size   = FRAMESIZE_240X240,
	.jpeg_quality = 20, //10-63 lower number means higher quality
	
	.fb_count     = 2,
};
// clang-format on

// ----------------------------------------------------------------------
// Static functions declaration

/**
 * @brief Grab frame from Camera, splits into chunks by 250 bytes
 *        and add to Tx queue
 * 
 */
static inline void take_new_image_frame(void);

/**
 * @brief In case of lost @ref ''PACKET_TYPE_ACK'' packet this 
 *        timer callback if iw was not reset will be triggered
 *        and do forced @ref ''take_new_image_frame''
 * 
 * @note Timeout is described with @ref ''FORCE_FRAME_UPDATE_TIMER_TIMEOUT''
 */
static void vForceFrameUpdateTimer(TimerHandle_t xTimer);

/**
 * @brief 
 * 
 * @param xTicksToSync how much time to waite before forced redraw of the OSD
 * @retval see @ref ''ulTaskNotifyTake''
 * 
 * @note Call only inside ''vImgOsdDrawTask''
 */
static uint32_t ulWaitNewFrameAck(TickType_t xTicksToSync);

/**
 * @brief Creates FreeRTOS objects what need to maintain the Camera
 */
static void init_camera_rtos(void);

/**
 * @brief Periodically call @ref ''take_new_image_frame'' if task was unlocked
 *        with external call of @ref ''vStartNewFrame'' or with forced update
 *        from timer via @ref ''vForceFrameUpdateTimer''
 * 
 * @param pvArg Argument for the task. (Not used and NULL is passed)
 * 
 * @note Frame limit can be set with @ref ''ENABLE_CAMERA_USE_FPS_LIMIT''
 */
static void vCameraTask(void* pvArg);

// ----------------------------------------------------------------------
// Static functions

static void
vInitialCameraSync(void)
{
	// Start DMA transfers
	esp_camera_fb_get();
}


static inline void
take_new_image_frame(void)
{
	xTakeFrame = pdTRUE;
}


void
send_jpg_header(uint8_t* pucImageData)
{
	uint16_t marker = 0;
	uint32_t ofs = 0;
	uint8_t ucData = 0;

	/* Find SOI marker */
	do
	{
		ucData = pucImageData[ofs];
		marker = marker << 8 | ucData;
		++ofs;
	} while(marker != 0xFFDA);

	// Now, we could send header once since know size of:
	// + 'SOI'
	// + 'SOF0'
	// + 'DRI'
	// + 'DHT'
	// + 'DQT'
	usDataOffsetExtra = ofs;

	// Now we can send our Jpg header
	vWirelessSendArray(PACKET_TYPE_INITIAL_HEADER_DATA, &pucImageData[0], usDataOffsetExtra, pdTRUE);
}


static void IRAM_ATTR
camera_data_available(const void* data, size_t count, bool last_dma_transfer)
{
	PROFILE_POINT(CONFIG_JPG_DMA_COPY_TIME_DBG_PROFILER, profile_point_start);

	if(data != NULL)
	{
		const uint32_t* src = (const uint32_t*)data;
		uint32_t* pulDest = (uint32_t*)&ucImageData[usImageDataSize];
		usImageDataSize += count;

		// Source buffer contain one byte of data in every word
		// This is why i cannot use memcpy() here
		// Turns out this is fastest way to copy data
		do
		{
			pulDest[0] = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
			pulDest[1] = src[4] | (src[5] << 8) | (src[6] << 16) | (src[7] << 24);

			pulDest += 2;
			src += 8;
			count -= 8;
		} while(count >= 8);

		if(count)
		{
			uint8_t* pucDest = (uint8_t*)pulDest;

			do
			{
				*pucDest++ = (uint8_t)*src++;
			} while(--count);
		}
	}
	else
	{
		if(last_dma_transfer)
		{
			if(xTakeFrame == pdTRUE)
			{
				xTakeFrame = pdFALSE;

				if(xFirstFrameHeaderSync)
				{
					xFirstFrameHeaderSync = pdFALSE;
					send_jpg_header(&ucImageData[0]);
				}

				usImageDataSize -= (usDataOffsetExtra);

				PROFILE_POINT(CONFIG_JPG_EOI_SEARCH_TIME_DBG_PROFILER, profile_point_start);

				// Start from the second half of the image what potentially contain garbage
				uint8_t* pucGarbage = (uint8_t*)&ucImageData[usDataOffsetExtra + (usImageDataSize / 2)];

				// With a blank image (covered lid on lens) it takes ~16±2us to find EOI (240x240)
				// On complex image with bunch of details it takes ~70±8us to find EOI (240x240)
				while(*(uint16_t*)pucGarbage++ != 0xD9FF)
					;
				usImageDataSize = pucGarbage - (uint8_t*)&ucImageData[usDataOffsetExtra];

				PROFILE_POINT(CONFIG_JPG_EOI_SEARCH_TIME_DBG_PROFILER, profile_point_end);

				// Copy data to Tx queue
				vWirelessSendArray(PACKET_TYPE_FRAME_DATA, &ucImageData[usDataOffsetExtra], usImageDataSize, pdTRUE);
			}

			usImageDataSize = 0;
		}
	}

	PROFILE_POINT(CONFIG_JPG_DMA_COPY_TIME_DBG_PROFILER, profile_point_end);
}

static uint32_t
ulWaitNewFrameAck(TickType_t xTicksToSync)
{
	return xSemaphoreTake(xFrameStartCounterHandler, xTicksToSync);
	// TODO: BD-0013 Frame limiter switch
	// return ulTaskNotifyTake(pdTRUE, xTicksToSync);
}

static void
init_camera_rtos(void)
{
	xFrameStartCounterHandler =
	    xSemaphoreCreateCountingStatic(MAX_NEW_FRAMES_TO_START, 0, &xFrameStartCounterControlBlock);
	assert(xFrameStartCounterHandler);

	xForceFrameUpdateTimer = xTimerCreateStatic("xForceFrameUpdateTimer",
	                                            pdMS_TO_TICKS(FORCE_FRAME_UPDATE_ON_START_TIMER_TIMEOUT),
	                                            pdTRUE,
	                                            NULL,
	                                            (TimerCallbackFunction_t)(vForceFrameUpdateTimer),
	                                            &xForceFrameUpdateTimerControlBlock);
	assert(xForceFrameUpdateTimer);


	xCameraTaskHandler = xTaskCreateStaticPinnedToCore((TaskFunction_t)(vCameraTask),
	                                                   assigned_name_for_task_camera,
	                                                   STACK_WORDS_SIZE_FOR_TASK_CAMERA,
	                                                   NULL,
	                                                   PRIORITY_LEVEL_FOR_TASK_CAMERA,
	                                                   xCameraStack,
	                                                   &xCameraTaskControlBlock,
	                                                   (BaseType_t)PINNED_CORE_FOR_TASK_CAMERA);
	assert(xCameraTaskHandler);
}

// ----------------------------------------------------------------------
// Accessors functions

void
vCameraSetLEDState(uint32_t ulState)
{
	// gpio_set_level(CAMERA_LED_PIN, ulState);

#if 0
	if(ulState)
	{
		ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, (8191 / 255) * 50));
		ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
	}
	else
	{
		ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
		ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
	}
#endif
}

void
vResetForcedFrameUpdate(void)
{
	xTimerReset(xForceFrameUpdateTimer, 0UL);
}

void
vEnableForcedFrameUpdate(void)
{
	xTimerChangePeriod(xForceFrameUpdateTimer, FORCE_FRAME_UPDATE_TIMER_TIMEOUT, 0UL);
}

void
vStartNewFrame(void)
{
	xSemaphoreGive(xFrameStartCounterHandler);
	// TODO: BD-0013 Frame limiter switch
	// xTaskNotifyGive(xCameraTaskHandler);
}


// ----------------------------------------------------------------------
// FreeRTOS functions

static void
vForceFrameUpdateTimer(TimerHandle_t xTimer)
{
	(void)xTimer;
	vStartNewFrame();

	ASYNC_PRINTF(CONFIG_TIMER_FRAME_UPDATE_DBG_PRINTOUT, async_print_type_str, "vForceFrameUpdateTimer\n", 0);
}


static void
vCameraTask(void* pvArg)
{
	(void)pvArg;
	// No further execution until full initialization
	task_sync_get_bits(TASK_SYNC_EVENT_BIT_CAMERA);

	ASYNC_PRINTF(CONFIG_ENABLE_TASK_START_EVENT_DBG_PRINTOUT, async_print_type_str, assigned_name_for_task_camera, 0);

	if(ulWaitNewFrameAck(portMAX_DELAY))
	{
		vInitialCameraSync();
		vStartNewFrame();
	}

	xTimerStart(xForceFrameUpdateTimer, 0UL);

#if (CONFIG_IMAGE_TX_TIME_DBG_PRINTOUT == 1)
	int64_t fr_start = 0;
	int64_t fr_end = 0;
#endif

#ifdef ENABLE_CAMERA_USE_FPS_LIMIT
	const TickType_t xNextFrameTime = pdMS_TO_TICKS(CAMERA_USE_FPS_LIMIT_TIME);
	TickType_t xLastWakeTime = xTaskGetTickCount();
#endif

	for(;;)
	{
#ifdef ENABLE_CAMERA_USE_FPS_LIMIT
		if(ulWaitNewFrameAck(0))
#else
		if(ulWaitNewFrameAck(portMAX_DELAY))
#endif
		{
#if (CONFIG_IMAGE_TX_TIME_DBG_PRINTOUT == 1)
			fr_start = esp_timer_get_time();
			take_new_image_frame();
			fr_end = esp_timer_get_time();

			ASYNC_PRINTF(1, async_print_type_u32, "tFr time: %u\n", fr_end - fr_start);
#else
			take_new_image_frame();
#endif
		}

#ifdef ENABLE_CAMERA_USE_FPS_LIMIT
		vTaskDelayUntil(&xLastWakeTime, xNextFrameTime);
#endif
	}
}

// ----------------------------------------------------------------------
// Core functions

void
init_camera_led(void)
{
	// gpio_set_direction(CAMERA_LED_PIN, GPIO_MODE_OUTPUT);
	// gpio_set_pull_mode(CAMERA_LED_PIN, GPIO_PULLUP_ONLY);
	// vCameraSetLEDState(pdFALSE);

#if 0
	ledc_channel_config_t ledc_channel_conf = {0};
	ledc_channel_conf.gpio_num = CAMERA_LED_PIN;
	ledc_channel_conf.speed_mode = LEDC_LOW_SPEED_MODE;
	ledc_channel_conf.channel = LEDC_CHANNEL_0;
	ledc_channel_conf.intr_type = LEDC_INTR_DISABLE;
	ledc_channel_conf.timer_sel = LEDC_TIMER_3;
	ledc_channel_conf.duty = 0;//;

	ledc_timer_config_t ledc_timer = {0};
	ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
	ledc_timer.duty_resolution = LEDC_TIMER_13_BIT;
	ledc_timer.timer_num = LEDC_TIMER_3;
	ledc_timer.freq_hz = 50; // With 50Hz less heating on LED
	ledc_timer.clk_cfg = LEDC_AUTO_CLK;

	ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_conf));
	ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
#endif
}

void
init_camera(void)
{
	init_camera_rtos();

	esp_err_t err = err = esp_camera_init(&xCamConfig_no_psram, camera_data_available);

	if(err != ESP_OK)
	{
#if (CONFIG_CAMERA_INIT_FAIL_DBG_PRINTOUT == 1)
		ASYNC_PRINTF(1, async_print_type_u32, "Camera init failed with error 0x%x\n", err);
		vTaskDelay(pdMS_TO_TICKS(500));
#endif // CONFIG_CAMERA_INIT_FAIL_DBG_PRINTOUT
		assert(pdFALSE);
		return;
	}

	init_camera_led();
}
