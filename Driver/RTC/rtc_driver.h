#ifndef __RTC_DRIVER_H__
#define __RTC_DRIVER_H__

#include "HeaderFiles.h"

void RTC_Driver_Init(void);
void RTC_Driver_SetUnixTimestamp(uint32_t timestamp);
uint32_t RTC_Driver_GetUnixTimestamp(void);
uint32_t RTC_Driver_GetTickMs(void);
void RTC_Driver_Tick1ms(void);
void RTC_Driver_EnterSleepAndWakeup10s(void);
void RTC_Driver_AlarmIRQHandler(void);

#endif
