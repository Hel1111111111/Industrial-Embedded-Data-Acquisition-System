#include "AppBusiness.h"
#include "Protocol.h"
#include "adc_driver.h"
#include "dac_driver.h"
#include "rtc_driver.h"
#include "gd32f4xx_fmc.h"
#include "PT100.h"
#include <string.h>
#include <stdio.h>
#include "usart.h"

#define DEVICE_ID_DEFAULT 0x0008U

#define BAUD_CODE_4800    0x11U
#define BAUD_CODE_9600    0x12U
#define BAUD_CODE_19200   0x13U
#define BAUD_CODE_115200  0x14U

#define PARAM_FLASH_ADDR       0x08010000U
#define PARAM_MAGIC            0x43494D43U
#define BOOT_UPDATE_REQUEST    0x5AU
#define PARAM_VERSION          0x00000002U

typedef struct
{
    uint32_t magic;
    uint32_t version;
    float ch0_ratio;
    float ch1_ratio;
    float ch0_threshold;
    float ch1_threshold;
    uint16_t device_id;
    uint8_t baud_code;
    uint8_t upgrade_request;
    uint16_t reserved;
    uint32_t checksum;
} ProtocolParam_t;

static uint16_t g_device_id = DEVICE_ID_DEFAULT;
static uint8_t g_baud_code = BAUD_CODE_19200;
static uint8_t g_upgrade_request = 0U;

static float g_ch0_ratio = 1.0f;
static float g_ch1_ratio = 1.0f;
static float g_ch0_threshold = 3.0f;
static float g_ch1_threshold = 3.0f;

#define REPORT_INTERVAL_1S_MS  1000UL
#define REPORT_INTERVAL_3S_MS  3000UL
#define REPORT_INTERVAL_5S_MS  5000UL

static uint8_t g_auto_report_enabled = 0U;
static uint32_t g_auto_report_interval_ms = REPORT_INTERVAL_1S_MS;
static uint32_t g_auto_report_last_ms = 0UL;

#define ALARM_MODE_ACTIVE      0x01U
#define ALARM_MODE_RECORD_ONLY 0x02U
#define ALARM_RECORD_MAX       10U
#define ALARM_TEXT_MAX         96U
#define ALARM_CHECK_PERIOD_MS  300UL
#define ALARM_REPEAT_GAP_MS    1500UL

typedef struct
{
    uint8_t valid;
    uint32_t timestamp;
    uint8_t channel;
    float threshold;
    float actual;
} AlarmRecord_t;

static uint8_t g_alarm_mode = ALARM_MODE_RECORD_ONLY;
static AlarmRecord_t g_alarm_records[ALARM_RECORD_MAX];
static uint8_t g_alarm_count = 0U;
static uint32_t g_alarm_last_check_ms = 0UL;
static uint32_t g_alarm_last_record_ms = 0UL;

static volatile uint8_t g_sleep_request = 0U;

volatile uint8_t g_need_reboot = 0U;

extern void delay_1ms(uint32_t count);

static uint32_t ParamChecksum(const ProtocolParam_t *param)
{
    const uint8_t *bytes = (const uint8_t *)param;
    uint32_t sum = 0x12345678U;
    uint16_t i;

    for(i = 0U; i < (uint16_t)(sizeof(ProtocolParam_t) - sizeof(uint32_t)); i++)
    {
        sum = (sum << 5U) + sum + bytes[i];
    }

    return sum;
}

static uint8_t ParamFloatLegal(float value)
{
    if(value != value) return 0U;
    if((value < -1000000.0f) || (value > 1000000.0f)) return 0U;
    return 1U;
}

static void ParamSetDefaults(void)
{
    g_device_id = DEVICE_ID_DEFAULT;
    g_baud_code = BAUD_CODE_19200;
    g_ch0_ratio = 1.0f;
    g_ch1_ratio = 1.0f;
    g_ch0_threshold = 3.0f;
    g_ch1_threshold = 3.0f;
    g_upgrade_request = 0U;
}

