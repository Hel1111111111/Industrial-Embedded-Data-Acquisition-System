#include "rtc_driver.h"


static uint32_t g_unix_timestamp = 0x6A09A79FUL;
static uint32_t g_set_ms = 0UL;
static volatile uint32_t g_ms_ticks = 0UL;
static volatile uint8_t g_rtc_alarm_wakeup = 0U;
static uint8_t g_hw_rtc_ready = 0U;

static uint8_t RTC_Driver_BinToBcd(uint8_t value)
{
    return (uint8_t)(((value / 10U) << 4U) | (value % 10U));
}

static uint8_t RTC_Driver_BcdToBin(uint8_t value)
{
    return (uint8_t)(((value >> 4U) * 10U) + (value & 0x0FU));
}

static void RTC_Driver_HardwareInit(void)
{
    rtc_parameter_struct rtc_initpara;
    uint32_t timeout;

    if(g_hw_rtc_ready != 0U)
    {
        return;
    }

    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();

    /* Prefer LXTAL. If it is not stable, fall back to IRC32K to avoid lock-up. */
    rcu_osci_on(RCU_LXTAL);
    timeout = 0x2FFFFFUL;
    while((rcu_flag_get(RCU_FLAG_LXTALSTB) == RESET) && (timeout > 0UL))
    {
        timeout--;
    }

    if(rcu_flag_get(RCU_FLAG_LXTALSTB) != RESET)
    {
        rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);
    }
    else
    {
        rcu_osci_on(RCU_IRC32K);
        timeout = 0x2FFFFFUL;
        while((rcu_flag_get(RCU_FLAG_IRC32KSTB) == RESET) && (timeout > 0UL))
        {
            timeout--;
        }
        rcu_rtc_clock_config(RCU_RTCSRC_IRC32K);
    }

    rcu_periph_clock_enable(RCU_RTC);

    rtc_initpara.factor_asyn = 0x63U;
    rtc_initpara.factor_syn = 0x13FU;
    rtc_initpara.year = 0x26U;
    rtc_initpara.day_of_week = RTC_SATURDAY;
    rtc_initpara.month = RTC_JAN;
    rtc_initpara.date = 0x01U;
    rtc_initpara.display_format = RTC_24HOUR;
    rtc_initpara.am_pm = RTC_AM;
    rtc_initpara.hour = 0x00U;
    rtc_initpara.minute = 0x00U;
    rtc_initpara.second = 0x00U;
    (void)rtc_init(&rtc_initpara);

    rtc_alarm_disable(RTC_ALARM0);
    rtc_flag_clear(RTC_FLAG_ALRM0);
    exti_interrupt_flag_clear(EXTI_17);
    exti_init(EXTI_17, EXTI_INTERRUPT, EXTI_TRIG_RISING);
    exti_interrupt_enable(EXTI_17);
    nvic_irq_enable(RTC_Alarm_IRQn, 1U, 0U);

    g_hw_rtc_ready = 1U;
}

void RTC_Driver_Init(void)
{
    g_set_ms = g_ms_ticks;
    RTC_Driver_HardwareInit();
}

void RTC_Driver_SetUnixTimestamp(uint32_t timestamp)
{
    g_unix_timestamp = timestamp;
    g_set_ms = g_ms_ticks;
}

uint32_t RTC_Driver_GetUnixTimestamp(void)
{
    return g_unix_timestamp + ((g_ms_ticks - g_set_ms) / 1000UL);
}

void RTC_Driver_Tick1ms(void)
{
    ++g_ms_ticks;
}

uint32_t RTC_Driver_GetTickMs(void)
{
    return g_ms_ticks;
}

void RTC_Driver_AlarmIRQHandler(void)
{
    if(rtc_flag_get(RTC_FLAG_ALRM0) != RESET)
    {
        rtc_alarm_disable(RTC_ALARM0);
        rtc_flag_clear(RTC_FLAG_ALRM0);
        exti_interrupt_flag_clear(EXTI_17);
        g_rtc_alarm_wakeup = 1U;
    }
}

void RTC_Driver_EnterSleepAndWakeup10s(void)
{
    uint32_t start_tick;

    /*
       ??????? pmu_wakeup_pin_enable(PMU_WAKEUP_PIN0),
       ????? GD32F4xx ?? pmu_wakeup_pin_enable() ???????
       ????? J-01 ?????,???? WFI ????? 10s?
    */

    start_tick = RTC_Driver_GetTickMs();

    while ((RTC_Driver_GetTickMs() - start_tick) < 10000UL)
    {
        pmu_to_sleepmode(WFI_CMD);
    }
}