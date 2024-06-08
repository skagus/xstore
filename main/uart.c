
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "driver/uart.h"
#include "driver/usb_serial_jtag.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_cdcacm.h"
#include "uart.h"

#define CFG_USB_CDC		(1)


#if 0
uint32_t UART_RxByte(char* pnRet, uint32_t nTimeOut)
{
	return usb_serial_jtag_read_bytes(pnRet, 1, nTimeOut);
}

void UART_TxByte(uint8_t nData)
{
	usb_serial_jtag_write_bytes((const char*)&nData, 1, 20 / portTICK_PERIOD_MS);
}

int UART_RxData(uint8_t* aBuf,uint32_t nBufSize,uint32_t nDelay)
{
	return usb_serial_jtag_read_bytes(aBuf, nBufSize, nDelay);
}

void UART_TxData(uint8_t* aBuf,uint32_t nSize)
{
	usb_serial_jtag_write_bytes((const char*)aBuf,nSize, 20 / portTICK_PERIOD_MS);
}
#endif

#define CONFIG_ESP_CONSOLE_UART_NUM		(0)
#include <fcntl.h>
void UART_Init()
{
#if CFG_USB_CDC
	usb_serial_jtag_driver_config_t usb_serial_config = {.tx_buffer_size = 128,
		.rx_buffer_size = 128};

	ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_config));
#else
	setvbuf(stdin,NULL,_IONBF,0);
	setvbuf(stdout,NULL,_IONBF,0);
	ESP_ERROR_CHECK(uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM,256,0,0,NULL,0));
	esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
	esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM,ESP_LINE_ENDINGS_CR);
	esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM,ESP_LINE_ENDINGS_CRLF);
#endif
}

uint32_t UART_RxByte(char* pnRet, uint32_t nTimeOut)
{
#if CFG_USB_CDC
	int nCnt = usb_serial_jtag_read_bytes(pnRet,1,nTimeOut);
#else
	int nCnt = uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM,&pnRet,1,nTimeOut);
#endif
	if (nCnt > 0)
	{
		return nCnt;
	}
	return 0;
}

void UART_TxByte(uint8_t nData)
{
#if CFG_USB_CDC
	usb_serial_jtag_write_bytes(&nData,1,500 / portTICK_PERIOD_MS);
#else
	uart_tx_chars(CONFIG_ESP_CONSOLE_UART_NUM, (char*)&nData, 1);
#endif
}

int UART_RxData(uint8_t *aBuf, uint32_t nBufSize, uint32_t nDelay)
{
#if CFG_USB_CDC
	return usb_serial_jtag_read_bytes(aBuf,nBufSize,nDelay);
#else
	return uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, aBuf, nBufSize, nDelay);
#endif
}

void UART_TxData(uint8_t *aBuf, uint32_t nSize)
{
#if CFG_USB_CDC
	usb_serial_jtag_write_bytes(aBuf,nSize,500 / portTICK_PERIOD_MS);
#else
	uart_tx_chars(CONFIG_ESP_CONSOLE_UART_NUM, (char*)aBuf, nSize);
#endif
}

void UART_Printf(const char* pFmt, ...)
{
	uint8_t aBuf[128];

	va_list arg_ptr;
	va_start(arg_ptr,pFmt);
	int nLen = vsprintf((char*)aBuf,pFmt,arg_ptr);
	va_end(arg_ptr);

	UART_TxData(aBuf, nLen);
}