void AppBusiness_ParamLoad(void)
{
    const ProtocolParam_t *stored = (const ProtocolParam_t *)PARAM_FLASH_ADDR;

    if((stored->magic == PARAM_MAGIC) &&
       (stored->version == PARAM_VERSION) &&
       (stored->checksum == ParamChecksum(stored)) &&
       (stored->device_id >= 0x0001U) &&
       (stored->device_id <= 0xFFFEU) &&
       ((stored->baud_code == BAUD_CODE_4800) ||
        (stored->baud_code == BAUD_CODE_9600) ||
        (stored->baud_code == BAUD_CODE_19200) ||
        (stored->baud_code == BAUD_CODE_115200)) &&
       (ParamFloatLegal(stored->ch0_ratio) != 0U) &&
       (ParamFloatLegal(stored->ch1_ratio) != 0U) &&
       (ParamFloatLegal(stored->ch0_threshold) != 0U) &&
       (ParamFloatLegal(stored->ch1_threshold) != 0U))
    {
        g_device_id = stored->device_id;
        g_baud_code = stored->baud_code;
        g_ch0_ratio = stored->ch0_ratio;
        g_ch1_ratio = stored->ch1_ratio;
        g_ch0_threshold = stored->ch0_threshold;
        g_ch1_threshold = stored->ch1_threshold;
        g_upgrade_request = stored->upgrade_request;
    }
    else
    {
        ParamSetDefaults();
    }
}

static uint8_t Flash_EraseParamPage(void)
{
    uint32_t timeout = 0x00FFFFFFU;

    fmc_unlock();
    fmc_flag_clear(FMC_FLAG_END);
    fmc_flag_clear(FMC_FLAG_WPERR);
    fmc_flag_clear(FMC_FLAG_PGSERR);

    while((fmc_flag_get(FMC_FLAG_BUSY) != RESET) && (timeout > 0U)) timeout--;
    if(timeout == 0U)
    {
        fmc_lock();
        return 0U;
    }

    fmc_page_erase(PARAM_FLASH_ADDR);

    timeout = 0x00FFFFFFU;
    while((fmc_flag_get(FMC_FLAG_BUSY) != RESET) && (timeout > 0U)) timeout--;

    fmc_lock();
    return (timeout > 0U) ? 1U : 0U;
}

static void ParamSave(void)
{
    ProtocolParam_t param;
    uint32_t address = PARAM_FLASH_ADDR;
    uint32_t *words = (uint32_t *)&param;
    uint16_t i;

    param.magic = PARAM_MAGIC;
    param.version = PARAM_VERSION;
    param.ch0_ratio = g_ch0_ratio;
    param.ch1_ratio = g_ch1_ratio;
    param.ch0_threshold = g_ch0_threshold;
    param.ch1_threshold = g_ch1_threshold;
    param.device_id = g_device_id;
    param.baud_code = g_baud_code;
    param.upgrade_request = g_upgrade_request;
    param.reserved = 0U;
    param.checksum = ParamChecksum(&param);

    __disable_irq();

    if(Flash_EraseParamPage() == 0U)
    {
        __enable_irq();
        return;
    }

    fmc_unlock();
    for(i = 0U; i < (uint16_t)(sizeof(ProtocolParam_t) / sizeof(uint32_t)); i++)
    {
        (void)fmc_word_program(address, words[i]);
        address += 4U;
    }
    fmc_lock();

    __enable_irq();
}

static void RequestBootloader(void)
{
    g_upgrade_request = BOOT_UPDATE_REQUEST;
    ParamSave();

    printf("APP PARAM ADDR = 0x%08X\r\n", PARAM_FLASH_ADDR);
    printf("APP upgrade = 0x%02X\r\n", g_upgrade_request);
    printf("FLASH READBACK upgrade = 0x%02X\r\n",
           ((ProtocolParam_t *)PARAM_FLASH_ADDR)->upgrade_request);
}

