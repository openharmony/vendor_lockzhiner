/*
 * Copyright (c) 2022 FuZhou Lockzhiner Electronic Co., Ltd. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "e53_intelligent_agriculture.h"
#include "lz_hardware.h"

/* i2c的设备编号 */
#define IA_I2C0                             0
/* i2c的通信速率 */
#define I2C_RATE                            100000
/* BH1750的i2c从设备地址 */
#define BH1750_ADDR                         0x23
/* sht30的i2c从设备地址 */
#define SHT30_ADDR                          0x44

static I2cBusIo m_ia_i2c0m2 = {
    .scl =  {
        .gpio = GPIO0_PA1,
        .func = MUX_FUNC3,
        .type = PULL_NONE,
        .drv = DRIVE_KEEP,
        .dir = LZGPIO_DIR_KEEP,
        .val = LZGPIO_LEVEL_KEEP
    },
    .sda =  {
        .gpio = GPIO0_PA0,
        .func = MUX_FUNC3,
        .type = PULL_NONE,
        .drv = DRIVE_KEEP,
        .dir = LZGPIO_DIR_KEEP,
        .val = LZGPIO_LEVEL_KEEP
    },
    .id = FUNC_ID_I2C0,
    .mode = FUNC_MODE_M2,
};

/* 读取SHT30的SHT30_ADDR寄存器位移定义 */
enum enum_offset_sht30_reg {
    EOFFSET_SHT30_REG_TEMP_H = 0,       /* 温度寄存器数值高位 */
    EOFFSET_SHT30_REG_TEMP_L,           /* 温度寄存器数值低位 */
    EOFFSET_SHT30_REG_TEMP_CRC,         /* 温度寄存器数值校验位 */
    EOFFSET_SHT30_REG_HUMIDITY_H,       /* 湿度寄存器数值高位 */
    EOFFSET_SHT30_REG_HUMIDITY_L,       /* 湿度寄存器数值低位 */
    EOFFSET_SHT30_REG_HUMIDITY_CRC,     /* 湿度寄存器数值校验位 */
    EOFFSET_SHT30_REG_MAX
};

/***************************************************************
* 函数名称: init_sht30
* 说    明: 初始化SHT30，设置测量周期
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void init_sht30(void)
{
    uint8_t send_data[2] = {0x22, 0x36};
    uint32_t send_len = 2;

    LzI2cWrite(IA_I2C0, SHT30_ADDR, send_data, send_len);
}

/***************************************************************
* 函数名称: init_bh1750
* 说    明: 写命令初始化BH1750
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void init_bh1750(void)
{
    uint8_t send_data[1] = {0x01};
    uint32_t send_len = 1;

    LzI2cWrite(IA_I2C0, BH1750_ADDR, send_data, send_len);
}

/***************************************************************
* 函数名称: start_bh1750
* 说    明: 启动BH1750
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void start_bh1750(void)
{
    uint8_t send_data[1] = {0x10};
    uint32_t send_len = 1;

    LzI2cWrite(IA_I2C0, BH1750_ADDR, send_data, send_len);
}

/***************************************************************
* 函数名称: sht30_calc_RH
* 说    明: 湿度计算
* 参    数: u16sRH：读取到的湿度原始数据
* 返 回 值: 计算后的湿度数据
***************************************************************/
float sht30_calc_RH(uint16_t u16sRH)
{
    float threshold = 100.0;
    float range = 65535.0;
    float humidityRH = 0;

    /* clear bits [1..0] (status bits) */
    u16sRH &= ~0x0003;
    /* calculate relative humidity [%RH] */
    /* RH = rawValue / (2^16-1) * 10 */
    humidityRH = (threshold * (float)u16sRH / range);

    return humidityRH;
}

