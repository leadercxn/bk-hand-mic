#include "board_config.h"
#include "lcd_display_handler.h"

#define LCD_BACK_LED_CONTINUE_TIME  2500    //ms

union data 
{
    uint8_t byte;

    struct
    {   
        uint8_t com1 : 1;       //低位
        uint8_t com2 : 1;
        uint8_t com3 : 1;
        uint8_t com4 : 1;
        uint8_t reserve : 4;
    } seg;
};

typedef struct
{
    uint8_t         seg_index;
    union data      seg_data;
} lcd_seg_cell_t;


/* seg 表 
 *
 * seg_index + 1 = LCD图中的 SEG
 */
static lcd_seg_cell_t m_seg_table[] = {
    {.seg_index = 0, .seg_data.byte = 0},  //seg1
    {.seg_index = 1, .seg_data.byte = 0},  //seg2
    {.seg_index = 2, .seg_data.byte = 0},
    {.seg_index = 3, .seg_data.byte = 0},   //seg3
    {.seg_index = 4, .seg_data.byte = 0},
    {.seg_index = 5, .seg_data.byte = 0},
    {.seg_index = 6, .seg_data.byte = 0},
    {.seg_index = 7, .seg_data.byte = 0},
    {.seg_index = 8, .seg_data.byte = 0},
    {.seg_index = 9, .seg_data.byte = 0},
    {.seg_index = 10, .seg_data.byte = 0},
    {.seg_index = 11, .seg_data.byte = 0},
    {.seg_index = 12, .seg_data.byte = 0},
    {.seg_index = 13, .seg_data.byte = 0},
    {.seg_index = 14, .seg_data.byte = 0},
    {.seg_index = 15, .seg_data.byte = 0},
    {.seg_index = 16, .seg_data.byte = 0},
};


static bool m_lcd_off = false;
static bool m_old_lcd_off = false;

static uint64_t lcd_bk_old_ticks = 0;

static uint16_t m_lcd_channel_freq = 6320;  // uint = 0.1 MHz
static bool is_lcd_channel_freq_refresh = false;

static uint8_t  m_battery_level = 3;
static bool is_battery_level_refresh = false;

/**
 * 相关IO配置
 */
static lcd_display_obj_t    m_lcd_display_obj = {
    .ht162x.wr_clk_pin.gpio_port_periph_clk = HT1621_WR_PORT_PERIPH_CLK,
    .ht162x.wr_clk_pin.p_gpio_port = HT1621_WR_PORT,
    .ht162x.wr_clk_pin.gpio_dir = GPIO_DIR_OUTPUR,
    .ht162x.wr_clk_pin.gpio_pin = HT1621_WR_PIN,

    .ht162x.cs_pin.gpio_port_periph_clk = HT1621_CS_PORT_PERIPH_CLK,
    .ht162x.cs_pin.p_gpio_port = HT1621_CS_PORT,
    .ht162x.cs_pin.gpio_dir = GPIO_DIR_OUTPUR,
    .ht162x.cs_pin.gpio_pin = HT1621_CS_PIN,

    .ht162x.data_pin.gpio_port_periph_clk = HT1621_DATA_PORT_PERIPH_CLK,
    .ht162x.data_pin.p_gpio_port = HT1621_DATA_PORT,
    .ht162x.data_pin.gpio_dir = GPIO_DIR_OUTPUR,
    .ht162x.data_pin.gpio_pin = HT1621_DATA_PIN,

    .lcd_back_light_pin.gpio_port_periph_clk = LCD_BACK_LIGHT_PORT_PERIPH_CLK,
    .lcd_back_light_pin.p_gpio_port = LCD_BACK_LIGHT_PORT,
    .lcd_back_light_pin.gpio_dir = GPIO_DIR_OUTPUR,
    .lcd_back_light_pin.gpio_pin = LCD_BACK_LIGHT_PIN,
};

/**
 * VER 1.0的LCD图纸
 */
#if 0

/**
 *@brief 适用于 显示数字channel index 和 channel freq 的数码管
 */