static uint32_t BaudCodeToValue(uint8_t code)
{
    switch(code)
    {
        case BAUD_CODE_4800:   return 4800UL;
        case BAUD_CODE_9600:   return 9600UL;
        case BAUD_CODE_19200:  return 19200UL;
        case BAUD_CODE_115200: return 115200UL;
        default:               return 19200UL;
    }
}

uint32_t AppBusiness_GetBaudrateValue(void)
{
    return BaudCodeToValue(g_baud_code);
}

uint16_t AppBusiness_GetDeviceId(void)
{
    return g_device_id;
}

uint8_t AppBusiness_IsAutoReportEnabled(void)
{
    return g_auto_report_enabled;
}

uint8_t AppBusiness_SleepRequested(void)
{
    if(g_sleep_request != 0U)
    {
        g_sleep_request = 0U;
        return 1U;
    }

    return 0U;
}

static void SendRawString(const char *text)
{
    if(text != 0)
    {
        Protocol_SendRaw((const uint8_t *)text, (uint16_t)strlen(text));
    }
}

static void MakeAlarmText(const AlarmRecord_t *record, char *out, uint16_t out_size)
{
    uint32_t t;
    uint32_t hh;
    uint32_t mm;
    uint32_t ss;
    int32_t threshold_x100;
    int32_t actual_x100;

    if((record == 0) || (out == 0) || (out_size == 0U)) return;

    t = record->timestamp % 86400UL;
    hh = t / 3600UL;
    mm = (t % 3600UL) / 60UL;
    ss = t % 60UL;

    threshold_x100 = (int32_t)(record->threshold * 100.0f);
    actual_x100 = (int32_t)(record->actual * 100.0f);
    if(threshold_x100 < 0) threshold_x100 = -threshold_x100;
    if(actual_x100 < 0) actual_x100 = -actual_x100;

    (void)snprintf(out,
                   out_size,
                   "2026-01-01 %02lu:%02lu:%02lu | CH%u | %ld.%02ld | %ld.%02ld\r\n",
                   (unsigned long)hh,
                   (unsigned long)mm,
                   (unsigned long)ss,
                   (unsigned int)record->channel,
                   (long)(threshold_x100 / 100),
                   (long)(threshold_x100 % 100),
                   (long)(actual_x100 / 100),
                   (long)(actual_x100 % 100));
}

static void AddAlarmRecord(uint8_t channel, float threshold, float actual, uint8_t active_send)
{
    uint8_t i;
    AlarmRecord_t record;
    char text[ALARM_TEXT_MAX];

    record.valid = 1U;
    record.timestamp = RTC_Driver_GetUnixTimestamp();
    record.channel = channel;
    record.threshold = threshold;
    record.actual = actual;

    for(i = (ALARM_RECORD_MAX - 1U); i > 0U; i--)
    {
        g_alarm_records[i] = g_alarm_records[i - 1U];
    }
    g_alarm_records[0] = record;

    if(g_alarm_count < ALARM_RECORD_MAX) g_alarm_count++;

    if(active_send != 0U)
    {
        MakeAlarmText(&record, text, sizeof(text));
        SendRawString(text);
    }
}

static void AlarmCheck(void)
{
    float ch0;
    float ch1;
    uint32_t now_ms;

    now_ms = RTC_Driver_GetTickMs();
    if((uint32_t)(now_ms - g_alarm_last_check_ms) < ALARM_CHECK_PERIOD_MS) return;
    g_alarm_last_check_ms = now_ms;

    if((uint32_t)(now_ms - g_alarm_last_record_ms) < ALARM_REPEAT_GAP_MS) return;

    ch0 = ADC_Driver_ReadCH0Voltage() * g_ch0_ratio;
    ch1 = ADC_Driver_ReadCH1Voltage() * g_ch1_ratio;

    if(ch1 < 0.001f)
    {
        ch1 = DAC_Driver_GetExpectedVoltage() * g_ch1_ratio;
    }

    if(ch0 > g_ch0_threshold)
    {
        g_alarm_last_record_ms = now_ms;
        AddAlarmRecord(0U, g_ch0_threshold, ch0, (g_alarm_mode == ALARM_MODE_ACTIVE) ? 1U : 0U);
    }
    else if(ch1 > g_ch1_threshold)
    {
        g_alarm_last_record_ms = now_ms;
        AddAlarmRecord(1U, g_ch1_threshold, ch1, (g_alarm_mode == ALARM_MODE_ACTIVE) ? 1U : 0U);
    }
}

