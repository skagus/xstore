/* SPIFFS filesystem example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "driver/uart.h"

#include "esp_system.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"

#include "xmodem.h"
#include "uart.h"

static const char *TAG = "APP";

#define BUF_SIZE (3000)

#define DBG_BUG_SIZE (128)

uint16_t aKey[DBG_BUG_SIZE];
uint16_t aVal[DBG_BUG_SIZE];

uint32_t nDbgIdx;

void DBG_Add(uint16_t nKey, uint16_t nVal)
{
	aKey[nDbgIdx] = nKey;
	aVal[nDbgIdx] = nVal;
	nDbgIdx++;
}

void dbg_Dump()
{
	printf("DBG Dump\n");
	for (uint32_t nIdx = 0; nIdx < nDbgIdx; nIdx++)
	{
		printf("{%d: %02X}\n", aKey[nIdx], aVal[nIdx]);
	}
}

struct Ctx
{
	uint32_t nNext;
} gCtx;

uint8_t aXBuf[BUF_SIZE];

void prepareTxData(void *pCtx, void *pTxBuf, int nSize)
{
	uint8_t *pSrc = aXBuf + gCtx.nNext;
	memcpy(pTxBuf, pSrc, nSize);
	gCtx.nNext += nSize;
}

void getRxData(void *pCtx, void *pRxBuf, int nSize)
{
	// nothing.
}

void app_main(void)
{
	ESP_LOGI(TAG, "Start APP");
	//	uart_set_baudrate(CONFIG_ESP_CONSOLE_UART_NUM, 115200);
	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);
	ESP_ERROR_CHECK(uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0));
	esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
	esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
	esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CRLF);

	const char *szLine = "Hello World..\n";
	uart_tx_chars(CONFIG_ESP_CONSOLE_UART_NUM, szLine, strlen(szLine));

	int nRet = 0;
	for(int i=0; i< BUF_SIZE; i++)
	{
		aXBuf[i] = 0xCC;
		if(0 == (i % 128)) aXBuf[i] = 0xA0 + ((i / 128) % 16);
	}
	
	while (true)
	{
		int16_t nCmd = UART_RxByte(SEC(1));
		if(nCmd < 0) 
			continue;
		switch (nCmd)
		{
		case 'P':
		{
			printf("Dump X Buf with Ret: %d\n", nRet);
			for (uint32_t nIdx = 0; nIdx < BUF_SIZE; nIdx++)
			{
				printf("%X ", aXBuf[nIdx]);
			}
			printf("\n");
			break;
		}
		case 'S':
		{
			printf("Start Xmodem RX\n");
			gCtx.nNext = 0;
			nRet = xmodemTransmit(aXBuf, BUF_SIZE);
			break;
		}
		case 'R':
		{
			printf("Start Xmodem TX\n");
			gCtx.nNext = 0;
			nRet = xmodemReceive(aXBuf, BUF_SIZE);
			break;
		}
		case 'D':
		{
			dbg_Dump();
			break;
		}
		}
	}
}


