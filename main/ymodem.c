#include <stdint.h>
#include <stdbool.h>
#include "uart.h"
#include "cli.h"

#include "esp_littlefs.h"
#include "esp_vfs_dev.h"


#define MODEM_SOH    0x01	// Start of Header(for 128 block)
#define MODEM_STX    0x02	// Start of Text(for 1024 block)
#define MODEM_ACK    0x06	// Acknowledge
#define MODEM_NACK   0x15	// Negative Acknowledge
#define MODEM_EOT    0x04	// End of Transmission
#define MODEM_C      0x43	// 'C' for sending file
#define MODEM_CAN    0x18	// Cancel


#define MODEM_SEQ_SIZE		2
#define DATA_SIZE_ORG		128
#define DATA_SIZE_EXT		1024
#define MODEM_CRC_SIZE		2
#define TIMEOUT			1000

#define LOG_NAME		"/fs/ymodem.log"


FILE* gf_log;
void log_init(char* szpath)
{
//	gf_log = fopen(szpath, "w");
}

void log_printf(const char* pFmt, ...)
{
	gf_log = fopen(LOG_NAME, "a");

	va_list args;
	va_start(args, pFmt);
	vfprintf(gf_log, pFmt, args);
	va_end(args);

	fclose(gf_log);
}

void log_close()
{
//	fclose(gf_log);
}


bool expactRcvByte(uint8_t nExp)
{
	uint8_t nCh;
	if(UART_RxByte((char*)&nCh, TIMEOUT) == 0)
	{
		log_printf("%d: TO\n", __LINE__);
		return false;
	}
	if(nCh != nExp)
	{
		log_printf("%d: Exp %X, Rcv: %X\n", __LINE__, nExp, nCh);
		return false;
	}

	return true;
}

bool rcvPacket(uint8_t* aBuf, uint32_t nSize)
{
	uint8_t nCh;
	uint32_t nIdx = 0;
	while(nIdx < nSize)
	{
		if(UART_RxByte((char*)&nCh, 2 * TIMEOUT) == 0)
		{
			log_printf("%d: TO %d/%d\n", __LINE__, nIdx, nSize);
			for(uint32_t i = 0; i < nIdx; i++)
			{
				log_printf("%2X ", aBuf[i]);
			}
			log_printf("\n");
			return false;
		}
		aBuf[nIdx] = nCh;
		nIdx++;
	}
	return true;
}

uint8_t aBuf[MODEM_SEQ_SIZE + DATA_SIZE_EXT + MODEM_CRC_SIZE];

void YM_Rcv()
{
	uint8_t nCh;
	uint8_t nSeq = 0;
	uint8_t* pData = &aBuf[2];
	uint32_t nDataLen;

	log_init("/fs/ymodem.log");

	log_printf("%3d, Start!!!!\n", __LINE__);
	// Wait Start.
	while(true)
	{
		UART_TxByte(MODEM_C);
		if(UART_RxByte((char*)&nCh, TIMEOUT))
		{
			if(nCh == MODEM_SOH) nDataLen = DATA_SIZE_ORG;
			else if(nCh == MODEM_STX) nDataLen = DATA_SIZE_EXT;
			else goto ERROR;
			break;
		}
	}

	log_printf("%3d, LEN: %d\n", __LINE__, nDataLen);
	// Receive Header.
	if(rcvPacket(aBuf, MODEM_SEQ_SIZE + nDataLen + MODEM_CRC_SIZE) == false) goto ERROR;
	
//	if(rcvPacket(aBuf, MODEM_SEQ_SIZE + nDataLen + MODEM_CRC_SIZE) == false) {}

	if(aBuf[0] != nSeq) goto ERROR;
	if(aBuf[1] != (0xFF - nSeq)) goto ERROR;
	// do something with file name.
	UART_TxByte(MODEM_ACK);
	UART_TxByte(MODEM_C);
	log_printf("%3d SEQ: %d\n", __LINE__, nSeq);
	nSeq++;

	while(true)
	{
		if(UART_RxByte((char*)&nCh, TIMEOUT) == 0) goto ERROR;

		if(nCh == MODEM_EOT) break;
		else if(nCh == MODEM_SOH) nDataLen = DATA_SIZE_ORG;
		else if(nCh == MODEM_STX) nDataLen = DATA_SIZE_EXT;
		else goto ERROR; // if(nCh == MODEM_CAN) goto ERROR;

		if(rcvPacket(aBuf, MODEM_SEQ_SIZE + nDataLen + MODEM_CRC_SIZE) == false) goto ERROR;
		if(aBuf[0] != nSeq) goto ERROR;
		if(aBuf[1] != (0xFF - nSeq)) goto ERROR;
		// do something with data.
		UART_TxByte(MODEM_ACK);
		log_printf("%3d SEQ: %d\n", __LINE__, nSeq);
		nSeq++;
	}

	log_printf("%3d\n", __LINE__);

	/// End Sequence.
	UART_TxByte(MODEM_NACK);
	if(expactRcvByte(MODEM_EOT) == false) goto ERROR;
	UART_TxByte(MODEM_ACK);
	UART_TxByte(MODEM_C);

	log_printf("%3d\n", __LINE__);

	// Receive Last Packet.
	if(expactRcvByte(MODEM_SOH) == false) goto ERROR;
	if(rcvPacket(aBuf, MODEM_SEQ_SIZE + DATA_SIZE_ORG + MODEM_CRC_SIZE) == false) goto ERROR;
	if(aBuf[0] != 0x00) goto ERROR;
	if(aBuf[1] != 0xFF) goto ERROR;
	UART_TxByte(MODEM_ACK);

	log_printf("%3d\n", __LINE__);
	log_close();
	return;
	
ERROR:
	UART_TxByte(MODEM_CAN);
	UART_TxByte(MODEM_CAN);
	UART_TxByte(MODEM_CAN);
	log_printf("%3d: Error!!!\n", __LINE__);
}

void YM_Snd()
{

}

void cmd_RcvY(uint8_t argc,char* argv[])
{
	YM_Rcv();
}

void cmd_SndY(uint8_t argc,char* argv[])
{
	YM_Snd();
}

void YM_Init()
{
	CLI_Register("ry", cmd_RcvY);
	CLI_Register("sy", cmd_SndY);
}

