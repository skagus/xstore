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
#include "console.h"

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
	for (uint32_t nIdx = 0; nIdx < nDbgIdx; nIdx++)
	{
		printf("{%d: %02X}\n", aKey[nIdx], aVal[nIdx]);
	}
}

void buf_dump(uint8_t* pBuf, uint32_t nSize)
{
	for (uint32_t nIdx = 0; nIdx < nSize; nIdx++)
	{
		if(0 == (nIdx % 32))
			printf("\n %4X: ", nIdx / 32);
		printf("%2X ", pBuf[nIdx]);
	}
}

uint8_t aRBuf[BUF_SIZE];
uint8_t aTBuf[BUF_SIZE];

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

	for(int i=0; i< BUF_SIZE; i++)
	{
		aTBuf[i] = 0xCC;
		if(0 == (i % 32)) aTBuf[i] = (i / 32);
	}
	char aLine[32];
	char* aArgs[MAX_CMD_TOKEN];
	while (true)
	{
		uint32_t nLen = CON_GetLine(aLine, 32);
		if(nLen <= 0) continue;

		printf("CMD:%s:", aLine);
		CON_Token(aLine, aArgs);

		int nRet = 0;
		if(CMD_COMP(aArgs[0], "rbuf"))
		{
			printf("Dump R Buf\n");
			buf_dump(aRBuf, BUF_SIZE);
			printf("\n");
		}
		else if(CMD_COMP(aArgs[0], "tbuf"))
		{
			printf("Dump S Buf\n");
			buf_dump(aTBuf, BUF_SIZE);
			printf("\n");
		}
		else if(CMD_COMP(aArgs[0], "tx"))
		{
			printf("Start to T -> PC\n");
			nRet = XM_Transmit(aTBuf, BUF_SIZE);
			printf("Ret: %d\n", nRet);
		}
		else if(CMD_COMP(aArgs[0], "rx"))
		{
			printf("Start to PC -> T\n");
			nRet = XM_Receive(aRBuf, BUF_SIZE);
			printf("Ret: %d\n", nRet);
		}
		else if(CMD_COMP(aArgs[0], "dbg"))
		{
			printf("Debug dump\n");
			dbg_Dump();
		}
		else
		{
			printf("supported cmd : buf, tx, rx, dbg \n");
		}
	}
}


