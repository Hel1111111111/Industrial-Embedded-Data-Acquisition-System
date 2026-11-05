#include "Protocol.h"
#include "usart.h"
#include "AppBusiness.h"
#include <string.h>

#define PROTOCOL_VER 0x02U

static uint8_t hex_char_to_val(uint8_t c)
{
    if(c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if(c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10U);
    if(c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10U);
    return 0xFFU;
}

static uint16_t ascii_to_bytes(uint8_t *ascii, uint16_t ascii_len, uint8_t *out, uint16_t out_size)
{
    uint16_t i;
    uint8_t high, low;

    if((ascii_len & 1U) != 0U) return 0U;
    if((ascii_len / 2U) > out_size) return 0U;

    for(i = 0U; i < (ascii_len / 2U); i++)
    {
        high = hex_char_to_val(ascii[2U * i]);
        low  = hex_char_to_val(ascii[(2U * i) + 1U]);
        if((high == 0xFFU) || (low == 0xFFU)) return 0U;
        out[i] = (uint8_t)((high << 4U) | low);
    }

    return (uint16_t)(ascii_len / 2U);
}

static uint8_t val_to_hex_char(uint8_t v)
{
    return (v < 10U) ? (uint8_t)('0' + v) : (uint8_t)('A' + v - 10U);
}

static uint16_t bytes_to_ascii(uint8_t *bytes, uint16_t len, uint8_t *ascii)
{
    uint16_t i;
    for(i = 0U; i < len; i++)
    {
        ascii[2U * i] = val_to_hex_char((uint8_t)(bytes[i] >> 4U));
        ascii[(2U * i) + 1U] = val_to_hex_char((uint8_t)(bytes[i] & 0x0FU));
    }
    return (uint16_t)(len * 2U);
}

uint16_t Protocol_Crc16Modbus(uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    uint8_t j;

    for(i = 0U; i < len; i++)
    {
        crc ^= buf[i];
        for(j = 0U; j < 8U; j++)
        {
            crc = (crc & 0x0001U) ? (uint16_t)((crc >> 1U) ^ 0xA001U) : (uint16_t)(crc >> 1U);
        }
    }

    return crc;
}

void Protocol_U32ToBE(uint32_t value, uint8_t *out)
{
    out[0] = (uint8_t)(value >> 24U);
    out[1] = (uint8_t)(value >> 16U);
    out[2] = (uint8_t)(value >> 8U);
    out[3] = (uint8_t)value;
}

uint32_t Protocol_BEToU32(uint8_t *in)
{
    return ((uint32_t)in[0] << 24U) |
           ((uint32_t)in[1] << 16U) |
           ((uint32_t)in[2] << 8U)  |
           ((uint32_t)in[3]);
}

void Protocol_FloatToBE(float value, uint8_t *out)
{
    union { float f; uint8_t b[4]; } u;
    u.f = value;
    out[0] = u.b[3];
    out[1] = u.b[2];
    out[2] = u.b[1];
    out[3] = u.b[0];
}

float Protocol_BEToFloat(uint8_t *in)
{
    union { float f; uint8_t b[4]; } u;
    u.b[3] = in[0];
    u.b[2] = in[1];
    u.b[1] = in[2];
    u.b[0] = in[3];
    return u.f;
}

void Protocol_SendRaw(const uint8_t *data, uint16_t len)
{
    if((data != 0) && (len <= 255U))
    {
        usart_send_str((uint8_t *)data, (uint8_t)len);
    }
}

void Protocol_SendFrame(uint16_t id, uint8_t type, uint16_t cmd, uint8_t *payload, uint8_t payload_len)
{
    uint8_t frame[96];
    uint8_t ascii[192];
    uint16_t index = 0U;
    uint16_t crc;
    uint16_t ascii_len;
    uint8_t i;

    frame[index++] = 0xA5U;
    frame[index++] = 0xB6U;
    frame[index++] = (uint8_t)(id >> 8U);
    frame[index++] = (uint8_t)id;
    frame[index++] = type;
    frame[index++] = (uint8_t)(cmd >> 8U);
    frame[index++] = (uint8_t)cmd;
    frame[index++] = payload_len;
    frame[index++] = PROTOCOL_VER;

    for(i = 0U; i < payload_len; i++)
    {
        frame[index++] = payload[i];
    }

    crc = Protocol_Crc16Modbus(frame, index);
    frame[index++] = (uint8_t)(crc >> 8U);
    frame[index++] = (uint8_t)crc;
    frame[index++] = 0xB6U;
    frame[index++] = 0xA5U;

    ascii_len = bytes_to_ascii(frame, index, ascii);
    usart_send_str(ascii, (uint8_t)ascii_len);
}

void Protocol_ReplyOK(uint16_t id, uint16_t cmd)
{
    uint8_t payload[1] = {0xFFU};
    Protocol_SendFrame(id, 0x02U, cmd, payload, 1U);
}

void Protocol_ReplyError(uint16_t id)
{
    Protocol_SendFrame(id, 0xFFU, 0xEEEEU, 0, 0U);
}

void Protocol_Process(uint8_t *ascii_buf, uint16_t ascii_len)
{
    uint8_t frame[160];
    uint16_t frame_len;
    uint16_t start;
    uint16_t end;
    uint16_t id;
    uint8_t type;
    uint16_t cmd;
    uint8_t len;
    uint8_t ver;
    uint8_t *content;
    uint16_t recv_crc;
    uint16_t calc_crc;

    while((ascii_len > 0U) &&
          ((ascii_buf[ascii_len - 1U] == '\r') ||
           (ascii_buf[ascii_len - 1U] == '\n') ||
           (ascii_buf[ascii_len - 1U] == ' ')))
    {
        ascii_len--;
    }

    if((ascii_len < 26U) || (ascii_len > 320U) || ((ascii_len & 1U) != 0U))
    {
        return;
    }

    frame_len = ascii_to_bytes(ascii_buf, ascii_len, frame, sizeof(frame));
    if(frame_len < 13U)
    {
        return;
    }

    start = ((uint16_t)frame[0] << 8U) | frame[1];
    end = ((uint16_t)frame[frame_len - 2U] << 8U) | frame[frame_len - 1U];
    if((start != 0xA5B6U) || (end != 0xB6A5U))
    {
        return;
    }

    id = ((uint16_t)frame[2] << 8U) | frame[3];
    type = frame[4];
    cmd = ((uint16_t)frame[5] << 8U) | frame[6];
    len = frame[7];
    ver = frame[8];
    content = &frame[9];

    if(ver != PROTOCOL_VER)
    {
        Protocol_ReplyError(AppBusiness_GetDeviceId());
        return;
    }

    if(frame_len != (uint16_t)(13U + len))
    {
        Protocol_ReplyError(AppBusiness_GetDeviceId());
        return;
    }

    recv_crc = ((uint16_t)frame[9U + len] << 8U) | frame[10U + len];
    calc_crc = Protocol_Crc16Modbus(frame, (uint16_t)(9U + len));
    if(recv_crc != calc_crc)
    {
        Protocol_ReplyError(AppBusiness_GetDeviceId());
        return;
    }

    if((id != AppBusiness_GetDeviceId()) && (id != 0xFFFFU))
    {
        return;
    }

    (void)AppBusiness_HandleCommand(id, type, cmd, content, len);
}

void Reply_Heartbeat(void)
{
    Protocol_SendFrame(AppBusiness_GetDeviceId(), 0x05U, 0x8888U, 0, 0U);
}

void Protocol_ParamLoad(void)
{
    AppBusiness_ParamLoad();
}

void Protocol_Task(void)
{
    AppBusiness_Task();
}

uint8_t Protocol_SleepRequested(void)
{
    return AppBusiness_SleepRequested();
}

uint32_t Protocol_GetBaudrateValue(void)
{
    return AppBusiness_GetBaudrateValue();
}

uint16_t Protocol_GetDeviceId(void)
{
    return AppBusiness_GetDeviceId();
}

uint8_t Protocol_IsAutoReportEnabled(void)
{
    return AppBusiness_IsAutoReportEnabled();
}
