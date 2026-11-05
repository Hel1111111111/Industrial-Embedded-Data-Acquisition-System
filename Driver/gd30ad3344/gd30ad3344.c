#include "gd30ad3344.h"

uint16_t AD3344_CONFIG = AD3344_PT100_CONFIG;


#define AD3344_SHORT_DELAY_US     10U
#define AD3344_LONG_DELAY_US      1000U
#define AD3344_RESET_DELAY_MS     30U
#define AD3344_WAKE_DELAY_MS      5U
#define AD3344_CONFIG_DELAY_MS    25U

#define AD3344_CMD_HIDDEN_WRITE   0x8100U
#define AD3344_CMD_HIDDEN_READ    0x8106U
#define AD3344_DUMMY_WORD         0x0000U
#define AD3344_RESET_WORD         0xFFFFU

#define AD3344_HIDDEN_BASE        0x10U
#define AD3344_HIDDEN_PROC_REG    0x02U
#define AD3344_HIDDEN_REF_REG     0x04U
#define AD3344_HIDDEN_PROC_VALUE  0xACCAU
#define AD3344_EXT_REF_ENABLE_BIT 0x0040U


static void AD3344_LocalDelayUs(uint32_t delay_us)
{
    volatile uint32_t loop_count;

    while(delay_us != 0U)
    {
        loop_count = 30U;

        while(loop_count != 0U)
        {
            loop_count--;
        }

        delay_us--;
    }
}


static void AD3344_SetCsLow(uint32_t before_delay_us)
{
    AD3344_CS_LOW();

    if(before_delay_us != 0U)
    {
        AD3344_LocalDelayUs(before_delay_us);
    }
}


static void AD3344_SetCsHigh(uint32_t after_delay_us)
{
    AD3344_CS_HIGH();

    if(after_delay_us != 0U)
    {
        AD3344_LocalDelayUs(after_delay_us);
    }
}


static uint16_t AD3344_Transfer16(uint16_t tx_data)
{
    while(spi_i2s_flag_get(AD3344_SPI, SPI_FLAG_TBE) == RESET)
    {
    }

    spi_i2s_data_transmit(AD3344_SPI, tx_data);

    while(spi_i2s_flag_get(AD3344_SPI, SPI_FLAG_RBNE) == RESET)
    {
    }

    return (uint16_t)spi_i2s_data_receive(AD3344_SPI);
}


static void AD3344_SpiGpioConfig(void)
{
    uint32_t spi_pins;

    spi_pins = AD3344_SCK_PIN | AD3344_MISO_PIN | AD3344_MOSI_PIN;

    rcu_periph_clock_enable(AD3344_GPIO_CLK);
    rcu_periph_clock_enable(AD3344_CS_CLK);

    gpio_af_set(AD3344_GPIO_PORT,
                AD3344_GPIO_AF,
                spi_pins);

    gpio_mode_set(AD3344_GPIO_PORT,
                  GPIO_MODE_AF,
                  GPIO_PUPD_NONE,
                  spi_pins);

    gpio_output_options_set(AD3344_GPIO_PORT,
                            GPIO_OTYPE_PP,
                            GPIO_OSPEED_50MHZ,
                            spi_pins);

    gpio_mode_set(AD3344_CS_PORT,
                  GPIO_MODE_OUTPUT,
                  GPIO_PUPD_PULLUP,
                  AD3344_CS_PIN);

    gpio_output_options_set(AD3344_CS_PORT,
                            GPIO_OTYPE_PP,
                            GPIO_OSPEED_50MHZ,
                            AD3344_CS_PIN);

    AD3344_CS_HIGH();
}


static void AD3344_SpiBusConfig(void)
{
    spi_parameter_struct spi_config;

    rcu_periph_clock_enable(AD3344_SPI_CLK);

    spi_struct_para_init(&spi_config);

    spi_config.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_config.device_mode          = SPI_MASTER;
    spi_config.frame_size           = SPI_FRAMESIZE_16BIT;
    spi_config.clock_polarity_phase = SPI_CK_PL_LOW_PH_2EDGE;
    spi_config.nss                  = SPI_NSS_SOFT;
    spi_config.prescale             = SPI_PSC_128;
    spi_config.endian               = SPI_ENDIAN_MSB;

    spi_init(AD3344_SPI, &spi_config);
    spi_enable(AD3344_SPI);
}


