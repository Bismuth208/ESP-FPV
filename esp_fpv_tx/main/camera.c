#include "camera.h"

#include "camera_pins.h"
#include "data_common.h"
#include "debug_tools_conf.h"
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
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_attr.h>
#include <esp_camera.h>
#include <esp_psram.h>
#include <esp_timer.h>
//
#include <assert.h>
#include <stdint.h>
#include <string.h>


// ----------------------------------------------------------------------
// Definitions, type & enum declaration

// On ESP-CAM from Ai-Thinker
#define CAMERA_LED_PIN (4)

// Maximum time before connection is treated as "Lost" and Timer will give forced Ack
// To start sending new frames
// Trigger pseudo software watchdog every 100 ms
#define FORCE_FRAME_UPDATE_TIMER_TIMEOUT (100)
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


// ----------------------------
// clang-format off
static const camera_config_t xCamConfig_psram = {
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

	.xclk_freq_hz = 20000000, //EXPERIMENTAL: Set to 16MHz on ESP32-S2 or ESP32-S3 to enable EDMA mode
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
	.frame_size   = FRAMESIZE_QVGA,
	.jpeg_quality = 20, // 10-63 lower number means higher quality

	.fb_count       = 2,
	.fb_location    = CAMERA_FB_IN_PSRAM,
	.grab_mode      = CAMERA_GRAB_LATEST,//CAMERA_GRAB_WHEN_EMPTY,

	.sccb_i2c_port = -1
};

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
	.frame_size   = FRAMESIZE_QVGA,
	.jpeg_quality = 20, //10-63 lower number means higher quality
	
	.fb_count     = 1,
	.fb_location  = CAMERA_FB_IN_DRAM,
	.grab_mode    = CAMERA_GRAB_LATEST,//CAMERA_GRAB_WHEN_EMPTY,

	.sccb_i2c_port = -1
};
// clang-format on

// ----------------------------------------------------------------------
// Static functions declaration

/**
 * @brief Grab frame from Camera, splits into chunks by 250 bytes
 *        and add to Tx queue
 * 
 */
static void take_new_image_frame(void);

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
	camera_fb_t* fb = NULL;

	do
	{
		fb = esp_camera_fb_get();
	} while(fb == NULL);

	uint16_t marker = 0;
	uint32_t ofs = 0;
	uint8_t ucImageData = 0;

	// printf("\n----New chunk-----\n");

	/* Find SOI marker */
	do
	{
		ucImageData = fb->buf[ofs];
		marker = marker << 8 | ucImageData;
		// printf("%02x ", fb->buf[ofs]);
		++ofs;
		// if((ofs & 0x0f) == 0x0f)
		// {
		// 	printf("\n");
		// }
	} while(marker != 0xFFDA);

	// Now, we could send header once since know size of:
	// + 'SOI'
	// + 'SOF0'
	// + 'DRI'
	// + 'DHT'
	// + 'DQT'
	usDataOffsetExtra = ofs;

	// printf("\nofs: %d\n", ofs);
	// printf("\nCRC32: %08x\n", ulCrc);
	// printf("\n----End chunk-----\n");

	// Now we can send our Jpg header
	vWirelessSendArray(PACKET_TYPE_INITIAL_HEADER_DATA, &fb->buf[0], usDataOffsetExtra, pdTRUE);

	esp_camera_fb_return(fb);
}


static void IRAM_ATTR
take_new_image_frame(void)
{
	camera_fb_t* fb = NULL;
#ifdef IMAGE_TX_SIZE_DBG_PRINTOUT
	size_t fb_len = 0;
#endif

	uint32_t ulDataLeft = 0;
	uint8_t* pucImage = NULL;

#ifdef NEW_IMAGE_FRAME_TX_TIME_DBG_PROFILER
	profile_point(profile_point_start, NEW_IMAGE_FRAME_TX_TIME_DBG_PROFILER_POINT_ID);
#endif

	fb = esp_camera_fb_get();
	if(!fb)
	{
#ifdef CAMERA_CAPTURE_FAIL_DBG_PRINTOUT
		async_printf(async_print_type_str, "Camera capture failed\n", 0);
#endif
		return;
	}

	pucImage = (uint8_t*)fb->buf;
	ulDataLeft = fb->len;

	// As we pre-sent Jpg header previously we could skip entire header.
	// It doesn't change.
	pucImage += usDataOffsetExtra;
	ulDataLeft -= usDataOffsetExtra;

	// And also skip those 2 bytes of 'EOI'
	ulDataLeft -= 2;

#ifdef IMAGE_TX_SIZE_DBG_PRINTOUT
	fb_len = ulDataLeft;
#endif

	// if(ulWaitNewFrameAck(1))
	{
		// I'm pretty sure what without Jpg header is's possible to decrypt an Image.
		// So, encryption is not needed and we can save some Air time and CPU cycles :)
		vWirelessSendArray(PACKET_TYPE_FRAME_DATA, pucImage, ulDataLeft, pdFALSE);
	}

	esp_camera_fb_return(fb);

#ifdef IMAGE_TX_SIZE_DBG_PRINTOUT
	async_printf(async_print_type_u32, "JPG: %uB\n", (uint32_t)(fb_len));
#endif


#ifdef NEW_IMAGE_FRAME_TX_TIME_DBG_PROFILER
	profile_point(profile_point_end, NEW_IMAGE_FRAME_TX_TIME_DBG_PROFILER_POINT_ID);
#endif
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

#ifdef TIMER_FRAME_UPDATE_DBG_PRINTOUT
	async_printf(async_print_type_str, "vForceFrameUpdateTimer\n", 0);
#endif
}


static void
vCameraTask(void* pvArg)
{
	(void)pvArg;
	// No further execution until full initialization
	task_sync_get_bits(TASK_SYNC_EVENT_BIT_CAMERA);

#ifdef ENABLE_TASK_START_EVENT_DBG_PRINTOUT
	async_printf(async_print_type_str, assigned_name_for_task_camera, 0);
#endif

	if(ulWaitNewFrameAck(portMAX_DELAY))
	{
		vInitialCameraSync();
		vStartNewFrame();
	}

	xTimerStart(xForceFrameUpdateTimer, 0UL);

#ifdef IMAGE_TX_TIME_DBG_PRINTOUT
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
#ifdef IMAGE_TX_TIME_DBG_PRINTOUT
			fr_start = esp_timer_get_time();
			take_new_image_frame();
			fr_end = esp_timer_get_time();

			async_printf(async_print_type_u32, "tFr time: %u\n", fr_end - fr_start);
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

	esp_err_t err = ESP_FAIL;

	if(esp_psram_is_initialized() /*psramFound()*/)
	{
		err = esp_camera_init(&xCamConfig_psram);
	}
	else
	{
		err = esp_camera_init(&xCamConfig_no_psram);
	}

	if(err != ESP_OK)
	{
#ifdef CAMERA_INIT_FAIL_DBG_PRINTOUT
		async_printf(async_print_type_u32, "Camera init failed with error 0x%x\n", err);
		vTaskDelay(pdMS_TO_TICKS(500));
#endif
		assert(pdFALSE);
		return;
	}

	init_camera_led();
}
