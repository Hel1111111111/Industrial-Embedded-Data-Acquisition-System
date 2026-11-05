/************************* 头文件 *************************/
#include "usart0.h"
#include "usart.h"
#include "stdio.h"

/************************* 宏定义 *************************/

/************************ 变量定义 ************************/

uint8_t usart0_recv_buf[128] = { 0 };
uint8_t usart0_recv_len = 0;

uint8_t usart0_real_recv_buf[128] = { 0 };
uint8_t usart0_real_recv_len = 0;

uint8_t usart0_recv_flag = 0;


void my_usart_init(void) {

    //! 首先开启对应的中断处理函数 
    nvic_irq_enable(USART0_IRQn, 3, 3);

    //! 使能USART时钟
    rcu_periph_clock_enable(RCU_USART0);
    rcu_periph_clock_enable(RCU_GPIOA);

    // 对TX配置成为复用模式
    gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_9);

    // 设置GPIO引脚模式
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_9);
    // 设置GPIO引脚参数
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);

    // 对RX配置成为复用模式
    gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_10);

    // 设置GPIO引脚模式
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_10);
    // 设置GPIO引脚参数
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10);

    // 配置USART
    usart_deinit(USART0);
    usart_baudrate_set(USART0, 115200U);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);


    usart_interrupt_enable(USART0, USART_INT_RBNE);
    usart_interrupt_enable(USART0, USART_INT_IDLE);

    usart_enable(USART0);

}


int fputc(int ch, FILE* f)
{
    usart_data_transmit(USART0, (uint8_t)ch);

    while (usart_flag_get(USART0, USART_FLAG_TBE) == RESET)
    {
        ;
    }
    return ch;
}


void USART0_IRQHandler(void)
{
    // usart0_recv_flag = 1;
    if (usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE) != RESET)
    {
        if (usart0_recv_len < sizeof(usart0_recv_buf)) {
            usart0_recv_buf[usart0_recv_len++] = usart_data_receive(USART0);
        }
    }
    //! 进行不定长数据的接收
    if (usart_interrupt_flag_get(USART0, USART_INT_FLAG_IDLE) != RESET && usart0_recv_len != 0)
    {
        memcpy(usart0_real_recv_buf, usart0_recv_buf, usart0_recv_len);
        usart0_real_recv_len = usart0_recv_len;
        usart0_recv_len = 0;
        usart0_recv_flag = 1;
        //清除标记位
        usart_data_receive(USART0);
    }
}

/****************************End*****************************/


