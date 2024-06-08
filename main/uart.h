#include <stdint.h>
#include "freertos/FreeRTOS.h"

#define printf	UART_Printf
#define putchar UART_TxByte

//int16_t UART_RxByte(uint32_t nDelay);
uint32_t UART_RxByte(char* pnRet, uint32_t nTimeOut);
void UART_TxByte(uint8_t nData);
int UART_RxData(uint8_t* aBuf, uint32_t nBufSize, uint32_t nDelay);
void UART_TxData(uint8_t* aBuf, uint32_t nBufSize);
void UART_Printf(const char* pFmt, ...);
void UART_Init();