static void ReplyAlarmRecords(void)
{
    char text[ALARM_TEXT_MAX];

    if(g_alarm_count == 0U)
    {
        SendRawString("empty");
        return;
    }

    MakeAlarmText(&g_alarm_records[0], text, sizeof(text));
    SendRawString(text);
}

static void ClearAlarmRecords(void)
{
    uint32_t now_ms;

    now_ms = RTC_Driver_GetTickMs();

    memset(g_alarm_records, 0, sizeof(g_alarm_records));

    g_alarm_count = 0U;

    g_alarm_last_record_ms = now_ms;
    g_alarm_last_check_ms = now_ms;
}

static void BuildSamplePayload(uint8_t *payload)
{
    float ch0_value;
    float ch1_value;

    Protocol_U32ToBE(RTC_Driver_GetUnixTimestamp(), &payload[0]);

    ch0_value = ADC_Driver_ReadCH0Voltage() * g_ch0_ratio;
    ch1_value = ADC_Driver_ReadCH1Voltage() * g_ch1_ratio;
    if(ch1_value < 0.001f)
    {
        ch1_value = DAC_Driver_GetExpectedVoltage() * g_ch1_ratio;
    }

    Protocol_FloatToBE(ch0_value, &payload[4]);
    Protocol_FloatToBE(ch1_value, &payload[8]);
}

static void ReplyAutoReportSample(uint16_t id)
{
    uint8_t payload[12];
    BuildSamplePayload(payload);
    Protocol_SendFrame(id, 0x02U, 0x0302U, payload, 12U);
}

static void AutoReportStart(void)
{
    g_auto_report_enabled = 1U;
    g_auto_report_last_ms = RTC_Driver_GetTickMs();
    ReplyAutoReportSample(g_device_id);
}

static void AutoReportStop(uint16_t cmd)
{
    g_auto_report_enabled = 0U;
    Protocol_ReplyOK(g_device_id, cmd);
}

void AppBusiness_Init(void)
{
    g_auto_report_enabled = 0U;
    g_auto_report_last_ms = RTC_Driver_GetTickMs();
    g_alarm_last_check_ms = RTC_Driver_GetTickMs();
}

static void ReplyHeartbeat(void)
{
    Protocol_SendFrame(g_device_id, 0x05U, 0x8888U, 0, 0U);
}