/***************************************************************
* 函数名称: sht30_calc_temperature
* 说    明: 温度计算
* 参    数: u16sT：读取到的温度原始数据
* 返 回 值: 计算后的温度数据
***************************************************************/
float sht30_calc_temperature(uint16_t u16sT)
{
    float threshold = 175.0;
    float range = 65535.0;
    float adjust = 45.0;
    float temperature = 0;

    /* clear bits [1..0] (status bits) */
    u16sT &= ~0x0003;
    /* calculate temperature [℃] */
    /* T = -45 + 175 * rawValue / (2^16-1) */
    temperature = (threshold * (float)u16sT / range - adjust);

    return temperature;
}

/***************************************************************
* 函数名称: sht30_check_crc
* 说    明: 检查数据正确性
* 参    数: data：读取到的数据
            nbrOfBytes：需要校验的数量
            checksum：读取到的校对比验值
* 返 回 值: 校验结果，0-成功 1-失败
***************************************************************/
uint8_t sht30_check_crc(uint8_t *data, uint8_t nbrOfBytes, uint8_t checksum)
{
    uint8_t byte_to_bits = 8;
    uint8_t high_bit = 0x80;
    uint8_t crc = 0xFF;
    uint8_t bit = 0;
    uint8_t byteCtr ;
    const int16_t POLYNOMIAL = 0x131;

    /* calculates 8-Bit checksum with given polynomial */
    for (byteCtr = 0; byteCtr < nbrOfBytes; ++byteCtr) {
        crc ^= (data[byteCtr]);
        for (bit = byte_to_bits; bit > 0; --bit) {
            if (crc & high_bit) {
                crc = (crc << 1) ^ POLYNOMIAL;
            } else {
                crc = (crc << 1);
            }
        }
    }

    if (crc != checksum) {
        return 1;
    } else {
        return 0;
    }
}

/***************************************************************
* 函数名称: e53_ia_io_init
* 说    明: E53_IA模块IO初始化
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void e53_ia_io_init(void)
{
    uint32_t ret = LZ_HARDWARE_FAILURE;

    /* 初始化紫光灯GPIO */
    LzGpioInit(GPIO0_PA2);
    /* 初始化电机GPIO */
    LzGpioInit(GPIO1_PD0);

    /* 设置GPIO0_PA2为输出模式 */
    ret = LzGpioSetDir(GPIO0_PA2, LZGPIO_DIR_OUT);
    if (ret != LZ_HARDWARE_SUCCESS) {
        printf("set GPIO0_PA2 Direction fail\n");
    }

    /* 设置GPIO1_PD0为输出模式 */
    ret = LzGpioSetDir(GPIO1_PD0, LZGPIO_DIR_OUT);
    if (ret != LZ_HARDWARE_SUCCESS) {
        printf("set GPIO0_PD0 Direction fail\n");
    }

    /* 初始化I2C */
    if (I2cIoInit(m_ia_i2c0m2) != LZ_HARDWARE_SUCCESS) {
        printf("init I2C I2C0 io fail\n");
    }
    /* I2C时钟频率100K */
    if (LzI2cInit(IA_I2C0, I2C_RATE) != LZ_HARDWARE_SUCCESS) {
        printf("init I2C I2C0 fail\n");
    }
}

