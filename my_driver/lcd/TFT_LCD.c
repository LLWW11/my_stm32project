#include "TFT_LCD.h"
#include "SPI.h"
#include "TFT_Img.h"
#include "dma.h"
#include "stdbool.h"
#include "stdlib.h"
#include "string.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
/*
硬件连接：
SDA：PA7   	MOSI数据线
SCL：PA5	SCK时钟线
RST：PD7	复位数据线,在正常操作过程中，保持拉高
DC： PD6	数据/控制线：1表示写入数据，0表示写入寄存器
CS： PA4	片选，低电平有效
BL： PD13	背光控制pin，拉高时打开背光，当被拉低时关闭背光
最大时钟频率为16.66Mhz
*/
#define SPI_DC_0  GPIO_ResetBits(GPIOD, GPIO_Pin_6)
#define SPI_DC_1  GPIO_SetBits(GPIOD, GPIO_Pin_6)
#define SPI_SCK_1 GPIO_ResetBits(GPIOA, GPIO_Pin_5)
#define SPI_SCK_0 GPIO_SetBits(GPIOA, GPIO_Pin_5)
#define SPI_RST_0 GPIO_ResetBits(GPIOD, GPIO_Pin_7)
#define SPI_RST_1 GPIO_SetBits(GPIOD, GPIO_Pin_7)
#define SPI_CS_0  GPIO_ResetBits(GPIOA, GPIO_Pin_4)
#define SPI_CS_1  GPIO_SetBits(GPIOA, GPIO_Pin_4)
#define SPI_BL_1  GPIO_SetBits(GPIOD, GPIO_Pin_13)
#define SPI_BL_0  GPIO_ResetBits(GPIOD, GPIO_Pin_13)

static SemaphoreHandle_t write_gram_Semphore;

void TFT_SEND_CMD(uint8_t o_command)
{
    SPI_CS_0;
    SPI_DC_0;
    // SPI_Cmd(SPI1, DISABLE); 
    SPI_DataSizeConfig(SPI1, SPI_DataSize_8b);
    // SPI_Cmd(SPI1, ENABLE);  
    SPI1_SendByte(o_command);
    SPI_CS_1; // 每次发送完数据之后片选拉高
}
void TFT_SEND_DATA(uint8_t o_data)
{
    SPI_CS_0;
    SPI_DC_1;
    // SPI_Cmd(SPI1, DISABLE); 
    SPI_DataSizeConfig(SPI1, SPI_DataSize_8b);
    // SPI_Cmd(SPI1, ENABLE);  
    SPI1_SendByte(o_data);
    SPI_CS_1;
}
void ST7789_write_reg(uint8_t reg, uint8_t dat)
{
    TFT_SEND_CMD(reg);
    TFT_SEND_DATA(dat);
}
void ST7789_write_reg_MultiData(uint8_t reg, uint8_t *dat, uint8_t len)
{
    TFT_SEND_CMD(reg);
    for (uint8_t i = 0; i < len; i++)
        TFT_SEND_DATA(*(dat + i)); // 软件写st7789的寄存器
}