static void digital_number_show(lcd_part_e part, uint8_t data)
{
    if(part > DIGITAL_6)
    {
        return;
    }

    lcd_seg_cell_t seg_cell_l;
    lcd_seg_cell_t seg_cell_h;

    encode_seg_code_t seg_code;
	digital_to_segdata(&seg_code, data);

    switch (part)
    {
        case DIGITAL_1:
            seg_cell_l.seg_index = 3;
            seg_cell_h.seg_index = 4;
            break;
        
        case DIGITAL_2:
            seg_cell_l.seg_index = 5;
            seg_cell_h.seg_index = 6;
            break;

        case DIGITAL_3:
            seg_cell_l.seg_index = 14;
            seg_cell_h.seg_index = 15;
            break;

        case DIGITAL_4:
            seg_cell_l.seg_index = 12;
            seg_cell_h.seg_index = 13;
            break;

        case DIGITAL_5:
            seg_cell_l.seg_index = 10;
            seg_cell_h.seg_index = 11;
            break;

        case DIGITAL_6:
            seg_cell_l.seg_index = 8;
            seg_cell_h.seg_index = 9;
            break;
    }

    //m_seg_table[seg_cell_l.seg_index].seg_data.seg.com1 = 0;
    m_seg_table[seg_cell_l.seg_index].seg_data.seg.com2 = seg_code.seg_f;
    m_seg_table[seg_cell_l.seg_index].seg_data.seg.com3 = seg_code.seg_g;
    m_seg_table[seg_cell_l.seg_index].seg_data.seg.com4 = seg_code.seg_e;

    m_seg_table[seg_cell_h.seg_index].seg_data.seg.com1 = seg_code.seg_a;
    m_seg_table[seg_cell_h.seg_index].seg_data.seg.com2 = seg_code.seg_b;
    m_seg_table[seg_cell_h.seg_index].seg_data.seg.com3 = seg_code.seg_c;
    m_seg_table[seg_cell_h.seg_index].seg_data.seg.com4 = seg_code.seg_d;

    ht162x_write(&m_lcd_display_obj.ht162x, seg_cell_l.seg_index, m_seg_table[seg_cell_l.seg_index].seg_data.byte);
    ht162x_write(&m_lcd_display_obj.ht162x, seg_cell_h.seg_index, m_seg_table[seg_cell_h.seg_index].seg_data.byte);
}

/**
 * @brief 适用于特别的字符显示
 */
static void digital_special_show(lcd_part_e part , bool enable)
{
    if(part < DIGITAL_T1)
    {
        return;
    }

    lcd_seg_cell_t seg_cell;

    switch(part)
    {
        case DIGITAL_T1:
            seg_cell.seg_index = 7;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com2 = enable;
            break;

        case DIGITAL_T2:
            seg_cell.seg_index = 7;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com1 = enable;
            break;

        case DIGITAL_T3:
            seg_cell.seg_index = 7;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com3 = enable;
            break;

        case DIGITAL_T4:
            seg_cell.seg_index = 7;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com4 = enable;
            break;

        case DIGITAL_T5:
            seg_cell.seg_index = 16;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com1 = enable;
            break;

        case DIGITAL_T6:
            seg_cell.seg_index = 16;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com2 = enable;
            break;
        
        case DIGITAL_T7:
            seg_cell.seg_index = 16;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com3 = enable;
            break;

        case DIGITAL_T8:
            seg_cell.seg_index = 14;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com1 = enable;
            break;

        case DIGITAL_T9:
            seg_cell.seg_index = 16;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com4 = enable;
            break;

        case DIGITAL_T10:
            seg_cell.seg_index = 8;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com1 = enable;
            break;

        case DIGITAL_T11:
            seg_cell.seg_index = 10;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com1 = enable;
            break;

        case DIGITAL_T12:
            seg_cell.seg_index = 3;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com1 = enable;
            break;

        case DIGITAL_T13:
            seg_cell.seg_index = 12;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com1 = enable;
            break;

        default:
            break;
    }

    ht162x_write(&m_lcd_display_obj.ht162x, seg_cell.seg_index, m_seg_table[seg_cell.seg_index].seg_data.byte);
}

#endif


/**
 * VER X.X 的LCD图纸
 */
#if 1

/**
 *@brief 适用于 显示数字channel index 和 channel freq 的数码管
 */
