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

#include "driver/usb_serial_jtag.h"
#include "esp_chip_info.h"
#include "esp_console.h"

#include "ymodem.h"
#include "uart.h"
#include "dos.h"
#include "cli.h"

static const char *TAG = "APP";

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

int gnCnt;
#define BUF_SIZE		(32)
void dummyTask(void* pParam)
{
	UART_Init();

	char aBuf[BUF_SIZE];

	while(1)
	{
#if 1
		int len = usb_serial_jtag_read_bytes(aBuf,BUF_SIZE,500 / portTICK_PERIOD_MS);
		if( len > 0)
		{
			usb_serial_jtag_write_bytes((const char*)aBuf, len,500 / portTICK_PERIOD_MS);
//			uart_flush(UBRIDGE_UART_PORT_NUM);
		}
#elif 0
		char nCh;
		// Write data back to the USB SERIAL JTAG
		if(UART_RxByte(&nCh, 0) > 0)
		{
			UART_TxByte(nCh);
		}
#endif
		gnCnt++;
//		vTaskDelay(140);
	}
}

#include "uart.h"

void app_main(void)
{
	vTaskDelay(100);

	UART_Init();
	CLI_Init();
	DOS_Init();
	YM_Init();

#if 0
	printf("Hello world! It is %s: %s\n",__DATE__,__TIME__);

	/* Print chip information */
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	printf("This is ESP32c3 chip with %d CPU cores, WiFi\n", chip_info.cores);

	UART_Init();
	printf("Checked: %s: %d\n",__FILE__,__LINE__);
#endif

//	xTaskCreate(dummyTask,"Dummy", 4096,(void*)1,tskIDLE_PRIORITY,NULL);

	char aBuf[BUF_SIZE];
	int nCnt = 0;
	while(1)
	{
		nCnt++;
		vTaskDelay(100);
//		UART_Printf("In Main: %d : %d\n",nCnt,gnCnt);
	}
}


