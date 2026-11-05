#ifndef GD30AD3344_H
#define GD30AD3344_H

#include "HeaderFiles.h"

/*
    ============================================================
    GD30AD3344 user configuration
    ============================================================
*/

/*
    AIN3 external reference enable

    1: 启用 AIN3 外部基准，AIN3 必须接稳定 2.5V 基准源
    0: 不启用 AIN3 外部基准，使用芯片内部基准

    注意：
    本工程固定 AIN0 单端输入。
    当 AD3344_USE_AIN3_EXT_REF = 1 时，不要再把 AIN3 当普通模拟输入。
*/
#define AD3344_USE_AIN3_EXT_REF          1

/*
    PT100 ADC configuration:
    - AIN0 single-ended input
    - PGA = +/-2.048V
    - Data rate = 50SPS
    - Single-shot mode
    - Pull-up enabled
    - NOP valid, write config
*/
#define AD3344_PT100_CONFIG              (AD3344_REG_CONFIG_OS_SINGLE       | \
                                          AD3344_REG_CONFIG_MUX_SINGLE_0    | \
                                          AD3344_REG_CONFIG_PGA_2_048V      | \
                                          AD3344_REG_CONFIG_MODE_SINGLE     | \
                                          AD3344_REG_CONFIG_DR_50SPS        | \
                                          AD3344_REG_CONFIG_PULL_UP_EN      | \
                                          AD3344_REG_CONFIG_NOP_VALID)

/*
    ADC conversion parameters
*/
#define PT100_ADC_PGA_RANGE_V            2.048f
#define PT100_ADC_FULL_SCALE             32768.0f

/*
    ============================================================
    GD30AD3344 Config Register bit definitions
    ============================================================
*/

/* OS bit */
#define AD3344_REG_CONFIG_OS_SINGLE      0x8000



/* MODE */
#define AD3344_REG_CONFIG_MODE_CONTIN    0x0000
#define AD3344_REG_CONFIG_MODE_SINGLE    0x0100

/* DR[2:0] */
#define AD3344_REG_CONFIG_DR_6_25SPS     0x0000
#define AD3344_REG_CONFIG_DR_12_5SPS     0x0020
#define AD3344_REG_CONFIG_DR_25SPS       0x0040
#define AD3344_REG_CONFIG_DR_50SPS       0x0060
#define AD3344_REG_CONFIG_DR_100SPS      0x0080
#define AD3344_REG_CONFIG_DR_250SPS      0x00A0
#define AD3344_REG_CONFIG_DR_500SPS      0x00C0
#define AD3344_REG_CONFIG_DR_1000SPS     0x00E0

/* MUX[2:0] */
#define AD3344_REG_CONFIG_MUX_DIFF_0_1   0x0000
#define AD3344_REG_CONFIG_MUX_DIFF_0_3   0x1000
#define AD3344_REG_CONFIG_MUX_DIFF_1_3   0x2000
#define AD3344_REG_CONFIG_MUX_DIFF_2_3   0x3000
#define AD3344_REG_CONFIG_MUX_SINGLE_0   0x4000
#define AD3344_REG_CONFIG_MUX_SINGLE_1   0x5000
#define AD334_REG_CONFIG_MUX_SINGLE_2    0x6000
#define AD3344_REG_CONFIG_MUX_SINGLE_2   0x6000
#define AD3344_REG_CONFIG_MUX_SINGLE_3   0x7000

/* PGA[2:0] */
#define AD3344_REG_CONFIG_PGA_6_144V     0x0000
#define AD3344_REG_CONFIG_PGA_4_096V     0x0200
#define AD3344_REG_CONFIG_PGA_2_048V     0x0400
#define AD3344_REG_CONFIG_PGA_1_024V     0x0600
#define AD3344_REG_CONFIG_PGA_0_512V     0x0800
#define AD3344_REG_CONFIG_PGA_0_256V     0x0A00
#define AD3344_REG_CONFIG_PGA_0_064V     0x0C00
#define AD3344_REG_CONFIG_PGA_MASK       0x0E00

/* PULL_UP_EN */
#define AD3344_REG_CONFIG_PULL_UP_EN     0x0008

/* NOP[1:0] = 01 valid */
#define AD3344_REG_CONFIG_NOP_VALID      0x0002

/*
    ============================================================
    SPI3 / GPIO interface
    ============================================================

    GD32F470VET6:
    PE12 = SPI3_SCK
    PE13 = SPI3_MISO
    PE14 = SPI3_MOSI
    PE9  = CS
*/

#define AD3344_SPI                       SPI3
#define AD3344_SPI_CLK                   RCU_SPI3

#define AD3344_GPIO_PORT                 GPIOE
#define AD3344_GPIO_CLK                  RCU_GPIOE
#define AD3344_GPIO_AF                   GPIO_AF_5

#define AD3344_SCK_PIN                   GPIO_PIN_12
#define AD3344_MISO_PIN                  GPIO_PIN_13
#define AD3344_MOSI_PIN                  GPIO_PIN_14

#define AD3344_CS_PORT                   GPIOE
#define AD3344_CS_CLK                    RCU_GPIOE
#define AD3344_CS_PIN                    GPIO_PIN_9

#define AD3344_CS_LOW()                  gpio_bit_reset(AD3344_CS_PORT, AD3344_CS_PIN)
#define AD3344_CS_HIGH()                 gpio_bit_set(AD3344_CS_PORT, AD3344_CS_PIN)

/*
    ============================================================
    External variables
    ============================================================
*/

extern uint16_t AD3344_CONFIG;

/*
    ============================================================
    Function declarations
    ============================================================
*/

void AD3344_SPI_Init(void);
uint16_t AD3344_SPI_SendHalfWord(uint16_t data);

void ad3344_reset(void);
void ad3344_init(uint16_t config);
uint16_t ad3344_read_data16(uint16_t config);
uint16_t ad3344_read_data32(uint16_t config_d, uint16_t *config_readback);

/*
    Official template style external reference functions
*/
void ad3344_process(void);
void ad3344_ExtRef(void);

#endif
