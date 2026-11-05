#ifndef __USART_H__
#define __USART_H__

/************************* 头文件 *************************/
#include "HeaderFiles.h"

/************************* 宏定义 *************************/
#define USART_PORT GPIOD
#define USART USART1
#define USART_TX_Pin GPIO_PIN_5
#define USART_RX_Pin GPIO_PIN_6
#define USARTX_RCU RCU_USART1
#define USART_PIN_RCU RCU_GPIOD


#define USART_485_CS_RCU RCU_GPIOE
#define USART_485_CS_PORT GPIOE
#define USARTX_485_CS_Pin GPIO_PIN_8

#define USARTX_485_Send 1
#define USARTX_485_Receive 0



/************************ 变量定义 ************************/


/************************ 函数定义 ************************/

void usart_set_baudrate(uint32_t baudrate);
void usart_init(void);
void usart_send_str(uint8_t* str, uint8_t len);
void usart_recv_buf(void);
uint8_t usart_is_send_busy(void);
#endif // !__USART_H__
/****************************End*****************************/
