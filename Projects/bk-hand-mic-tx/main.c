#include "board_config.h"

#include "bk953x_handler.h"
#include "lcd_display_handler.h"
#include "button_handler.h"
#include "battery_handler.h"
#include "flash_handler.h"

#define SCHED_MAX_EVENT_DATA_SIZE   8
#define SCHED_QUEUE_SIZE            20

static app_scheduler_t  m_app_scheduler;

static gpio_object_t   m_power_on_gpio = 
{
    .gpio_port_periph_clk = POWER_ON_PORT_PERIPH_CLK,
    .p_gpio_port = POWER_ON_PORT,
    .gpio_dir = GPIO_DIR_OUTPUR,
    .gpio_pin = POWER_ON_PIN,
};

typedef enum
{
    CMD_FREQ = 0x01,
    CMD_VOL,
    CMD_TX_POWER,
} ir_tx_cmd_e;

/**
 * ir红外传输数据结构体
 */
typedef struct
{
    uint8_t len;
    uint8_t band_type;
    uint8_t region_band;
    uint8_t freq_ch;
} __attribute__((__packed__ )) ir_command_freq_t;

typedef struct
{
    ir_tx_cmd_e cmd;

    union data 
    {
      ir_command_freq_t freq_data;
      uint8_t           cmd_data[4];
    } __attribute__((__packed__ )) tx_data;

    uint8_t crc;
} __attribute__((__packed__ )) ir_tx_data_t;

union user_spec_data
{
  uint8_t byte;

  struct
  {
    uint8_t chan_sync_flag : 1; //信道同步完成标志
    uint8_t batt_level     : 2; //电池电平
    uint8_t reserve        : 5;
  } user_data;
};

/**
 * 系统开机flag
 */
static bool m_sys_power_on = false;
static uint64_t m_per_battery_sample_ticks = 0;

static union user_spec_data m_user_spec_data = {
  .byte = 0,
};

/**
 * @brief 按键回调
 */
static void button_handler(button_event_e event)
{
    static uint64_t pre_ticks = 0;
    uint64_t now_ticks = mid_timer_ticks_get();

    static button_event_e old_event = BUTTON_EVENT_MAX;

    /**
     * 两个时间得间隔200ms
     */
    if(old_event != event)
    {
        old_event = event;
    }
    else if(event == BUTTON_EVENT_LONG_PUSH)
    {
        if(now_ticks - pre_ticks > 2000)
        {
            pre_ticks = now_ticks;
        }
        else
        {
          return;
        }
    }
    else if(now_ticks - pre_ticks > 200)
    {
        pre_ticks = now_ticks;
    }
    else
    {
      return;
    }
  
    switch(event)
    {
        case BUTTON_EVENT_PUSH:
          if(m_sys_power_on)
          {
            lcd_black_light_enable(true);
            trace_debug("turn on lcd led\n\r");
          }

          break;

        case BUTTON_EVENT_LONG_PUSH:
          if(m_sys_power_on)
          {
              m_sys_power_on = false;
              trace_debug("system power off\n\r");

              bk953x_task_stage_set(BK_STATE_IDLE);
          }
          else
          {
              m_sys_power_on = true;
              trace_debug("system power on\n\r");

              bk953x_task_stage_set(BK_STAGE_INIT);
          }

          gpio_output_set(&m_power_on_gpio, m_sys_power_on);
          lcd_black_light_enable(m_sys_power_on);
          lcd_off_status_set(m_sys_power_on);
          break;

        default:
          break;
    }
}

static uint16_t ch_convert_to_freq(uint8_t ch, region_band_e region_band, band_type_e band_type)
{
  uint16_t freq = 6320;

  switch(region_band)
  {
      case REGION_BAND_DEFAULT:       //默认地区

        #define U_BAND_CH_101_FREQ   6600
        #define U_BAND_CH_1_FREQ     6320

        if(band_type == BAND_TYPE_V)  //V段
        {

        }
        else                          //U段
        {
          if(ch > 100)
          {
            freq = U_BAND_CH_101_FREQ + 3 * (ch - 101);
          }
          else
          {
            freq = U_BAND_CH_1_FREQ + 3 * (ch - 1);
          }
        }
        break;

      default:
        break;
  }

  return freq;
}