void st7789_write_gram_DMA(uint8_t data[], uint32_t length, bool singleColor)
{
    SPI_DataSizeConfig(SPI1, SPI_DataSize_16b);
    SPI_CS_0;
    SPI_DC_1; //
    length >>= 1;
    xSemaphoreTake(write_gram_Semphore, 0);
    do
    {
        uint32_t chunk_size = (length < 65535) ? length : 65535;
        DMA_Cmd(DMA2_Stream5, DISABLE);
        if (singleColor)
            DMA2_Stream5->CR &= ~DMA_SxCR_MINC; // 单一颜色刷屏关闭地址自增
        else
            DMA2_Stream5->CR |= DMA_SxCR_MINC;
        DMA2_Stream5->M0AR = (uint32_t)data;
        DMA2_Stream5->NDTR = chunk_size;

        DMA_ClearFlag(DMA2_Stream5, DMA_FLAG_TCIF5);
        DMA_Cmd(DMA2_Stream5, ENABLE);
        xSemaphoreTake(write_gram_Semphore, portMAX_DELAY);

        if (!singleColor)
            data += chunk_size * 2;
        length -= chunk_size;
    } while (length > 0);
    while (SPI_GetFlagStatus(SPI1, SPI_FLAG_BSY) != RESET)
        ;
    SPI_CS_1; //
}
void TFT_clear(void)
{
    unsigned int ROW, column;
    TFT_SEND_CMD(0x2a);  // Column address set
    TFT_SEND_DATA(0x00); // start column
    TFT_SEND_DATA(0x00);
    TFT_SEND_DATA(0x00); // end column
    TFT_SEND_DATA(0xF0);

    TFT_SEND_CMD(0x2b);  // Row address set
    TFT_SEND_DATA(0x00); // start row
    TFT_SEND_DATA(0x00);
    TFT_SEND_DATA(0x01); // end row
    TFT_SEND_DATA(0x40);
    TFT_SEND_CMD(0x2C);                         // Memory write
    for (ROW = 0; ROW < TFT_LINE_NUMBER; ROW++) // ROW loop
    {
        for (column = 0; column < TFT_COLUMN_NUMBER; column++) // column loop
        {
            TFT_SEND_DATA(0xFF);
            TFT_SEND_DATA(0xFF);
        }
    }
}
void TFT_full(uint16_t color)
{
    unsigned int ROW, column;

    TFT_SEND_CMD(0x2a);  // Column address set
    TFT_SEND_DATA(0x00); // start column
    TFT_SEND_DATA(0x00);
    TFT_SEND_DATA(0x00); // end column
    TFT_SEND_DATA(0xEF);

    TFT_SEND_CMD(0x2b);  // Row address set
    TFT_SEND_DATA(0x00); // start row
    TFT_SEND_DATA(0x00);
    TFT_SEND_DATA(0x01); // end row
    TFT_SEND_DATA(0x3F);
    TFT_SEND_CMD(0x2C);                         // Memory write
    for (ROW = 0; ROW < TFT_LINE_NUMBER; ROW++) // ROW loop
    {
        for (column = 0; column < TFT_COLUMN_NUMBER; column++) // column loop
        {
            TFT_SEND_DATA(color >> 8);
            TFT_SEND_DATA(color);
        }
    }
}
void TFT_Reset(void)
{
    SPI_RST_0;
    vTaskDelay(pdMS_TO_TICKS(1)); // 至少20us
    SPI_RST_1;
    vTaskDelay(pdMS_TO_TICKS(120)); // 见手册
}
void TFT_init(void)
{
    SPI_CS_0;
    SPI1_Init();
    SPI_DMA_init();
    SPI_BL_1;
    SPI_SCK_1;
    TFT_Reset();

    write_gram_Semphore = xSemaphoreCreateBinary();
    configASSERT(write_gram_Semphore);

    TFT_SEND_CMD(0x01); // Software Reset
    vTaskDelay(pdMS_TO_TICKS(120));

    TFT_SEND_CMD(0x11);           // Sleep Out
    vTaskDelay(pdMS_TO_TICKS(6)); //

    //-----------------------ST7789V Frame rate setting-----------------//
    ST7789_write_reg(0x3A, 0x05); // 65k mode
    ST7789_write_reg(0xC5, 0x1A); // VCOM
    ST7789_write_reg(0x36, 0x00); // 屏幕显示方向设置

    //-------------ST7789V Frame rate setting-----------//
    uint8_t Frame_rate_data[] = {0x05, 0x05, 0x00, 0x33, 0x33};
    ST7789_write_reg_MultiData(0xb2, Frame_rate_data,
                               sizeof(Frame_rate_data) / sizeof(uint8_t));

    ST7789_write_reg(0xb7, 0x05); // Gate Control 12.2v   -10.43v
    //--------------ST7789V Power setting---------------//
    ST7789_write_reg(0xBB, 0x3F); // VCOM
    ST7789_write_reg(0xC0, 0x2c); // Power control
    ST7789_write_reg(0xc2, 0x01); // VDV and VRH Command Enable
    ST7789_write_reg(0xc3, 0x0f); // VRH Set// 4.3+( vcom+vcom offset+vdv)
    ST7789_write_reg(0xC4, 0x20); // VDV Set 0v
    ST7789_write_reg(0xc6, 0x01); // Frame Rate Control in Normal Mode 111hz
    ST7789_write_reg(0xd0, 0xa4); // Power Control 1
    ST7789_write_reg(0xd0, 0xa1);
    ST7789_write_reg(0xe8, 0x03); // Power Control 1
    ST7789_write_reg(0xe9, 0x09); // Equalize time control
    ST7789_write_reg(0xe9, 0x09);
    ST7789_write_reg(0xe9, 0x08);
    //---------------ST7789V gamma setting-------------//
    uint8_t reg_list1[] = {0xd0, 0x05, 0x09, 0x09, 0x08, 0x14, 0x28,
                           0x33, 0x3F, 0x07, 0x13, 0x14, 0x28, 0x30};
    ST7789_write_reg_MultiData(
        0xe0, reg_list1, sizeof(reg_list1) / sizeof(uint8_t)); // Set Gamma

    uint8_t reg_list2[] = {0xd0, 0x05, 0x09, 0x09, 0x08, 0x03, 0x24,
                           0x32, 0x32, 0x3B, 0x14, 0x13, 0x28, 0x2F};
    ST7789_write_reg_MultiData(
        0xe1, reg_list2, sizeof(reg_list2) / sizeof(uint8_t)); // Set Gamma

    TFT_SEND_CMD(0x20); // 反显
    vTaskDelay(pdMS_TO_TICKS(120));
    TFT_SEND_CMD(0x29); // 开启显示
    TFT_full(BLACK);
}

