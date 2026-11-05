#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>

void Protocol_Process(uint8_t *ascii_buf, uint16_t ascii_len);
void Protocol_Task(void);
void Protocol_ParamLoad(void);

void Protocol_SendFrame(uint16_t id, uint8_t type, uint16_t cmd, uint8_t *payload, uint8_t payload_len);
void Protocol_ReplyOK(uint16_t id, uint16_t cmd);
void Protocol_ReplyError(uint16_t id);
void Protocol_SendRaw(const uint8_t *data, uint16_t len);
uint16_t Protocol_Crc16Modbus(uint8_t *buf, uint16_t len);

void Protocol_U32ToBE(uint32_t value, uint8_t *out);
uint32_t Protocol_BEToU32(uint8_t *in);
void Protocol_FloatToBE(float value, uint8_t *out);
float Protocol_BEToFloat(uint8_t *in);

void Reply_Heartbeat(void);
uint8_t Protocol_SleepRequested(void);
uint32_t Protocol_GetBaudrateValue(void);
uint16_t Protocol_GetDeviceId(void);
uint8_t Protocol_IsAutoReportEnabled(void);

#endif