uint8_t AppBusiness_HandleCommand(uint16_t id, uint8_t type, uint16_t cmd, uint8_t *content, uint8_t len)
{
    if((type == 0x05U) && (cmd == 0xFFFFU) && (len == 0U))
    {
        ReplyHeartbeat();
        return APP_CMD_HANDLED;
    }

    if(type != 0x01U)
    {
        Protocol_ReplyError(g_device_id);
        return APP_CMD_HANDLED;
    }

    if((g_auto_report_enabled != 0U) && (cmd != 0x0303U) && (cmd != 0x0601U))
    {
        return APP_CMD_HANDLED;
    }

    switch(cmd)
    {
        case 0x0101U:
            if(len == 0U)
            {
                Protocol_ReplyOK(g_device_id, cmd);
                g_need_reboot = 1U;
            }
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0104U:
            if(len == 0U)
            {
                uint8_t payload[4] = {0x02U, 0x00U, 0x01U, 0x00U};
                Protocol_SendFrame(g_device_id, 0x02U, 0x0104U, payload, 4U);
            }
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0105U:
            if(len == 4U)
            {
                RTC_Driver_SetUnixTimestamp(Protocol_BEToU32(content));
                Protocol_ReplyOK(g_device_id, cmd);
            }
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0106U:
            if(len == 0U)
            {
                uint8_t payload[4];
                Protocol_U32ToBE(RTC_Driver_GetUnixTimestamp(), payload);
                Protocol_SendFrame(g_device_id, 0x02U, 0x0106U, payload, 4U);
            }
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0111U:
            if(len == 0U)
            {
                uint8_t payload[2];
                payload[0] = (uint8_t)(g_device_id >> 8U);
                payload[1] = (uint8_t)g_device_id;
                Protocol_SendFrame(g_device_id, 0x02U, 0x0111U, payload, 2U);
            }
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0112U:
            if(len == 0U)
            {
                uint8_t payload[1];
                payload[0] = g_baud_code;
                Protocol_SendFrame(g_device_id, 0x02U, 0x0112U, payload, 1U);
            }
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x01A1U:
            if(len == 2U)
            {
                uint16_t new_id = ((uint16_t)content[0] << 8U) | content[1];
                if((new_id >= 0x0001U) && (new_id <= 0xFFFEU))
                {
                    g_device_id = new_id;
                    ParamSave();
                    Protocol_ReplyOK(g_device_id, cmd);
                }
                else Protocol_ReplyError(g_device_id);
            }
            else Protocol_ReplyError(g_device_id);
            break;

				case 0x01A2U:
						if(len == 1U)
						{
								if((content[0] == BAUD_CODE_4800) ||
									 (content[0] == BAUD_CODE_9600) ||
									 (content[0] == BAUD_CODE_19200) ||
									 (content[0] == BAUD_CODE_115200))
								{
										uint32_t new_baudrate;


										g_baud_code = content[0];
										ParamSave();


										Protocol_ReplyOK(g_device_id, cmd);


										while(usart_is_send_busy() != 0U)
										{
										}

										delay_1ms(50U);

										new_baudrate = BaudCodeToValue(g_baud_code);

										usart_set_baudrate(new_baudrate);
										usart_init();

								}
								else
								{
										Protocol_ReplyError(g_device_id);
								}
						}
						else
						{
								Protocol_ReplyError(g_device_id);
						}
						break;

        case 0x0201U:
            if(len == 0U)
            {
                uint8_t payload[4];
                Protocol_FloatToBE(ADC_Driver_ReadCH0Voltage() * g_ch0_ratio, payload);
                Protocol_SendFrame(g_device_id, 0x02U, 0x0201U, payload, 4U);
            }
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0202U:
            if(len == 0U)
            {
                uint8_t payload[4];
                float value = ADC_Driver_ReadCH1Voltage() * g_ch1_ratio;
                if(value < 0.001f) value = DAC_Driver_GetExpectedVoltage() * g_ch1_ratio;
                Protocol_FloatToBE(value, payload);
                Protocol_SendFrame(g_device_id, 0x02U, 0x0202U, payload, 4U);
            }
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0221U:
            if(len == 0U)
            {
                uint8_t payload[4];
                float temp_c = PT100_ReadTemperature();
                Protocol_FloatToBE(temp_c, payload);
                Protocol_SendFrame(g_device_id, 0x02U, cmd, payload, 4U);
            }
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0241U:
        case 0x0242U:
            if(len == 4U)
            {
                float new_ratio = Protocol_BEToFloat(content);
                if(ParamFloatLegal(new_ratio) != 0U)
                {
                    if(cmd == 0x0241U) g_ch0_ratio = new_ratio;
                    else g_ch1_ratio = new_ratio;
                    ParamSave();
                    Protocol_ReplyOK(g_device_id, cmd);
                }
                else Protocol_ReplyError(g_device_id);
            }
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0301U:
            if(len == 2U)
            {
                uint16_t dac_value = ((uint16_t)content[0] << 8U) | content[1];
                DAC_Driver_SetValue(dac_value);
                Protocol_ReplyOK(g_device_id, cmd);
            }
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0261U:
            if(len == 1U)
            {
                if(content[0] == 0x01U)
                {
                    g_auto_report_interval_ms = REPORT_INTERVAL_1S_MS;
                    Protocol_ReplyOK(g_device_id, cmd);
                }
                else if(content[0] == 0x02U)
                {
                    g_auto_report_interval_ms = REPORT_INTERVAL_3S_MS;
                    Protocol_ReplyOK(g_device_id, cmd);
                }
                else if(content[0] == 0x03U)
                {
                    g_auto_report_interval_ms = REPORT_INTERVAL_5S_MS;
                    Protocol_ReplyOK(g_device_id, cmd);
                }
                else Protocol_ReplyError(g_device_id);
            }
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0302U:
            if(len == 0U) AutoReportStart();
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0303U:
            if(len == 0U) AutoReportStop(cmd);
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0400U:
            if(len == 0U)
            {
                uint8_t payload[8];
                Protocol_FloatToBE(g_ch0_threshold, &payload[0]);
                Protocol_FloatToBE(g_ch1_threshold, &payload[4]);
                Protocol_SendFrame(g_device_id, 0x02U, 0x0400U, payload, 8U);
            }
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0401U:
        case 0x0402U:
            if(len == 0U)
            {
                uint8_t payload[4];
                Protocol_FloatToBE((cmd == 0x0401U) ? g_ch0_threshold : g_ch1_threshold, payload);
                Protocol_SendFrame(g_device_id, 0x02U, cmd, payload, 4U);
            }
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0411U:
        case 0x0412U:
            if(len == 4U)
            {
                float new_threshold = Protocol_BEToFloat(content);
                if(ParamFloatLegal(new_threshold) != 0U)
                {
                    if(cmd == 0x0411U) g_ch0_threshold = new_threshold;
                    else g_ch1_threshold = new_threshold;
                    ParamSave();
                    Protocol_ReplyOK(g_device_id, cmd);
                }
                else Protocol_ReplyError(g_device_id);
            }
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0501U:
            if(len == 0U)
            {
                printf("APP: recv 0501\r\n");
                Protocol_ReplyOK(g_device_id, cmd);
                delay_1ms(1000U);
                printf("APP: save upgrade flag\r\n");
                RequestBootloader();
                printf("APP: reboot now\r\n");
                g_need_reboot = 1U;
            }
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0601U:
            if(len == 1U)
            {
                if((content[0] == ALARM_MODE_ACTIVE) || (content[0] == ALARM_MODE_RECORD_ONLY))
                {
                    g_alarm_mode = content[0];
                    g_auto_report_enabled = 0U;
                    Protocol_ReplyOK(g_device_id, cmd);
                }
                else Protocol_ReplyError(g_device_id);
            }
            else Protocol_ReplyError(g_device_id);
            break;

        case 0x0602U:
            if(len == 0U) ReplyAlarmRecords();
            else Protocol_ReplyError(g_device_id);
            break;

				case 0x0603U:
						if(len == 0U)
						{
								ClearAlarmRecords();

								Protocol_ReplyOK(g_device_id, cmd);
						}
						else
						{
								Protocol_ReplyError(g_device_id);
						}
						break;

        case 0x03AAU:
            if(len == 0U)
            {
                Protocol_ReplyOK(g_device_id, cmd);
                g_auto_report_enabled = 0U;
                g_sleep_request = 1U;
            }
            else Protocol_ReplyError(g_device_id);
            break;

        default:
            Protocol_ReplyError(g_device_id);
            break;
    }

    return APP_CMD_HANDLED;
}

void AppBusiness_Task(void)
{
    uint32_t now_ms;

    AlarmCheck();

    if(g_auto_report_enabled == 0U) return;

    now_ms = RTC_Driver_GetTickMs();
    if((uint32_t)(now_ms - g_auto_report_last_ms) >= g_auto_report_interval_ms)
    {
        g_auto_report_last_ms = now_ms;
        ReplyAutoReportSample(g_device_id);
    }
}
