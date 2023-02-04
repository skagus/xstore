#include <stdint.h>

#define SEC(x)			((x) * 1000)


int16_t UART_RxByte(uint32_t nDelay);
void UART_TxByte(uint8_t nData);
int UART_RxData(uint8_t* aBuf, uint32_t nBufSize, uint32_t nDelay);
void UART_TxData(uint8_t* aBuf, uint32_t nBufSize);



#define _inbyte(x)		UART_RxByte(x)
#define _outbyte(x)		UART_TxByte(x)
