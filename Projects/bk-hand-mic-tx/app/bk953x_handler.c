#include "stdio.h"
#include "board_config.h"
#include "bk953x_handler.h"

static bk953x_object_t m_bk9531_obj;

static gpio_object_t   m_bk9531_rst;

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

static void bk953x_stage_task_run(bk953x_task_t *p_task)
{
    int err_code = 0;

    static uint64_t old_ticks = 0;
    static uint64_t mic_old_ticks = 0;
    gpio_object_t *p_rst_gpio = (gpio_object_t *)p_task->p_bk953x_object->p_rst_gpio;

    uint8_t mic_rssi = 0;

    static uint8_t usr_data = 0x01;

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
            break;

        case BK_STAGE_NORMAL:

                if(mid_timer_ticks_get() - old_ticks > 5000)
                {
                    old_ticks = mid_timer_ticks_get();
                    err_code = bk9531_tx_spec_data_set(p_task->p_bk953x_object, usr_data);
                    if(!err_code)
                    {
                        trace_debug("bk9531_tx_spec_data_set success usr_data = 0x%02x\n\r",usr_data);
                        usr_data++;
                    }
                }

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

/**
 * @brief 设置通道序号来获取对应寄存器的值
 */
int bk953x_ch_index_set(uint16_t chan_index)
{
    freq_chan_object_t freq_chan_obj;

    freq_chan_obj.chan_index = chan_index;
}
