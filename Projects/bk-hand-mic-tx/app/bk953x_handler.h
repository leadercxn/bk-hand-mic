#ifndef __BK953X_HANDLER_H
#define __BK953X_HANDLER_H

/**
 * 自定义用户数据结构 1B
 * 
 * 
 */

typedef enum
{
    BK_STAGE_INIT,
    BK_STAGE_NORMAL,
    BK_STAGE_SEARCHING,
    BK_STATE_IDLE,
    BK_STAGE_MAX,
} bk953x_task_stage_e;

typedef enum
{
    BK953X_CFG_FREQ,
    BK953X_CFG_RF_POWER,
    BK953X_CFG_MUTE,
    BK953X_CFG_I2S,
    BK953X_CFG_USER_DATA,
    BK953X_CFG_RF_RSSI,
    BK953X_CFG_AF_VOL,
    BK953X_CFG_RX_ID,
} bk953x_cfg_option_e;

int bk9531_init(void);

void bk953x_loop_task(void);

void bk953x_ch_index_set(uint8_t ch_index, uint16_t freq);

void bk953x_task_stage_set(bk953x_task_stage_e stage);

void bk953x_user_spec_data_set(uint8_t user_data);

#endif
