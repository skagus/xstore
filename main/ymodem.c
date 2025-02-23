#include <stdint.h>
#include <stdbool.h>
#include "esp_littlefs.h"
#include "esp_vfs_dev.h"
#include "uart.h"
#include "crc16.h"
#include "cli.h"
#include "ymodem.h"



#define MODEM_SOH			0x01	// Start of Header(for 128 block)
#define MODEM_STX			0x02	// Start of Text(for 1024 block)
#define MODEM_ACK			0x06	// Acknowledge
#define MODEM_NACK			0x15	// Negative Acknowledge
#define MODEM_EOT			0x04	// End of Transmission
#define MODEM_C		 		0x43	// 'C' for sending file
#define MODEM_CAN			0x18	// Cancel


#define MODEM_SEQ_SIZE		2
#define DATA_SIZE_ORG		128
#define DATA_SIZE_EXT		1024
#define MODEM_CRC_SIZE		2
#define TIMEOUT				100

#define LOG_NAME		"/fs/ym.log"

uint8_t aBuf[MODEM_SEQ_SIZE + DATA_SIZE_EXT + MODEM_CRC_SIZE + 10];

void log_init(char* szpath)
{
//	gf_log = fopen(szpath, "w");
}

void log_printf(const char* pFmt, ...)
{
#if 0
	FILE* gf_log;
	gf_log = fopen(LOG_NAME, "a");

	va_list args;
	va_start(args, pFmt);
	vfprintf(gf_log, pFmt, args);
	va_end(args);

	fclose(gf_log);
#endif
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
		log_printf("%d: TO exp:%X\n", __LINE__, nExp);
		return false;
	}
	log_printf("%d: Exp %X, Rcv: %X\n", __LINE__, nExp, nCh);
	if(nCh != nExp)
	{
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

bool _ParityCheck(bool bCRC, const uint8_t* aBuf, int nDataSize)
{
	if(bCRC)
	{
		uint16_t crc = CRC16_ccitt(aBuf, nDataSize);
		uint16_t tcrc = (aBuf[nDataSize] << 8) + aBuf[nDataSize + 1];
		if(crc == tcrc) return true;
	}
	else
	{
		uint8_t nChkSum = 0;
		for(int i = 0; i < nDataSize; ++i)
		{
			nChkSum += aBuf[i];
		}
		if(nChkSum == aBuf[nDataSize]) return true;
	}

	return false;
}


void YM_Rcv(char* szTopDir)
{
	uint8_t* pData = &aBuf[MODEM_SEQ_SIZE];
	char aFullPath[256];
	uint8_t nCh;
	uint8_t nSeq = 0;
	uint32_t nDataLen;
	FILE* pFile = NULL;

	uint32_t nDirLen = strlen(szTopDir);
	strcpy(aFullPath, szTopDir);
	if(aFullPath[nDirLen - 1] != '/')
	{
		aFullPath[nDirLen++] = '/';
		aFullPath[nDirLen] = 0;
	}
	printf("Start RX to %s(%d)\n", aFullPath, nDirLen);

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

	// log_printf("%3d, LEN: %d\n", __LINE__, nDataLen);
	// Receive Header.
	if(rcvPacket(aBuf, MODEM_SEQ_SIZE + nDataLen + MODEM_CRC_SIZE) == false) goto ERROR;

	if(aBuf[0] != nSeq) goto ERROR;
	if(aBuf[1] != (0xFF - nSeq)) goto ERROR;
	if(!_ParityCheck(true, pData, nDataLen)) goto ERROR;
	// do something with file name.
	strcpy(aFullPath + nDirLen, (char*)pData);
	int fileSize = atoi((char*)pData + strlen((char*)pData) + 1);
	
	log_printf("%3d: file name: %s, size: %s, %d\n",
		__LINE__, pData, pData + strlen((char*)pData) + 1, fileSize);

	if(fileSize <= 0) fileSize = INT32_MAX;

	pFile = fopen(aFullPath, "w");
	if(pFile == NULL) goto ERROR;
	////////
	UART_TxByte(MODEM_ACK);
	UART_TxByte(MODEM_C);
	nSeq++;

	while(true)
	{
		if(UART_RxByte((char*)&nCh, TIMEOUT) == 0) goto ERROR;

		if(nCh == MODEM_EOT)
		{
			fclose(pFile);
			pFile = NULL;
			break;
		}
		else if(nCh == MODEM_SOH) nDataLen = DATA_SIZE_ORG;
		else if(nCh == MODEM_STX) nDataLen = DATA_SIZE_EXT;
		else goto ERROR; // if(nCh == MODEM_CAN) goto ERROR;

		if(!rcvPacket(aBuf, MODEM_SEQ_SIZE + nDataLen + MODEM_CRC_SIZE)) goto ERROR;
		if(aBuf[0] != nSeq) goto ERROR;
		if(aBuf[1] != (0xFF - nSeq)) goto ERROR;
		if(!_ParityCheck(true, pData, nDataLen)) goto ERROR;
		// do something with data.
		if(fileSize < nDataLen)
		{
			nDataLen = fileSize;
		}
		fwrite(pData, 1, nDataLen, pFile);
		///
		UART_TxByte(MODEM_ACK);
		log_printf("%3d SEQ: %d\n", __LINE__, nSeq);
		nSeq++;
		fileSize -= nDataLen;
	}

	log_printf("%3d\n", __LINE__);

	/// End Sequence.
	UART_TxByte(MODEM_NACK);
	if(!expactRcvByte(MODEM_EOT)) goto ERROR;
	UART_TxByte(MODEM_ACK);

	log_printf("%3d\n", __LINE__);

	// Receive Last Packet.
	UART_TxByte(MODEM_C);
	if(!expactRcvByte(MODEM_SOH)) goto ERROR;
	log_printf("%3d\n", __LINE__);
	if(!rcvPacket(aBuf, MODEM_SEQ_SIZE + DATA_SIZE_ORG + MODEM_CRC_SIZE)) goto ERROR;
	if(aBuf[0] != 0x00) goto ERROR;
	if(aBuf[1] != 0xFF) goto ERROR;
	UART_TxByte(MODEM_ACK);

	log_printf("%3d: Success\n", __LINE__);

	printf("\n\nSuccess\n");
	return;
	
ERROR:
	if(pFile != NULL) fclose(pFile);
	UART_TxByte(MODEM_CAN);
	UART_TxByte(MODEM_CAN);
	UART_TxByte(MODEM_CAN);
	log_printf("%3d: Error!!!\n", __LINE__);
	printf("\n\nError\n");
}


void _ParityGen(bool bCRC, uint8_t* aBuf, int nDataSize)
{
	if(bCRC)
	{
		uint16_t crc = CRC16_ccitt(aBuf, nDataSize);
		aBuf[nDataSize] = crc >> 8;
		aBuf[nDataSize + 1] = crc & 0xFF;
	}
	else
	{
		uint8_t nChkSum = 0;
		for(int i = 0; i < nDataSize; ++i)
		{
			nChkSum += aBuf[i];
		}
		aBuf[nDataSize] = nChkSum;
	}
}

void sendPacket(uint8_t* aBuf, uint32_t nSize)
{
	UART_TxData(aBuf, nSize);
}

int _getFileSize(char* szPath)
{
	struct stat st;
	if(stat(szPath, &st) != 0) return -1;
	return st.st_size;
}
	
void YM_Snd(char* szFullPath)
{
	uint8_t* pData = &aBuf[MODEM_SEQ_SIZE];
	uint8_t nSeq = 0;
	uint32_t nSpaceLen = DATA_SIZE_ORG;
	uint32_t nDataLen;
	FILE* pFile;
	int nRestSize = _getFileSize(szFullPath);
	pFile = fopen(szFullPath, "r");
	if(pFile == NULL) goto ERROR;

	char* szFileName = strrchr(szFullPath, '/');
	if(szFileName == NULL) szFileName = szFullPath;
	else szFileName++;

	printf("Start TX: %s\n", szFullPath);

	// Wait Start.
	int nRestTry = 20;
	while(nRestTry-- > 0)
	{
		if(expactRcvByte(MODEM_C)) break;
	}
	if(nRestTry <= 0) goto ERROR;

	// Prepare Header.
	aBuf[0] = nSeq;
	aBuf[1] = 0xFF - nSeq;
	nDataLen = strlen(szFileName);
	memcpy(pData, szFileName, nDataLen);
	sprintf((char*)pData + nDataLen + 1, "%d", nRestSize);
//	memset(pData + nDataLen, 0, nSpaceLen - nDataLen);
	_ParityGen(true, pData, nSpaceLen);
	UART_TxByte(MODEM_SOH);
	sendPacket(aBuf, MODEM_SEQ_SIZE + nSpaceLen + MODEM_CRC_SIZE);
	if(!expactRcvByte(MODEM_ACK)) goto ERROR;
	if(!expactRcvByte(MODEM_C)) goto ERROR;

	nSeq++;
	while(true)
	{
		nSpaceLen = DATA_SIZE_EXT;
		aBuf[0] = nSeq;
		aBuf[1] = 0xFF - nSeq;
		nDataLen = fread(pData, 1, nSpaceLen, pFile);
		nRestSize -= nDataLen;
		if(nDataLen <= 0)
		{
			fclose(pFile);
			pFile = NULL;
			break;
		}
#if 0
		if(nDataLen < DATA_SIZE_ORG)
		{
			nSpaceLen = DATA_SIZE_ORG;
		}
#endif
//		memset(pData + nDataLen, 0, nSpaceLen - nDataLen);
		_ParityGen(true, pData, nSpaceLen);
		UART_TxByte(MODEM_STX);
		sendPacket(aBuf, MODEM_SEQ_SIZE + nSpaceLen + MODEM_CRC_SIZE);
		if(!expactRcvByte(MODEM_ACK)) goto ERROR;
		nSeq++;
	}

	// End Sequence.
	UART_TxByte(MODEM_EOT);
	if(!expactRcvByte(MODEM_NACK)) goto ERROR;
	UART_TxByte(MODEM_EOT);
	if(!expactRcvByte(MODEM_ACK)) goto ERROR;
	if(!expactRcvByte(MODEM_C)) goto ERROR;

	// Send Last Packet.
	aBuf[0] = 0x00;
	aBuf[1] = 0xFF;
	UART_TxByte(MODEM_SOH);
	memset(pData, 0, DATA_SIZE_ORG);
	_ParityGen(true, pData, DATA_SIZE_ORG);
	sendPacket(aBuf, MODEM_SEQ_SIZE + DATA_SIZE_ORG + MODEM_CRC_SIZE);
	if(!expactRcvByte(MODEM_ACK)) goto ERROR;

	log_printf("%3d: Success\n", __LINE__);

	printf("\n\nSuccess\n");
	return;

ERROR:
	UART_TxByte(MODEM_CAN);
	UART_TxByte(MODEM_CAN);
	UART_TxByte(MODEM_CAN);
	log_printf("%3d: Error!!!\n", __LINE__);
	printf("\n\nError\n");
}

void cmd_RcvY(uint8_t argc,char* argv[])
{
	if(argc < 2)
	{
		printf("Needs top directory\n");
		return;
	}
	YM_Rcv(argv[1]);
}

void cmd_SndY(uint8_t argc,char* argv[])
{
	if(argc < 2)
	{
		printf("Needs file name with path\n");
		return;
	}
	YM_Snd(argv[1]);
}

void cmd_Test(uint8_t argc,char* argv[])
{
	uint8_t* pBuf = &(aBuf[MODEM_SEQ_SIZE + DATA_SIZE_EXT + MODEM_CRC_SIZE]);
	printf("Test: %2X %2X\n", pBuf[0], pBuf[1]);
}

void YM_Init()
{
	CLI_Register("ymt", cmd_Test);
	CLI_Register("ry", cmd_RcvY);
	CLI_Register("sy", cmd_SndY);
}

