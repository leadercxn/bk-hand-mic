#ifndef __FLASH_HANDLER_H
#define __FLASH_HANDLER_H

#include "board_config.h"

typedef struct
{
    uint32_t magic;

    uint8_t         ch_index;
    region_band_e   region_band;      //根据不同的地区，选择频段
    band_type_e     band_type;          //U/V段

    uint32_t crc32;
} __attribute__((aligned(4))) app_param_t;

extern app_param_t g_app_param;

void app_param_flash_init(void);
void app_param_flash_update(void);

#endif
