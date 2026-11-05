#include "usart.h"
#include "Protocol.h"

/************************  ************************/
static uint8_t Buf_Send[128];
__IO static uint8_t usart_send_len = 0;
__IO static uint8_t usart_send_index = 0;
__IO uint8_t usart_recvSuccess_flag = 1;

uint8_t recv_buf[128] = { 0 };
uint8_t recv_len = 0;
uint8_t recv_real_buf[128] = { 0 };
uint8_t recv_real_len = 0;
uint8_t recv_flag = 0;
uint8_t usart_led_state = 0;

__IO static uint8_t send_busy = 0;   
static uint32_t g_usart_baudrate = 19200U;


void usart_485_CS(uint8_t cs);

void usart_set_baudrate(uint32_t baudrate)
{
    g_usart_baudrate = baudrate;
}

void usart_init(void)
{
    nvic_irq_enable(USART1_IRQn, 5, 0);
    rcu_periph_clock_enable(USARTX_RCU);
    rcu_periph_clock_enable(USART_PIN_RCU);
    rcu_periph_clock_enable(USART_485_CS_RCU);

    gpio_mode_set(USART_485_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLDOWN, USARTX_485_CS_Pin);
    gpio_output_options_set(USART_485_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USARTX_485_CS_Pin);

    gpio_af_set(USART_PORT, GPIO_AF_7, USART_TX_Pin);
    gpio_mode_set(USART_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, USART_TX_Pin);
    gpio_output_options_set(USART_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART_TX_Pin);

    gpio_af_set(USART_PORT, GPIO_AF_7, USART_RX_Pin);
    gpio_mode_set(USART_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, USART_RX_Pin);
    gpio_output_options_set(USART_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART_RX_Pin);

    usart_deinit(USART);
    usart_baudrate_set(USART, g_usart_baudrate);
    usart_transmit_config(USART, USART_TRANSMIT_ENABLE);
    usart_receive_config(USART, USART_RECEIVE_ENABLE);

    usart_interrupt_enable(USART, USART_INT_RBNE);
    usart_interrupt_enable(USART, USART_INT_IDLE);
    usart_enable(USART);
}

void usart_send_str(uint8_t* str, uint8_t len)
{
    if (len > sizeof(Buf_Send))
    {
        return;
    }

    while (send_busy);
    send_busy = 1;

    usart_485_CS(USARTX_485_Send);

    usart_interrupt_disable(USART, USART_INT_RBNE);
    usart_interrupt_disable(USART, USART_INT_IDLE);
    memcpy(Buf_Send, str, len);
    usart_send_len = len;
    usart_interrupt_enable(USART, USART_INT_TBE);
}

uint8_t usart_is_send_busy(void)
{
    return send_busy;
}
void usart_recv_buf(void)
{
    if (recv_flag)
    {
        Protocol_Process(recv_real_buf, recv_real_len);
        recv_flag = 0;
    }
}

void usart_485_CS(uint8_t cs)
{
    if (cs == 1)
    {
        gpio_bit_set(USART_485_CS_PORT, USARTX_485_CS_Pin);
    }
    else
    {
        gpio_bit_reset(USART_485_CS_PORT, USARTX_485_CS_Pin);
    }

}

void USART1_IRQHandler(void)
{
    if (usart_interrupt_flag_get(USART, USART_INT_FLAG_TBE))
    {
        if (usart_send_index < usart_send_len)
        {
            usart_data_transmit(USART, Buf_Send[usart_send_index]);
            usart_send_index++;
        }
        else
        {
            Buf_Send[usart_send_index] = '\0';
            usart_send_index = 0;
            usart_send_len = 0;
            usart_interrupt_disable(USART, USART_INT_TBE);
            usart_interrupt_enable(USART, USART_INT_TC);
        }
    }

    if (usart_interrupt_flag_get(USART, USART_INT_FLAG_TC))
    {
        usart_interrupt_flag_clear(USART, USART_INT_FLAG_TC);
        usart_interrupt_disable(USART, USART_INT_TC);
        usart_485_CS(USARTX_485_Receive);
        usart_interrupt_enable(USART, USART_INT_RBNE);
        usart_interrupt_enable(USART, USART_INT_IDLE);
        send_busy = 0;   
    }

    if (usart_interrupt_flag_get(USART, USART_INT_FLAG_RBNE) != RESET)
    {
        usart_recvSuccess_flag = 0;
        if (recv_len < sizeof(recv_buf))
        {
            recv_buf[recv_len++] = usart_data_receive(USART);
        }
    }

    if (usart_interrupt_flag_get(USART, USART_INT_FLAG_IDLE) != RESET)
    {
        uint8_t clean_len;


        (void)usart_data_receive(USART);

        clean_len = recv_len;


        while ((clean_len > 0U) &&
               ((recv_buf[clean_len - 1U] == '\r') ||
                (recv_buf[clean_len - 1U] == '\n') ||
                (recv_buf[clean_len - 1U] == ' ')))
        {
            clean_len--;
        }

        if (clean_len != 0U)
        {
            memcpy(recv_real_buf, recv_buf, clean_len);
            recv_real_len = clean_len;

            if (recv_real_len < sizeof(recv_real_buf))
            {
                recv_real_buf[recv_real_len] = '\0';
            }
            else
            {
                recv_real_buf[sizeof(recv_real_buf) - 1U] = '\0';
            }

            recv_flag = 1U;
        }

        recv_len = 0U;
        usart_recvSuccess_flag = 1U;
    }
}