static void digital_number_show(lcd_part_e part, uint8_t data)
{
    if(part > DIGITAL_6)
    {
        return;
    }

    lcd_seg_cell_t seg_cell_l;
    lcd_seg_cell_t seg_cell_h;

    encode_seg_code_t seg_code;
	digital_to_segdata(&seg_code, data);

    switch (part)
    {
        case DIGITAL_1:
            seg_cell_l.seg_index = 3;
            seg_cell_h.seg_index = 4;
            break;
        
        case DIGITAL_2:
            seg_cell_l.seg_index = 5;
            seg_cell_h.seg_index = 6;
            break;

        case DIGITAL_3:
            seg_cell_l.seg_index = 9;
            seg_cell_h.seg_index = 10;
            break;

        case DIGITAL_4:
            seg_cell_l.seg_index = 11;
            seg_cell_h.seg_index = 12;
            break;

        case DIGITAL_5:
            seg_cell_l.seg_index = 13;
            seg_cell_h.seg_index = 14;
            break;

        case DIGITAL_6:
            seg_cell_l.seg_index = 15;
            seg_cell_h.seg_index = 16;
            break;
    }

    //m_seg_table[seg_cell_l.seg_index].seg_data.seg.com1 = 0;
    m_seg_table[seg_cell_l.seg_index].seg_data.seg.com2 = seg_code.seg_f;
    m_seg_table[seg_cell_l.seg_index].seg_data.seg.com3 = seg_code.seg_g;
    m_seg_table[seg_cell_l.seg_index].seg_data.seg.com4 = seg_code.seg_e;

    m_seg_table[seg_cell_h.seg_index].seg_data.seg.com1 = seg_code.seg_a;
    m_seg_table[seg_cell_h.seg_index].seg_data.seg.com2 = seg_code.seg_b;
    m_seg_table[seg_cell_h.seg_index].seg_data.seg.com3 = seg_code.seg_c;
    m_seg_table[seg_cell_h.seg_index].seg_data.seg.com4 = seg_code.seg_d;

    ht162x_write(&m_lcd_display_obj.ht162x, seg_cell_l.seg_index, m_seg_table[seg_cell_l.seg_index].seg_data.byte);
    ht162x_write(&m_lcd_display_obj.ht162x, seg_cell_h.seg_index, m_seg_table[seg_cell_h.seg_index].seg_data.byte);
}

/**
 * @brief 适用于特别的字符显示
 */
static void digital_special_show(lcd_part_e part , bool enable)
{
    if(part < DIGITAL_T1)
    {
        return;
    }

    lcd_seg_cell_t seg_cell;

    switch(part)
    {
        case DIGITAL_T1:
            seg_cell.seg_index = 7;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com2 = enable;
            break;

        case DIGITAL_T2:
            seg_cell.seg_index = 7;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com1 = enable;
            break;

        case DIGITAL_T3:
            seg_cell.seg_index = 7;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com3 = enable;
            break;

        case DIGITAL_T4:
            seg_cell.seg_index = 7;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com4 = enable;
            break;

        case DIGITAL_T5:
            seg_cell.seg_index = 8;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com1 = enable;
            break;

        case DIGITAL_T6:
            seg_cell.seg_index = 8;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com2 = enable;
            break;
        
        case DIGITAL_T7:
            seg_cell.seg_index = 8;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com3 = enable;
            break;

        case DIGITAL_T8:
            seg_cell.seg_index = 9;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com1 = enable;
            break;

        case DIGITAL_T9:
            seg_cell.seg_index = 8;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com4 = enable;
            break;

        case DIGITAL_T10:
            seg_cell.seg_index = 15;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com1 = enable;
            break;

        case DIGITAL_T11:
            seg_cell.seg_index = 13;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com1 = enable;
            break;

        case DIGITAL_T12:
            seg_cell.seg_index = 3;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com1 = enable;
            break;

        case DIGITAL_T13:
            seg_cell.seg_index = 11;
            m_seg_table[seg_cell.seg_index].seg_data.seg.com1 = enable;
            break;

        default:
            break;
    }

    ht162x_write(&m_lcd_display_obj.ht162x, seg_cell.seg_index, m_seg_table[seg_cell.seg_index].seg_data.byte);
}

#endif


/**
 * @brief 显示通道频率
 * 
 * @param [in] data 单位 0.1 MHz
 */