/**
 * @brief:      短一点的为x轴,长一点的为y轴,排针那块的左上角为(0,0)
 * @param:      起始X, 起始Y, 结束X, 结束Y, 颜色
 * @retval:     none
 */
void TFT_SetWindow(uint16_t sx,
                   uint16_t sy,
                   uint16_t ex,
                   uint16_t ey,
                   uint16_t color)
{
    // Column Address Set
    uint8_t colum_addr_set[] = {sx >> 8, sx & 0xff, ex >> 8, ex & 0xff};
    ST7789_write_reg_MultiData(0x2a, colum_addr_set,
                               sizeof(colum_addr_set) / sizeof(uint8_t));

    // Row Address Set
    uint8_t row_addr_set[] = {sy >> 8, sy & 0xff, ey >> 8, ey & 0xff};
    ST7789_write_reg_MultiData(0x2b, row_addr_set,
                               sizeof(row_addr_set) /
                                   sizeof(uint8_t)); // Set Gamma
    TFT_SEND_CMD(0x2C);
    uint32_t total_pixels = (ex - sx + 1) * (ey - sy + 1) * 2;
    st7789_write_gram_DMA((uint8_t *)&color, total_pixels, true);
}
static bool in_screen_range(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    if (x1 >= TFT_COLUMN_NUMBER || y1 >= TFT_LINE_NUMBER)
        return false;
    if (x2 >= TFT_COLUMN_NUMBER || y2 >= TFT_LINE_NUMBER)
        return false;
    if (x1 > x2 || y1 > y2)
        return false;

    return true;
}
void TFT_LCD_show_img(uint16_t x, uint16_t y, const img_t *img)
{
    if (x >= TFT_COLUMN_NUMBER || y >= TFT_LINE_NUMBER ||
        x + img->width - 1 >= TFT_COLUMN_NUMBER ||
        y + img->height - 1 >= TFT_LINE_NUMBER)
        return;
    // 计算图片结束的坐标
    uint16_t ex = x + img->width - 1;
    uint16_t ey = y + img->height - 1;

    uint8_t colum_addr_set[] = {x >> 8, x & 0xff, ex >> 8, ex & 0xff};
    ST7789_write_reg_MultiData(0x2a, colum_addr_set, 4);
    uint8_t row_addr_set[] = {y >> 8, y & 0xff, ey >> 8, ey & 0xff};
    ST7789_write_reg_MultiData(0x2b, row_addr_set, 4);

    TFT_SEND_CMD(0x2C);
    uint32_t size = img->height * img->width * 2;

    st7789_write_gram_DMA((uint8_t *)img->data, size, false);
}
void st7789_draw_font(uint16_t x,
                      uint16_t y,
                      uint16_t width,
                      uint16_t height,
                      const uint8_t *model,
                      uint16_t color_ch,
                      uint16_t color_bg)
{
    uint16_t bytes_per_row = (width + 7) / 8;

    static uint8_t buff[72 * 72 * 2]; // 最大的字符数组长度
    uint8_t *pbuf = buff;
    for (uint16_t row = 0; row < height; row++)
    {
        const uint8_t *row_data = model + row * bytes_per_row;
        for (uint16_t col = 0; col < width; col++)
        {
            uint8_t pixel = row_data[col / 8] & (1 << (7 - col % 8));
            uint16_t pixel_color = pixel ? color_ch : color_bg;
            *pbuf++ = pixel_color & 0xff;
            *pbuf++ = (pixel_color >> 8) & 0xff;
        }
    }
    TFT_SetWindow(x, y, x + width - 1, y + height - 1, color_bg);
    st7789_write_gram_DMA(buff, pbuf - buff, false);
}

