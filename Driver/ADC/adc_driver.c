#include "adc_driver.h"

static uint16_t ADC_Driver_ReadChannelOnce(uint8_t channel)
{
    uint32_t timeout = 1000000UL;

    adc_routine_channel_config(ADC_DRIVER_PERIPH,
                               0U,
                               channel,
                               ADC_SAMPLETIME_144);

    adc_flag_clear(ADC_DRIVER_PERIPH, ADC_FLAG_EOC);
    adc_software_trigger_enable(ADC_DRIVER_PERIPH, ADC_ROUTINE_CHANNEL);

    while ((adc_flag_get(ADC_DRIVER_PERIPH, ADC_FLAG_EOC) == RESET) &&
           (timeout > 0UL))
    {
        --timeout;
    }

    if (timeout == 0UL)
    {
        return 0U;
    }

    adc_flag_clear(ADC_DRIVER_PERIPH, ADC_FLAG_EOC);
    return (uint16_t)adc_routine_data_read(ADC_DRIVER_PERIPH);
}

static uint16_t ADC_Driver_ReadChannelAverage(uint8_t channel)
{
    uint32_t sum = 0UL;
    uint16_t index;

    for (index = 0U; index < ADC_DRIVER_AVERAGE_COUNT; ++index)
    {
        sum += ADC_Driver_ReadChannelOnce(channel);
    }

    return (uint16_t)(sum / ADC_DRIVER_AVERAGE_COUNT);
}

void ADC_Driver_Init(void)
{
    rcu_periph_clock_enable(ADC_DRIVER_GPIO_RCU);
    rcu_periph_clock_enable(ADC_DRIVER_RCU);

    gpio_mode_set(ADC_DRIVER_GPIO_PORT,
                  GPIO_MODE_ANALOG,
                  GPIO_PUPD_NONE,
                  ADC_DRIVER_CH0_GPIO_PIN | ADC_DRIVER_CH1_GPIO_PIN);

    adc_deinit();
    adc_clock_config(ADC_ADCCK_PCLK2_DIV8);
    adc_sync_mode_config(ADC_SYNC_MODE_INDEPENDENT);
    adc_special_function_config(ADC_DRIVER_PERIPH, ADC_SCAN_MODE, DISABLE);
    adc_special_function_config(ADC_DRIVER_PERIPH, ADC_CONTINUOUS_MODE, DISABLE);
    adc_data_alignment_config(ADC_DRIVER_PERIPH, ADC_DATAALIGN_RIGHT);
    adc_channel_length_config(ADC_DRIVER_PERIPH, ADC_ROUTINE_CHANNEL, 1U);
    adc_external_trigger_config(ADC_DRIVER_PERIPH,
                                ADC_ROUTINE_CHANNEL,
                                EXTERNAL_TRIGGER_DISABLE);

    adc_enable(ADC_DRIVER_PERIPH);
    delay_1ms(1U);
    adc_calibration_enable(ADC_DRIVER_PERIPH);
}

uint16_t ADC_Driver_ReadCH0Raw(void)
{
    return ADC_Driver_ReadChannelAverage(ADC_DRIVER_CH0_CHANNEL);
}

uint16_t ADC_Driver_ReadCH1Raw(void)
{
    return ADC_Driver_ReadChannelAverage(ADC_DRIVER_CH1_CHANNEL);
}

float ADC_Driver_RawToVoltage(uint16_t raw_value)
{
    if (raw_value > ADC_DRIVER_FULL_SCALE)
    {
        raw_value = ADC_DRIVER_FULL_SCALE;
    }

    return ((float)raw_value * ADC_DRIVER_REFERENCE_VOLTAGE) /
           (float)ADC_DRIVER_FULL_SCALE;
}

float ADC_Driver_ReadCH0Voltage(void)
{
    return ADC_Driver_RawToVoltage(ADC_Driver_ReadCH0Raw());
}

float ADC_Driver_ReadCH1Voltage(void)
{
    return ADC_Driver_RawToVoltage(ADC_Driver_ReadCH1Raw());
}
