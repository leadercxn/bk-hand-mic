#ifndef __BATTERY_HANDLER_H
#define __BATTERY_HANDLER_H

#define BATTERY_MV_LEVEL_3_MIN  2800
#define BATTERY_MV_LEVEL_2_MIN  2550
#define BATTERY_MV_LEVEL_1_MIN  2350

void battery_loop_task(void);

uint16_t battery_mv_get(void);
uint8_t battery_mv_level_get(void);

#endif
