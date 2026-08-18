#include "stm32f10x.h"
#include "si2c.h"
#include "oled.h"
#include "delay.h"
#include "cat_frames.h"

SI2C_TypeDef si2c;
OLED_TypeDef oled;

void My_SoftwareI2C_Init(void);
void My_OLEDScreen_Init(void);
int i2c_write_bytes(uint8_t addr, const uint8_t *pdata, uint16_t size);

int main(void)
{
    My_SoftwareI2C_Init();
    My_OLEDScreen_Init();
    OLED_SetCursor(&oled, (128U - CAT_FRAME_WIDTH) / 2U, 0);

    while(1)
    {
        for(uint32_t i = 0; i < CAT_FRAME_COUNT; i++)
        {
            OLED_Clear(&oled);
            OLED_DrawBitmap(&oled, CAT_FRAME_WIDTH, CAT_FRAME_HEIGHT, cat_frames[i]);
            OLED_SendBuffer(&oled);
            Delay(10);
        }
    }
}

void My_SoftwareI2C_Init(void)
{
    si2c.SCL_GPIOx = GPIOB;
    si2c.SCL_GPIO_Pin = GPIO_Pin_6;
    si2c.SDA_GPIOx = GPIOB;
    si2c.SDA_GPIO_Pin = GPIO_Pin_7;

    My_SI2C_Init(&si2c);
}

int i2c_write_bytes(uint8_t addr, const uint8_t *pdata, uint16_t size)
{
    return My_SI2C_SendBytes(&si2c, addr, pdata, size);
}

void My_OLEDScreen_Init(void)
{
    OLED_InitTypeDef OLED_InitStruct;

    OLED_InitStruct.i2c_write_cb = i2c_write_bytes;

    OLED_Init(&oled, &OLED_InitStruct);
}
