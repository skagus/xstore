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
#include "esp_log.h"
#include "driver/uart.h"

#include "esp_system.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"

#include "xmodem.h"
#include "uart.h"
#include "dos.h"
#include "cli.h"

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

void _dbg_dump(uint8_t argc,char* argv[])
{
	for (uint32_t nIdx = 0; nIdx < nDbgIdx; nIdx++)
	{
		printf("{%d: %02X}\n", aKey[nIdx], aVal[nIdx]);
	}
}



void app_main(void)
{
#if 1
	uart_set_baudrate(CONFIG_ESP_CONSOLE_UART_NUM, 115200);
	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);
	ESP_ERROR_CHECK(uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0));
	esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
	esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
	esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CRLF);
#endif
	printf("\n\nHello world!\n");

	/* Print chip information */
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	printf("This is ESP8266 chip with %d CPU cores, WiFi, ",
		   chip_info.cores);

	printf("\n\nHello world!2222\n");

	CLI_Init();
	DOS_Init();

	CLI_Register("dbg",_dbg_dump);

	int nCnt = 0;
	while(1)
	{
//		printf("In Main: %d\n",nCnt);
		nCnt++;
		vTaskDelay(100);
	}

}


