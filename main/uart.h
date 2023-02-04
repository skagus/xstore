#include <stdint.h>
#include "freertos/FreeRTOS.h"

#define SEC(x)			((x) * configTICK_RATE_HZ)
#define MSEC(x)			((x) * configTICK_RATE_HZ / 1000)

int16_t UART_RxByte(uint32_t nDelay);
void UART_TxByte(uint8_t nData);
int UART_RxData(uint8_t* aBuf, uint32_t nBufSize, uint32_t nDelay);
void UART_TxData(uint8_t* aBuf, uint32_t nBufSize);


