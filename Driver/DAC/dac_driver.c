#include "dac_driver.h"

static uint16_t g_dac_value = DAC_DRIVER_DEFAULT_VALUE;

void DAC_Driver_Init(void)
{
    rcu_periph_clock_enable(DAC_DRIVER_GPIO_RCU);
    rcu_periph_clock_enable(RCU_DAC);

    gpio_mode_set(DAC_DRIVER_GPIO_PORT,
                  GPIO_MODE_ANALOG,
                  GPIO_PUPD_NONE,
                  DAC_DRIVER_GPIO_PIN);

    dac_deinit(DAC_DRIVER_PERIPH);
    dac_trigger_source_config(DAC_DRIVER_PERIPH,
                              DAC_DRIVER_OUTPUT,
                              DAC_TRIGGER_SOFTWARE);
    dac_trigger_enable(DAC_DRIVER_PERIPH, DAC_DRIVER_OUTPUT);
    dac_wave_mode_config(DAC_DRIVER_PERIPH,
                         DAC_DRIVER_OUTPUT,
                         DAC_WAVE_DISABLE);
    dac_output_buffer_enable(DAC_DRIVER_PERIPH, DAC_DRIVER_OUTPUT);
    dac_enable(DAC_DRIVER_PERIPH, DAC_DRIVER_OUTPUT);

    DAC_Driver_SetValue(DAC_DRIVER_DEFAULT_VALUE);
}

void DAC_Driver_SetValue(uint16_t value)
{
    if (value > DAC_DRIVER_MAX_VALUE)
    {
        value = DAC_DRIVER_MAX_VALUE;
    }

    g_dac_value = value;
    dac_data_set(DAC_DRIVER_PERIPH,
                 DAC_DRIVER_OUTPUT,
                 DAC_ALIGN_12B_R,
                 value);
    dac_software_trigger_enable(DAC_DRIVER_PERIPH, DAC_DRIVER_OUTPUT);
}

uint16_t DAC_Driver_GetValue(void)
{
    return g_dac_value;
}

float DAC_Driver_GetExpectedVoltage(void)
{
    return ((float)g_dac_value * 3.3f) / 4095.0f;
}
