#ifndef __APP_BUSINESS_H
#define __APP_BUSINESS_H

#include <stdint.h>

#define APP_CMD_HANDLED     1U
#define APP_CMD_UNHANDLED   0U

void AppBusiness_Init(void);
void AppBusiness_Task(void);
uint8_t AppBusiness_HandleCommand(uint16_t id, uint8_t type, uint16_t cmd, uint8_t *content, uint8_t len);

uint8_t AppBusiness_SleepRequested(void);
uint8_t AppBusiness_IsAutoReportEnabled(void);
uint32_t AppBusiness_GetBaudrateValue(void);
uint16_t AppBusiness_GetDeviceId(void);
void AppBusiness_ParamLoad(void);

#endif