// 使用DMA绘制字符的方法
void st7789_write_single_ascii(uint16_t x, // 起始x坐标
                               uint16_t y, // 起始Y坐标
                               char ch,    // 要显示的字符
                               const font_t *font,
                               uint16_t color_bg,
                               uint16_t color_ch)
{
    if (font == NULL)
        return;
    uint16_t fheight = font->height, fwidth = font->height / 2;
    if (ch < 0x20 || ch > 0x7E)
        return;
    if (!in_screen_range(x, y, x + fwidth - 1, y + fheight - 1))
        return;

    uint32_t char_index = 0;
    uint16_t bytes_perRow = (fwidth + 7) / 8;
    if (font->height == 80 || font->height == 54)
    {
        if (ch >= '0' && ch <= '9')
            char_index = ch - '0';
        if (font->height == 80)
        {
            switch (ch)
            {
            case ':':
                char_index = 10;
                break;
            case '-':
                char_index = 11;
                break;
            case ' ':
                char_index = 12;
                break;
            default:
                break;
            }
        }
        if (font->height == 54)
        {
            switch (ch)
            {
            case '-':
                char_index = 10;
                break;
            case '%':
                char_index = 11;
                break;
            case ' ':
                char_index = 12;
                break;
            default:
                break;
            }
        }
    }
    else
        char_index = ch - ' ';

    const uint8_t *p_data =
        font->ascii_model + char_index * (font->height * bytes_perRow);
    st7789_draw_font(x, y, fwidth, fheight, p_data, color_ch, color_bg);
}

void st7789_rite_single_Chinese(uint16_t x,
                                uint16_t y,
                                char *ch,
                                const font_t *font,
                                uint16_t color_bg,
                                uint16_t color_ch)
{
    if (font == NULL || ch == NULL)
        return;
    uint16_t fheight = font->height, fwidth = font->height;
    // uint16_t bytes_perRow = (fwidth + 7) / 8;

    const Ch_font_t *c = font->chinese;
    if (!in_screen_range(x, y, x + fwidth - 1, y + fheight - 1))
        return;
    while (c->name != NULL)
    {
        if (strcmp(c->name, ch) == 0)
            break;
        c++;
    }
    if (c->name == NULL)
        return;

    // const uint8_t *p_data = c->model;
    st7789_draw_font(x, y, fwidth, fheight, c->model, color_ch, color_bg);
}

/// 不用DMA的方法绘制字符串
void TFT_LCD_Write_single_ASCII(uint16_t x, // 起始x坐标
                                uint16_t y, // 起始Y坐标
                                char ch,    // 要显示的字符
                                const font_t *font,
                                uint16_t color_bg,
                                uint16_t color_ch)
{
    uint32_t i, j, k;
    uint8_t temp;
    uint16_t ey = y + font->height - 1;
    uint8_t width = (font->height) / 2;
    uint16_t ex = x + width - 1;
    uint16_t bytes_perRow = (width + 7) / 8;
    if (!in_screen_range(x, y, ex, ey))
        return;

    uint32_t char_index = 0;
    if (font->height == 80 || font->height == 54)
    {
        if (ch >= '0' && ch <= '9')
            char_index = ch - '0';
        if (font->height == 80)
        {
            switch (ch)
            {
            case ':':
                char_index = 10;
                break;
            case '-':
                char_index = 11;
                break;
            case ' ':
                char_index = 12;
                break;
            default:
                break;
            }
        }
        if (font->height == 54)
        {
            switch (ch)
            {
            case '-':
                char_index = 10;
                break;
            case '%':
                char_index = 11;
                break;
            case ' ':
                char_index = 12;
                break;
            default:
                break;
            }
        }
    }
    else
    {
        char_index = ch - ' ';
    }

    const uint8_t *p_data =
        font->ascii_model + char_index * (font->height * bytes_perRow);

    TFT_SetWindow(x, y, ex, ey, color_bg);
    TFT_SEND_CMD(0x2C);
    for (i = 0; i < font->height; i++) // 遍历高度
    {
        uint8_t pixel_cnt = 0;
        for (j = 0; j < bytes_perRow; j++)
        {
            temp = *p_data++;
            for (k = 0; k < 8; k++)
            {
                if (pixel_cnt < width)
                {                                 // 判断当前位是0还是1
                    if ((temp >> (7 - k)) & 0x01) // 高位在前
                    {
                        TFT_SEND_DATA(color_ch >> 8);
                        TFT_SEND_DATA(color_ch & 0xFF);
                    }
                    else
                    {
                        TFT_SEND_DATA(color_bg >> 8);
                        TFT_SEND_DATA(color_bg & 0xFF);
                    }
                    pixel_cnt++;
                }
            }
        }
    }
}

