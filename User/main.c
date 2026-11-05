/************************* 头文件 *************************/

#include "HeaderFiles.h"



/************************ 执行函数 ************************/

#define APP_START_ADDR 0x08011000U

int main(void)
{
		printf("aaaaa");
    SCB->VTOR = APP_START_ADDR;
	__enable_irq();
	System_Init();   // 系统初始化
	
	UsrFunction();   // 执行用户函数
	
}


/****************************End*****************************/