/***************************************************************
* 函数名称: e53_ia_init
* 说    明: 初始化E53_IA
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void e53_ia_init(void)
{
    e53_ia_io_init();
    init_bh1750();
    init_sht30();
}

/***************************************************************
* 函数名称: e53_ia_read_data
* 说    明: 测量光照强度、温度、湿度
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void e53_ia_read_data(e53_ia_data_t *pData)
{
#define SHT30_CRC_STRING_MAXSIZE    3   /* SHT30的CRC字符串长度 */
#define SHT30_CRC_DATA_MAXSIZE      2   /* CRC校验的数据长度 */
#define SHT30_CRC_OFFSET            2   /* CRC校验的数组偏移量 */
    uint32_t wait_start_hb1750 = 180;
    float luminance_rate = 1.2;
    uint16_t high_byte_bit = 8;
    uint8_t recv_data[2] = {0};
    uint32_t receive_len = 2;
    uint8_t rc;

    start_bh1750();
    LOS_Msleep(wait_start_hb1750);

    LzI2cRead(IA_I2C0, BH1750_ADDR, recv_data, receive_len);
    pData->luminance = (float)(((recv_data[0] << high_byte_bit) + recv_data[1]) / luminance_rate);

    /* checksum verification */
    uint8_t data[SHT30_CRC_STRING_MAXSIZE];
    uint16_t tmp;
    /* byte 0,1 is temperature byte 4,5 is humidity */
    uint8_t SHT30_Data_Buffer[EOFFSET_SHT30_REG_MAX];
    memset(SHT30_Data_Buffer, 0, EOFFSET_SHT30_REG_MAX);
    uint8_t send_data[2] = {0xE0, 0x00};
    uint32_t send_len = 2;
    LzI2cWrite(IA_I2C0, SHT30_ADDR, send_data, send_len);
    receive_len = EOFFSET_SHT30_REG_MAX;
    LzI2cRead(IA_I2C0, SHT30_ADDR, SHT30_Data_Buffer, receive_len);

    /* check temperature */
    data[0] = SHT30_Data_Buffer[EOFFSET_SHT30_REG_TEMP_H];
    data[1] = SHT30_Data_Buffer[EOFFSET_SHT30_REG_TEMP_L];
    data[2] = SHT30_Data_Buffer[EOFFSET_SHT30_REG_TEMP_CRC];
    rc = sht30_check_crc(data, SHT30_CRC_DATA_MAXSIZE, data[SHT30_CRC_OFFSET]);
    if (!rc) {
        tmp = ((uint16_t)data[0] << high_byte_bit) | data[1];
        pData->temperature = sht30_calc_temperature(tmp);
    }

    /* check humidity */
    data[0] = SHT30_Data_Buffer[EOFFSET_SHT30_REG_HUMIDITY_H];
    data[1] = SHT30_Data_Buffer[EOFFSET_SHT30_REG_HUMIDITY_L];
    data[2] = SHT30_Data_Buffer[EOFFSET_SHT30_REG_HUMIDITY_CRC];
    rc = sht30_check_crc(data, SHT30_CRC_DATA_MAXSIZE, data[SHT30_CRC_OFFSET]);
    if (!rc) {
        tmp = ((uint16_t)data[0] << high_byte_bit) | data[1];
        pData->humidity = sht30_calc_RH(tmp);
    }
}

/***************************************************************
* 函数名称: light_set
* 说    明: 紫光灯控制
* 参    数:
*          OFF,关
*          ON,开
* 返 回 值: 无
***************************************************************/
void light_set(SWITCH_STATUS_ENUM status)
{
    if (status == ON) {
        /* 设置GPIO0_PA2输出高电平点亮灯 */
        LzGpioSetVal(GPIO0_PA2, LZGPIO_LEVEL_HIGH);
    }
    if (status == OFF) {
        /* 设置GPIO0_PA2输出低电平关闭灯 */
        LzGpioSetVal(GPIO0_PA2, LZGPIO_LEVEL_LOW);
    }
}

/***************************************************************
* 函数名称: motor_set_status
* 说    明: 电机控制
* 参    数:
*          OFF,关
*          ON,开
* 返 回 值: 无
***************************************************************/
void motor_set_status(SWITCH_STATUS_ENUM status)
{
    if (status == ON) {
        /* 设置GPIO0_PD0输出高电平打开电机 */
        LzGpioSetVal(GPIO1_PD0, LZGPIO_LEVEL_HIGH);
    }
    if (status == OFF) {
        /* 设置GPIO0_PD0输出低电平关闭电机 */
        LzGpioSetVal(GPIO1_PD0, LZGPIO_LEVEL_LOW);
    }
}
