#include <stdint.h>
#include <string.h>
#include "crc16.h"
#include "uart.h"
#include "xmodem.h"

#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18
#define CTRLZ 0x1A

#define MAXRETRANS 25
#define TRANSMIT_XMODEM_1K		(0)

static int check(int crc, const uint8_t *buf, int sz)
{
	if (crc)
	{
		uint16_t crc = CRC16_ccitt(buf, sz);
		uint16_t tcrc = (buf[sz] << 8) + buf[sz + 1];
		if (crc == tcrc)
			return 1;
	}
	else
	{
		int i;
		uint8_t cks = 0;
		for (i = 0; i < sz; ++i)
		{
			cks += buf[i];
		}
		if (cks == buf[sz])
			return 1;
	}

	return 0;
}

int XM_Receive(uint8_t* pBuf, int nReqSize)
{
	uint8_t aRxBuf[1030]; /* 1024 for XModem 1k + 3 head chars + 2 crc + nul */
	uint8_t *pRxPos;
	int nDataSize;
	int bCRC = 0;
	uint8_t trychar = 'C';
	uint8_t nRxPktNo = 1;
	int nTmp;
	int nRxByte = 0;
	int nRestChance = MAXRETRANS;

	while(1)
	{
		for (int nTry = 0; nTry < MAX_RETRY; ++nTry)
		{
			if (trychar)
				UART_TxByte(trychar);
			if ((nTmp = UART_RxByte(SEC(2))) >= 0)
			{
				switch (nTmp)
				{
				case SOH:
					nDataSize = 128;
					goto RECV;
				case STX:
					nDataSize = 1024;
					goto RECV;
				case EOT:
					while (UART_RxByte(MSEC(10)) >= 0){}
					UART_TxByte(ACK);
					return nRxByte; /* normal end */
				case CAN:
					if ((nTmp = UART_RxByte(SEC(1))) == CAN)
					{
						while (UART_RxByte(MSEC(10)) >= 0){}
						UART_TxByte(ACK);
						return -1; /* canceled by remote */
					}
					break;
				default:
					break;
				}
			}
		}
		if (trychar == 'C')
		{
			trychar = NAK;
			continue;
		}
		while (UART_RxByte(MSEC(10)) >= 0){}
		UART_TxByte(CAN);
		UART_TxByte(CAN);
		UART_TxByte(CAN);
		return -2; /* sync error */

	RECV:
		if (trychar == 'C')
			bCRC = 1;
		trychar = 0;
		pRxPos = aRxBuf;
		*pRxPos++ = nTmp;
		for (int i = 0; i < (nDataSize + bCRC + 3); i++)
		{
			if ((nTmp = UART_RxByte(SEC(2))) < 0)
				goto REJECT;
			*pRxPos++ = nTmp;
		}

		// Good packet.
		if (aRxBuf[1] == (uint8_t)(~aRxBuf[2]) &&
			(aRxBuf[1] == nRxPktNo || aRxBuf[1] == nRxPktNo - 1) &&
			check(bCRC, &aRxBuf[3], nDataSize))
		{
			if (aRxBuf[1] == nRxPktNo)
			{
				int nSizeData = nReqSize - nRxByte;
				if (nSizeData > nDataSize)
					nSizeData = nDataSize;
				if (nSizeData > 0)
				{
					memcpy(&pBuf[nRxByte], &aRxBuf[3], nSizeData);
					nRxByte += nSizeData;
				}
				nRxPktNo++; // Success.
				nRestChance = MAXRETRANS + 1;
			}
			if (--nRestChance <= 0)
			{
				while (UART_RxByte(MSEC(10)) >= 0){}
				UART_TxByte(CAN);
				UART_TxByte(CAN);
				UART_TxByte(CAN);
				return -3; /* too many retry error */
			}
			UART_TxByte(ACK);
			continue;
		}
	REJECT:
		while (UART_RxByte(MSEC(10)) >= 0){}
		UART_TxByte(NAK);
	}
}

