#include "stdio.h"
#include "stdbool.h"
#include "board_config.h"
#include "flash_handler.h"
#include "bk953x_handler.h"

static bk953x_object_t m_bk9531_obj;

static gpio_object_t   m_bk9531_rst;

static bool m_is_freq_update = false;

static uint8_t m_freq_ch = 1;
static uint16_t m_freq = 6320;

typedef struct
{
    bk953x_task_stage_e stage;
    bk953x_object_t     *p_bk953x_object;
} bk953x_task_t;

static bk953x_task_t m_bk953x_task = {
    .p_bk953x_object = &m_bk9531_obj,
    .stage = BK_STATE_IDLE,
};

int bk9531_init(void)
{
    int err_code = 0;

#ifdef FT32
    m_bk9531_rst.gpio_port_periph_clk = BK9531_CE_PERIPH_CLK;
    m_bk9531_rst.p_gpio_port = BK9531_CE_PORT;
#endif

    m_bk9531_rst.gpio_dir = GPIO_DIR_OUTPUR;
    m_bk9531_rst.gpio_pin = BK9531_CE_PIN;

#ifdef FT32
    m_bk9531_obj.mid_bk953x_object.virt_i2c_object.sda_port_periph_clk = VIRT1_SDA_GPIO_CLK;
    m_bk9531_obj.mid_bk953x_object.virt_i2c_object.scl_port_periph_clk = VIRT1_SCL_GPIO_CLK;
    m_bk9531_obj.mid_bk953x_object.virt_i2c_object.p_sda_gpio_port = VIRT1_SDA_GPIO_PORT;
    m_bk9531_obj.mid_bk953x_object.virt_i2c_object.p_scl_gpio_port = VIRT1_SCL_GPIO_PORT;
    m_bk9531_obj.mid_bk953x_object.virt_i2c_object.sda_gpio_pin = VIRT1_SDA_PIN;
    m_bk9531_obj.mid_bk953x_object.virt_i2c_object.scl_gpio_pin = VIRT1_SCL_PIN;
#endif

    m_bk9531_obj.p_rst_gpio = (void *)&m_bk9531_rst;

    bk9531_res_init(&m_bk9531_obj);

    return err_code;
}

static uint32_t ch_convert_to_regval(uint8_t ch, region_band_e region_band, band_type_e band_type)
{
    uint32_t regval = BK9531_FREQ_632_MHZ;

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
                    regval = BK9531_FREQ_660_MHZ + BK9531_FREQ_0_3_MHZ * (ch - 101);
                }
                else
                {
                    regval = BK9531_FREQ_632_MHZ + BK9531_FREQ_0_3_MHZ * (ch - 1);
                }
            }
            break;

        default:
            break;
    }

    trace_debug("ch_convert_to_regval return 0x%08x\n\r",regval);
    return regval;
}