void TFT_LCD_Write_single_Chinese(uint16_t x,
                                  uint16_t y,
                                  char *ch,
                                  const font_t *font,
                                  uint16_t color_bg,
                                  uint16_t color_ch)
{
    if (font == NULL || ch == NULL)
        return;
    uint16_t fheight = font->height;
    uint16_t fwidth = font->height;
    uint16_t bytes_perRow = (fwidth + 7) / 8;
    uint32_t i, j, k;
    uint8_t temp;
    uint16_t ey = y + fheight - 1;
    uint16_t ex = x + fwidth - 1;
    const Ch_font_t *c = font->chinese;
    if (!in_screen_range(x, y, x + fwidth - 1, y + fheight - 1))
        return;
    while (c->name != NULL)
    {
        if (strcmp(c->name, ch) == 0)
            break;
        c++;
    }
    if (c->name == NULL)
        return;

    const uint8_t *p_data = c->model;

    TFT_SetWindow(x, y, ex, ey, color_bg);

    TFT_SEND_CMD(0x2C);
    for (i = 0; i < font->height; i++)
    {
        uint16_t pixel_cnt = 0; //
        for (j = 0; j < bytes_perRow; j++)
        {
            temp = *p_data++;
            for (k = 0; k < 8; k++)
            {
                if (pixel_cnt < fwidth)
                {
                    if ((temp >> (7 - k)) & 0x01)
                    {
                        TFT_SEND_DATA(color_ch >> 8);
                        TFT_SEND_DATA(color_ch & 0xFF);
                    }
                    else
                    {
                        TFT_SEND_DATA(color_bg >> 8);
                        TFT_SEND_DATA(color_bg & 0xFF);
                    }
                    pixel_cnt++;
                }
            }
        }
    }
}
void TFT_LCD_Write_string_ASCII(uint16_t x,
                                uint16_t y,
                                char *str,
                                const font_t *font,
                                uint16_t color_bg,
                                uint16_t color_ch)
{
    uint8_t i = 0;

    while (*(str + i) != '\0')
    {
        if (x <= TFT_COLUMN_NUMBER)
            TFT_LCD_Write_single_ASCII(x, y, *(str + i), font, color_bg,
                                       color_ch);
        else
            break;
        x += font->height / 2;
        i++;
    }
}
static bool is_gb2312(char ch)
{
    return ((unsigned char)ch >= 0xA1 && (unsigned char)ch <= 0xF7);
}
void TFT_LCD_Write_Chinese_String(uint16_t x,
                                  uint16_t y,
                                  char *str,
                                  const font_t *font,
                                  uint16_t color_bg,
                                  uint16_t color_ch)
{
    uint8_t i = 0;

    while (*(str + i) != '\0')
    {
        char ch[5];
        strncpy(ch, str + i, 2);
        ch[2] = '\0';
        if (x <= TFT_COLUMN_NUMBER)
            TFT_LCD_Write_single_Chinese(x, y, ch, font, color_bg, color_ch);
        else
            break;
        x += font->height;
        i += 2;
    }
}
/*混有ASCII和汉字的字符串
 */
void TFT_LCD_Write_String(uint16_t x,
                          uint16_t y,
                          char *str,
                          const font_t *font,
                          uint16_t color_bg,
                          uint16_t color_ch)
{
    uint8_t i = 0;
    while (*(str + i) != '\0')
    {
        int len = is_gb2312(*(str + i)) ? 2 : 1;
        if (len <= 0)
            continue;
        else if (len == 1)
        {
            st7789_write_single_ascii(x, y, *(str + i), font, color_bg,
                                      color_ch);
            x += font->height / 2;
        }
        else
        {
            char ch[5];
            strncpy(ch, str + i, len);
            ch[len] = '\0';
            st7789_rite_single_Chinese(x, y, ch, font, color_bg, color_ch);
            // TFT_LCD_Write_single_Chinese(x, y, ch, font, color_bg, color_ch);
            x += font->height;
        }
        i += len;
    }
}

void DMA2_Stream5_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA2_Stream5, DMA_IT_TCIF5) == SET)
    {
        DMA_ClearITPendingBit(DMA2_Stream5, DMA_IT_TCIF5);
        BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(write_gram_Semphore, &pxHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
    }
}