int XM_Transmit(uint8_t *pReqBuf, int nReqSize)
{
	uint8_t anTxBuf[1030]; /* 1024 for XModem 1k + 3 head chars + 2 crc + nul */
	int nDataSize;
	int bCRC = 0;
	uint8_t nPktNo = 1;
	int nSent = 0;
	int nTmp;

	while(1)
	{
		for (int nTry = 0; nTry < 16; nTry++)
		{
			if ((nTmp = UART_RxByte(SEC(2))) >= 0)
			{
				switch (nTmp)
				{
				case 'C':
					bCRC = 1;
					goto TRANS;
				case NAK:
					bCRC = 0;
					goto TRANS;
				case CAN:
					if ((nTmp = UART_RxByte(SEC(1))) == CAN)
					{
						UART_TxByte(ACK);
						while (UART_RxByte(MSEC(10)) >= 0){}
						return -1; /* canceled by remote */
					}
					break;
				default:
					break;
				}
			}
		}
		UART_TxByte(CAN);
		UART_TxByte(CAN);
		UART_TxByte(CAN);
		while (UART_RxByte(MSEC(10)) >= 0){}
		return -2; /* no sync */

		while(1)
		{
		TRANS:
#if (TRANSMIT_XMODEM_1K == 1)
			anTxBuf[0] = STX;
			nDataSize = 1024;
#else
			anTxBuf[0] = SOH;
			nDataSize = 128;
#endif
			anTxBuf[1] = nPktNo;
			anTxBuf[2] = ~nPktNo;
			int nTxByte = nReqSize - nSent;
			if (nTxByte > nDataSize)
				nTxByte = nDataSize;
			if (nTxByte > 0)
			{
				memset(&anTxBuf[3], 0, nDataSize);
				memcpy(&anTxBuf[3], &pReqBuf[nSent], nTxByte);
				if (nTxByte < nDataSize)
					anTxBuf[3 + nTxByte] = CTRLZ;
				if (bCRC)
				{
					uint16_t ccrc = CRC16_ccitt(&anTxBuf[3], nDataSize);
					anTxBuf[nDataSize + 3] = (ccrc >> 8) & 0xFF;
					anTxBuf[nDataSize + 4] = ccrc & 0xFF;
				}
				else
				{
					uint8_t nChkSum = 0;
					for (int i = 3; i < nDataSize + 3; ++i)
					{
						nChkSum += anTxBuf[i];
					}
					anTxBuf[nDataSize + 3] = nChkSum;
				}
				for (int nTry = 0; nTry < MAXRETRANS; nTry++)
				{
					for (int i = 0; i < nDataSize + 4 + bCRC; i++)
					{
						UART_TxByte(anTxBuf[i]);
					}
					if ((nTmp = UART_RxByte(SEC(1))) >= 0)
					{
						switch (nTmp)
						{
						case ACK:
							++nPktNo;
							nSent += nDataSize;
							goto TRANS;
						case CAN:
							if ((nTmp = UART_RxByte(SEC(1))) == CAN)
							{
								UART_TxByte(ACK);
								while (UART_RxByte(MSEC(10)) >= 0){}
								return -1; /* canceled by remote */
							}
							break;
						case NAK:
						default:
							break;
						}
					}
				}
				UART_TxByte(CAN);
				UART_TxByte(CAN);
				UART_TxByte(CAN);
				while (UART_RxByte(MSEC(10)) >= 0){}
				return -4; /* xmit error */
			}
			else
			{
				for (int nTry = 0; nTry < 10; nTry++)
				{
					UART_TxByte(EOT);
					if ((nTmp = UART_RxByte(SEC(2))) == ACK)
						break;
				}
				while (UART_RxByte(MSEC(10)) >= 0){}
				return (nTmp == ACK) ? nSent : -5;
			}
		}
	}
	return -6;
}
