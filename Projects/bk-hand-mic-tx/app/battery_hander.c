#include "stdio.h"
#include "board_config.h"
#include "battery_handler.h"

uint8_t battery_mv_level_get(void)
{
    uint16_t battery_mv = adc_ch_result_get(ADC_CHANNEL_1) * 3300 / 4095;
    uint8_t  battery_mv_level = 0;

    if(battery_mv > BATTERY_MV_LEVEL_3_MIN)
    {
        battery_mv_level = 3;
    }
    else if(battery_mv > BATTERY_MV_LEVEL_2_MIN)
    {
        battery_mv_level = 2;
    }
    else if(battery_mv > BATTERY_MV_LEVEL_1_MIN)
    {
        battery_mv_level = 1;
    }
    else
    {
        battery_mv_level = 0;
    }

    return battery_mv_level;
}