static void bk953x_stage_task_run(bk953x_task_t *p_task)
{
    int err_code = 0;

    static uint64_t mic_old_ticks = 0;
    gpio_object_t *p_rst_gpio = (gpio_object_t *)p_task->p_bk953x_object->p_rst_gpio;
    uint32_t id = 0x00000000;
    static uint32_t lo_amp_temp = 0;

    freq_chan_object_t freq_chan = {
        .chan_index = 1,
        .freq = 6320,
        .reg_value = 0x4D260000,
    };
     

    uint8_t mic_rssi = 0;

    switch(p_task->stage)
    {
        case BK_STAGE_INIT:
            /**
             * 硬件复位,复位要适当的延时，别太快
             */
            gpio_config(p_rst_gpio);

            gpio_output_set(p_rst_gpio, 1);
            delay_ms(100);
            gpio_output_set(p_rst_gpio, 0);
            delay_ms(100);
            gpio_output_set(p_rst_gpio, 1);
            delay_ms(100);

            bk9531_chip_id_get(p_task->p_bk953x_object);
            trace_debug("9531_chip_id = 0x%08x \n\r",m_bk9531_obj.chip_id);

            /**
             * 寄存器配置
             */
            err_code = bk9531_config_init(p_task->p_bk953x_object);
            if(err_code == 0)
            {
                trace_debug("BK_STAGE_INIT done, go to BK_STAGE_NORMAL\n\r");
                p_task->stage = BK_STAGE_NORMAL;
            }

            freq_chan.reg_value = ch_convert_to_regval(m_freq_ch, g_app_param.region_band, g_app_param.band_type);

            freq_chan.band_type = g_app_param.band_type;
            freq_chan.chan_index = m_freq_ch;
            freq_chan.freq = m_freq;

            bk9531_tx_freq_chan_set(p_task->p_bk953x_object, &freq_chan);

//            bk9531_tx_id_set(p_task->p_bk953x_object,id);

//            mid_bk953x_read_one_reg(&p_task->p_bk953x_object->mid_bk953x_object, BK953X_DEVICE_ID, 0x72, &lo_amp_temp);
//            lo_amp_temp &= 0xff;
            break;

        case BK_STAGE_NORMAL:

            if(m_is_freq_update)
            {
                m_is_freq_update = false;

                freq_chan.reg_value = ch_convert_to_regval(m_freq_ch, g_app_param.region_band, g_app_param.band_type);

                freq_chan.band_type = g_app_param.band_type;
                freq_chan.chan_index = m_freq_ch;
                freq_chan.freq = m_freq;

                bk9531_tx_freq_chan_set(p_task->p_bk953x_object, &freq_chan);
            }
/**
 * 防止出现单载波，然而还是没啥用
 */
#if 0
                mid_bk953x_read_one_reg(&p_task->p_bk953x_object->mid_bk953x_object, BK953X_DEVICE_ID, 0x72, &value);
                value &= 0xff;
                if(value != lo_amp_temp)
                {
                    trace_debug("single RF!\n\r");
                    bk9531_tx_trigger(p_task->p_bk953x_object);
                    delay_ms(5);

                    mid_bk953x_read_one_reg(&p_task->p_bk953x_object->mid_bk953x_object, BK953X_DEVICE_ID, 0x72, &lo_amp_temp);
                    lo_amp_temp &= 0xff;
                }
#endif

/**
 * 读取麦克风的音量大小
 */
#if 0
                if(mid_timer_ticks_get() - mic_old_ticks > 100)
                {
                    mic_old_ticks = mid_timer_ticks_get();
                    err_code = bk9531_tx_mic_rssi_get(p_task->p_bk953x_object, &mic_rssi);
                    if(!err_code)
                    {
                        trace_debug("mic_rssi 0x%02x\n\r",mic_rssi);
                    }
                }
#endif

            break;

        case BK_STAGE_SEARCHING:

            break;

        case BK_STATE_IDLE:
            /**
             * IDLE 状态下，关闭射频
             */
            gpio_output_set(p_rst_gpio, 0);
            break;

        default:
            break;
    }
}

void bk953x_loop_task(void)
{
    bk953x_stage_task_run(&m_bk953x_task);
}

void bk953x_task_stage_set(bk953x_task_stage_e stage)
{
    m_bk953x_task.stage = stage;
}

void bk953x_ch_index_set(uint8_t ch_index, uint16_t freq)
{
    m_is_freq_update = true;
    m_freq_ch = ch_index;
    m_freq = freq;
}

/**
 * @brief 设置通道序号来获取对应寄存器的值
 */
int bk953x_ch_index_search(uint16_t chan_index)
{
    freq_chan_object_t freq_chan_obj;

    freq_chan_obj.chan_index = chan_index;
}

void bk953x_user_spec_data_set(uint8_t user_data)
{
    bk9531_tx_spec_data_set(m_bk953x_task.p_bk953x_object, user_data);
}