static void channel_freq_show(uint16_t hund_freq)
{
    uint8_t hundred = hund_freq / 1000;
    uint8_t decade = hund_freq % 1000 / 100;
    uint8_t unit =   hund_freq % 100 / 10;
    uint8_t point = hund_freq % 10;

    digital_number_show(DIGITAL_1, hundred);
    digital_number_show(DIGITAL_2, decade);
    digital_number_show(DIGITAL_3, unit);
    digital_number_show(DIGITAL_4, point);

    digital_special_show(DIGITAL_T13, true);

    digital_number_show(DIGITAL_5, 0);
    digital_number_show(DIGITAL_6, 0);
}

/**
 * @brief 显示关机logo
 */
static void channel_off_show(void)
{
    uint8_t i = 0;

    /**
     * 清屏
     */
    for(i = 3; i < 17; i++)
    {
        m_seg_table[i].seg_data.byte = 0;
        ht162x_write(&m_lcd_display_obj.ht162x, i, m_seg_table[i].seg_data.byte);
    }

    digital_number_show(DIGITAL_2, 0x0);
    digital_number_show(DIGITAL_3, 0xF);
    digital_number_show(DIGITAL_4, 0xF);

    delay_ms(200);
}

/**
 * @brief 清屏显示
 */
static void lcd_clear(void)
{
    ht162x_all_clean(&m_lcd_display_obj.ht162x);
}

/**
 * @brief 显示电池电量level
 */
static int battery_level_show(uint8_t level)
{
    digital_special_show(DIGITAL_T4,true);

    if(level > 0)
    {
        digital_special_show(DIGITAL_T1,true);
    }
    else
    {
        digital_special_show(DIGITAL_T1,false);
    }

    if(level > 1)
    {
        digital_special_show(DIGITAL_T2,true);
    }
    else
    {
        digital_special_show(DIGITAL_T2,false);
    }

    if(level > 2)
    {
        digital_special_show(DIGITAL_T3,true);
    }
    else
    {
        digital_special_show(DIGITAL_T3,false);
    }
}

int lcd_display_init(void)
{
    ht162x_init(&m_lcd_display_obj.ht162x);

    ht162x_all_clean(&m_lcd_display_obj.ht162x);

    gpio_config(&m_lcd_display_obj.lcd_back_light_pin);
    gpio_output_set(&m_lcd_display_obj.lcd_back_light_pin, 1);

    return 0;
}

void lcd_display_loop_task(void)
{
    /**
     * 亮灯持续 LCD_BACK_LED_CONTINUE_TIME
     */
    if(mid_timer_ticks_get() - lcd_bk_old_ticks > LCD_BACK_LED_CONTINUE_TIME)
    {
        lcd_black_light_enable(false);
    }

    /**
     * 开关机的切换
     */
    if(m_old_lcd_off != m_lcd_off)
    {
        if(m_lcd_off)
        {
            battery_level_show(m_battery_level);
            channel_freq_show(m_lcd_channel_freq);

            digital_special_show(DIGITAL_T9,true);
            digital_special_show(DIGITAL_T5,true);
        }
        else
        {
            channel_off_show();
            delay_ms(500);
            lcd_clear();
        }
        m_old_lcd_off = m_lcd_off;
    }

    if(is_lcd_channel_freq_refresh)
    {
        channel_freq_show(m_lcd_channel_freq);
        is_lcd_channel_freq_refresh = false;
    }

    if(is_battery_level_refresh)
    {
        battery_level_show(m_battery_level);
        is_battery_level_refresh = false;
    }
}

void lcd_black_light_enable(bool enable)
{
    if(enable)
    {
        gpio_output_set(&m_lcd_display_obj.lcd_back_light_pin, 0);

        lcd_bk_old_ticks = mid_timer_ticks_get();
    }
    else
    {
        gpio_output_set(&m_lcd_display_obj.lcd_back_light_pin, 1);
    }
}

void lcd_off_status_set(bool enable)
{
    m_lcd_off = enable;
}

void lcd_channel_freq_set(uint16_t freq)
{
    m_lcd_channel_freq = freq;
    is_lcd_channel_freq_refresh = true;
}

void lcd_battery_level_set(uint8_t level)
{
    m_battery_level = level;
    is_battery_level_refresh = true;
}
