#ifndef _DISPLAY_OSD_H
#define _DISPLAY_OSD_H

#ifdef __cplusplus
extern "C" {
#endif


// ----------------------------------------------------------------------
// Definitions, type & enum declaration

#define TEXT_FOR_RSSI       ("RSSi:")
#define TEXT_POS_X_FOR_RSSI (40)
#define TEXT_POS_Y_FOR_RSSI (0)

#define TEXT_FOR_RTT       ("RTT:")
#define TEXT_POS_X_FOR_RTT (32)
#define TEXT_POS_Y_FOR_RTT (16)

#define TEXT_FOR_DATA_RATE       ("kBs:")
#define TEXT_POS_X_FOR_DATA_RATE (32)
#define TEXT_POS_Y_FOR_DATA_RATE (32)

#define TEXT_FOR_TX_POWER_1       ("P1%:")
#define TEXT_POS_X_FOR_TX_POWER_1 (32)
#define TEXT_POS_Y_FOR_TX_POWER_1 (48)

#define TEXT_FOR_TX_POWER_2       ("P2%:")
#define TEXT_POS_X_FOR_TX_POWER_2 (32)
#define TEXT_POS_Y_FOR_TX_POWER_2 (64)

#define TEXT_FOR_CHANNEL       ("Ch:")
#define TEXT_POS_X_FOR_CHANNEL (32)
#define TEXT_POS_Y_FOR_CHANNEL (80)

#define TEXT_FOR_FPS       ("FPS:")
#define TEXT_POS_X_FOR_FPS (32)
#define TEXT_POS_Y_FOR_FPS (96)

#define TEXT_FOR_FRAME_TIME       ("Tfr:")
#define TEXT_POS_X_FOR_FRAME_TIME (32)
#define TEXT_POS_Y_FOR_FRAME_TIME (112)


// ----------------------------------------------------------------------
// Accessors functions

/**
 * @brief
 */ 
void vImgChunkStartDraw(void);

// ----------------------------------------------------------------------
// Core functions

/**
 * @brief
 */
void init_osd_stats(void);

/**
 * @brief
 */
void init_display(void);


#ifdef __cplusplus
}
#endif

#endif /* _DISPLAY_OSD_H */