static void app_evt_schedule(void * p_event_data)
{
    trace_debug("app_evt_schedule\n\r");
}

/**
 * @brief host-task 主任务，负责各个task之间的交互
 */
static void host_loop_task(void)
{
    uint8_t ir_rx_data_len = 0;
    uint8_t ir_rx_data[16] = {0};

    static uint64_t old_ir_rx_ticks = 0;

    /**
     * 关机状态下不执行
     */
    if(!m_sys_power_on)
    {
        return;
    }

    /**
     * 一分钟采样一次电池电压并更新LCD
     */
    if(((mid_timer_ticks_get() - m_per_battery_sample_ticks) > 60000) || (m_per_battery_sample_ticks == 0))
    {
        m_per_battery_sample_ticks = mid_timer_ticks_get();

        lcd_battery_level_set( battery_mv_level_get() );
    }

    /**
     * 红外解码
     */
    ir_rx_data_len = ir_rx_decode_result_get(ir_rx_data);
    if(ir_rx_data_len)
    {
        uint16_t freq = 6320;
        ir_tx_data_t ir_tx_data;

        old_ir_rx_ticks = mid_timer_ticks_get();

        trace_debug("host_loop_task ir rx data:");
        trace_dump_d(ir_rx_data, ir_rx_data_len);

        memset(&ir_tx_data,0,sizeof(ir_tx_data_t));
        memcpy(&ir_tx_data,ir_rx_data,sizeof(ir_tx_data_t));
        /**
         * 解析数据
         */
        switch(ir_tx_data.cmd)
        {
          case CMD_FREQ:
              g_app_param.band_type = ir_tx_data.tx_data.freq_data.band_type;
              g_app_param.region_band = ir_tx_data.tx_data.freq_data.region_band;
              g_app_param.ch_index = ir_tx_data.tx_data.freq_data.freq_ch;

              freq = ch_convert_to_freq(g_app_param.ch_index, g_app_param.region_band, g_app_param.band_type);

              /**
               * 设置LCD freq显示
               */
              lcd_channel_freq_set(freq);
              lcd_black_light_enable(true);

              /**
               * 设置BK9531频点
               */
              bk953x_ch_index_set(g_app_param.ch_index,freq);
              app_param_flash_update();

              /**
               * 设置用户数据，提示信道同步完成
               */
              m_user_spec_data.user_data.chan_sync_flag = 1;
              bk953x_user_spec_data_set(m_user_spec_data.byte);

            break;
        }
    }
    else if(m_user_spec_data.user_data.chan_sync_flag)
    {
      /**
       * 超过1500ms没接收到IR数据,置0
       */
      if(mid_timer_ticks_get() - old_ir_rx_ticks > 1500)
      {
        /**
         * 同步完成，清0
         */
        m_user_spec_data.user_data.chan_sync_flag = 0;
        bk953x_user_spec_data_set(m_user_spec_data.byte);
        trace_debug("clean ir chan_sync_flag\n\r");
      }
    }
}

int main(void)
{
  trace_init();

  /*greeting*/
  trace_info("\n\r\n\r\n\r\n\r");
  trace_info("       *** Welcome to the Hand-Mic Project ***\n\r");
  trace_info("\n\r");


  TIMER_INIT();

  /**
   * delay 函数的初始化
   */
  mid_system_tick_init();

  app_param_flash_init();

  APP_SCHED_INIT(&m_app_scheduler, SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
//  app_sched_event_put(&m_app_scheduler, NULL, 0, app_evt_schedule);

  bk953x_ch_index_set(g_app_param.ch_index,ch_convert_to_freq(g_app_param.ch_index, g_app_param.region_band, g_app_param.band_type));

  lcd_display_init();
  lcd_channel_freq_set(ch_convert_to_freq(g_app_param.ch_index, g_app_param.region_band, g_app_param.band_type));

  gpio_config(&m_power_on_gpio);
  button_hw_init(button_handler);

  /**
   * 资源申请
   */
  bk9531_init();

  ir_rx_init();

  /* adc 相关 */
  adc_init();

  trace_info("Start loop\n\r");
  while(1)
  {
    app_sched_execute(&m_app_scheduler);

    bk953x_loop_task();
    button_loop_task();
    lcd_display_loop_task();
    host_loop_task();
  }
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif


