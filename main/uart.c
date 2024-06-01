#include "freertos/FreeRTOS.h"
#include "driver/uart.h"
#include "uart.h"

int16_t UART_RxByte(uint32_t nDelay)
{
	uint8_t nRcv = 0xFF;
	int nCnt = uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, &nRcv, 1, nDelay);
	if (nCnt > 0)
	{
		return nRcv;
	}
	return -1;
}

uint32_t UART_RxD(char* pnRet)
{
	uint8_t nRcv = 0xFF;
	int nCnt = uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM,&nRcv,1, 1);
	if(nCnt > 0)
	{
		*pnRet = (char)(nRcv & 0xFF);
		return 1;
	}
	return 0;
}

void UART_TxByte(uint8_t nData)
{
	uart_tx_chars(CONFIG_ESP_CONSOLE_UART_NUM, (char*)&nData, 1);
}

int UART_RxData(uint8_t *aBuf, uint32_t nBufSize, uint32_t nDelay)
{
	return uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, aBuf, nBufSize, nDelay);
}

void UART_TxData(uint8_t *aBuf, uint32_t nSize)
{
	uart_tx_chars(CONFIG_ESP_CONSOLE_UART_NUM, (char*)aBuf, nSize);
}