static void AD3344_WriteRegisterSequence(uint16_t address, uint16_t value)
{
    AD3344_SetCsLow(AD3344_LONG_DELAY_US);

    AD3344_Transfer16(AD3344_CMD_HIDDEN_WRITE);
    AD3344_Transfer16(address);
    AD3344_Transfer16(value);

    AD3344_LocalDelayUs(AD3344_LONG_DELAY_US);

    AD3344_SetCsHigh(AD3344_LONG_DELAY_US);
}


static uint16_t AD3344_ReadRegisterSequence(uint16_t address)
{
    uint16_t read_value;

    AD3344_SetCsLow(AD3344_LONG_DELAY_US);

    AD3344_Transfer16(AD3344_CMD_HIDDEN_READ);
    AD3344_LocalDelayUs(AD3344_LONG_DELAY_US);

    AD3344_Transfer16(address);
    AD3344_LocalDelayUs(AD3344_LONG_DELAY_US);

    read_value = AD3344_Transfer16(AD3344_DUMMY_WORD);
    AD3344_LocalDelayUs(AD3344_LONG_DELAY_US);

    AD3344_SetCsHigh(AD3344_LONG_DELAY_US);

    return read_value;
}


static uint16_t AD3344_MakeHiddenAddress(uint16_t offset)
{
    return (uint16_t)(AD3344_HIDDEN_BASE + offset);
}


/*
 * Initialize GPIO pins and SPI peripheral.
 */
void AD3344_SPI_Init(void)
{
    AD3344_SpiGpioConfig();
    AD3344_SpiBusConfig();
}


/*
 * Send one 16-bit word and return the received 16-bit word.
 */
uint16_t AD3344_SPI_SendHalfWord(uint16_t data)
{
    return AD3344_Transfer16(data);
}


/*
 * Reset GD30AD3344 by sending 32 continuous high bits.
 */
void ad3344_reset(void)
{
    AD3344_CS_HIGH();

    delay_1ms(AD3344_RESET_DELAY_MS);

    AD3344_SetCsLow(AD3344_SHORT_DELAY_US);

    AD3344_Transfer16(AD3344_RESET_WORD);
    AD3344_Transfer16(AD3344_RESET_WORD);

    AD3344_SetCsHigh(AD3344_SHORT_DELAY_US);

    delay_1ms(AD3344_WAKE_DELAY_MS);
}


/*
 * Write one configuration word to start or configure conversion.
 */
void ad3344_init(uint16_t config)
{
    AD3344_SetCsLow(AD3344_SHORT_DELAY_US);

    AD3344_Transfer16(config);

    AD3344_SetCsHigh(AD3344_SHORT_DELAY_US);

    delay_1ms(AD3344_CONFIG_DELAY_MS);
}


/*
 * Read one 16-bit ADC result.
 */
uint16_t ad3344_read_data16(uint16_t config)
{
    uint16_t adc_code;

    AD3344_SetCsLow(AD3344_SHORT_DELAY_US);

    adc_code = AD3344_Transfer16(config);

    AD3344_SetCsHigh(AD3344_SHORT_DELAY_US);

    return adc_code;
}


/*
 * Read ADC result and one additional readback word.
 */
uint16_t ad3344_read_data32(uint16_t config_d, uint16_t *config_readback)
{
    uint16_t adc_code;
    uint16_t readback_word;

    AD3344_SetCsLow(AD3344_SHORT_DELAY_US);

    adc_code = AD3344_Transfer16(config_d);
    readback_word = AD3344_Transfer16(AD3344_DUMMY_WORD);

    AD3344_SetCsHigh(AD3344_SHORT_DELAY_US);

    if(config_readback != 0)
    {
        *config_readback = readback_word;
    }

    return adc_code;
}



void ad3344_process(void)
{
    uint16_t hidden_address;

    hidden_address = AD3344_MakeHiddenAddress(AD3344_HIDDEN_PROC_REG);

    AD3344_WriteRegisterSequence(hidden_address, AD3344_HIDDEN_PROC_VALUE);
}


/*
 * Enable AIN3 external reference mode.
 */
void ad3344_ExtRef(void)
{
    uint16_t hidden_address;
    uint16_t register_value;

    hidden_address = AD3344_MakeHiddenAddress(AD3344_HIDDEN_REF_REG);

    register_value = AD3344_ReadRegisterSequence(hidden_address);
    register_value = (uint16_t)(register_value | AD3344_EXT_REF_ENABLE_BIT);

    AD3344_WriteRegisterSequence(hidden_address, register_value);
}