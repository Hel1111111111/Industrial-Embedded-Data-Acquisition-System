#ifndef __ADC_DRIVER_H__
#define __ADC_DRIVER_H__

#include "HeaderFiles.h"


#define ADC_DRIVER_PERIPH              ADC0
#define ADC_DRIVER_RCU                 RCU_ADC0
#define ADC_DRIVER_GPIO_RCU            RCU_GPIOC
#define ADC_DRIVER_GPIO_PORT           GPIOC
#define ADC_DRIVER_CH0_GPIO_PIN        GPIO_PIN_0
#define ADC_DRIVER_CH1_GPIO_PIN        GPIO_PIN_1
#define ADC_DRIVER_CH0_CHANNEL         ADC_CHANNEL_10
#define ADC_DRIVER_CH1_CHANNEL         ADC_CHANNEL_11
#define ADC_DRIVER_REFERENCE_VOLTAGE   3.3f
#define ADC_DRIVER_FULL_SCALE          4095U
#define ADC_DRIVER_AVERAGE_COUNT       8U

void ADC_Driver_Init(void);
uint16_t ADC_Driver_ReadCH0Raw(void);
uint16_t ADC_Driver_ReadCH1Raw(void);
float ADC_Driver_RawToVoltage(uint16_t raw_value);
float ADC_Driver_ReadCH0Voltage(void);
float ADC_Driver_ReadCH1Voltage(void);

#endif
