#ifndef __DAC_DRIVER_H__
#define __DAC_DRIVER_H__

#include "HeaderFiles.h"

#define DAC_DRIVER_PERIPH          DAC0
#define DAC_DRIVER_OUTPUT          DAC_OUT0
#define DAC_DRIVER_GPIO_RCU        RCU_GPIOA
#define DAC_DRIVER_GPIO_PORT       GPIOA
#define DAC_DRIVER_GPIO_PIN        GPIO_PIN_4
#define DAC_DRIVER_DEFAULT_VALUE   2048U
#define DAC_DRIVER_MAX_VALUE       4095U

void DAC_Driver_Init(void);
void DAC_Driver_SetValue(uint16_t value);
uint16_t DAC_Driver_GetValue(void);
float DAC_Driver_GetExpectedVoltage(void);

#endif
