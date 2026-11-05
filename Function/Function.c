#include "Function.h"
#include "usart.h"
#include "usart0.h"
#include "LED.h"
#include "Protocol.h"
#include "AppBusiness.h"
#include "gd30ad3344.h"
#include "PT100.h"
#include "rtc_driver.h"
#include "dac_driver.h"
#include "adc_driver.h"
#include "OLED.h"

#define TEAM_ID_TEXT       "2026933628"
#define OLED_STATUS_IDLE   0U
#define OLED_STATUS_AUTO   1U

extern volatile uint8_t g_need_reboot;

static uint8_t s_led1_state = 0U;
static uint32_t s_led1_last_ms = 0UL;
static uint8_t s_oled_status = 0xFFU;

static void App_OLED_ShowStatus(uint8_t status)
{
    if(s_oled_status == status)
    {
        return;
    }

    s_oled_status = status;

    OLED_Clear();
    OLED_ShowString(0, 0, (uint8_t *)TEAM_ID_TEXT, 16);

    if(status == OLED_STATUS_AUTO)
    {
        OLED_ShowString(0, 16, (uint8_t *)"AutoSample", 16);
    }
    else
    {
        OLED_ShowString(0, 16, (uint8_t *)"IDLE", 16);
    }

    OLED_Refresh();
}

static void App_StatusTask(void)
{
    uint32_t now_ms;
    uint8_t auto_running;

    now_ms = RTC_Driver_GetTickMs();

    /* LED1: APP alive indicator, toggle every 1 second after entering APP. */
    if((uint32_t)(now_ms - s_led1_last_ms) >= 1000UL)
    {
        s_led1_last_ms = now_ms;
        if(s_led1_state == 0U)
        {
            LED1_ON();
            s_led1_state = 1U;
        }
        else
        {
            LED1_OFF();
            s_led1_state = 0U;
        }
    }

    auto_running = Protocol_IsAutoReportEnabled();

    /* LED2: automatic sampling/reporting indicator. */
    if(auto_running != 0U)
    {
        LED2_ON();
        App_OLED_ShowStatus(OLED_STATUS_AUTO);
    }
    else
    {
        LED2_OFF();
        App_OLED_ShowStatus(OLED_STATUS_IDLE);
    }
}

void System_Init(void)
{
    systick_config();
    my_usart_init();

    LED_Init();
    LED1_OFF();
    LED2_OFF();

    ADC_Driver_Init();
    DAC_Driver_Init();
    RTC_Driver_Init();

    OLED_Init();
    App_OLED_ShowStatus(OLED_STATUS_IDLE);

    /* External ADC / PT100 temperature channel. */
    AD3344_SPI_Init();
    ad3344_reset();
    PT100_Init();

    Protocol_ParamLoad();
    AppBusiness_Init();
    usart_set_baudrate(Protocol_GetBaudrateValue());
    usart_init();
}

void UsrFunction(void)
{
    Reply_Heartbeat();

    while(1)
    {
        usart_recv_buf();
        Protocol_Task();
        App_StatusTask();

        if(g_need_reboot != 0U)
        {
            while(usart_is_send_busy() != 0U)
            {
            }

            delay_1ms(200U);
            NVIC_SystemReset();
        }

        if(Protocol_SleepRequested() != 0U)
        {
            while(usart_is_send_busy() != 0U)
            {
            }

            delay_1ms(100U);
            RTC_Driver_EnterSleepAndWakeup10s();
            usart_send_str((uint8_t *)"instrument wakeup", 17U);
        }
    }
}

/**************************** End *****************************/
