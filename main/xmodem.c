#include <stdint.h>
#include <string.h>
#include "macro.h"
#include "crc16.h"
#include "uart.h"
#include "cli.h"
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

int XM_Receive(TreatFunc fTreat, void* pCtx, int nReqSize)
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
			if(UART_RxByte(&nTmp, SEC(2)))
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
					while (UART_RxByte(&nTmp, MSEC(10))){}
					UART_TxByte(ACK);
					return nRxByte; /* normal end */
				case CAN:
					if (UART_RxByte(&nTmp, SEC(1)) > 0)
					{
						if(nTmp == CAN)
						{
							while(UART_RxByte(&nTmp, MSEC(10))){}
							UART_TxByte(ACK);
							return -1; /* canceled by remote */
						}
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
		while (UART_RxByte(&nTmp, MSEC(10)) >= 0){}
		UART_TxByte(CAN);
		UART_TxByte(CAN);
		UART_TxByte(CAN);
		return -2; /* sync error */

	RECV:
		if (trychar == 'C')
		{
			bCRC = 1;
		}
		trychar = 0;
		pRxPos = aRxBuf;
		*pRxPos++ = nTmp;
		for (int i = 0; i < (nDataSize + bCRC + 3); i++)
		{
			if (0 == (UART_RxByte(&nTmp, SEC(2))))
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
					fTreat(pCtx, nRxByte, &aRxBuf[3], nSizeData);
					nRxByte += nSizeData;
				}
				nRxPktNo++; // Success.
				nRestChance = MAXRETRANS + 1;
			}
			if (--nRestChance <= 0)
			{
				while (UART_RxByte(&nTmp, MSEC(10))){}
				UART_TxByte(CAN);
				UART_TxByte(CAN);
				UART_TxByte(CAN);
				return -3; /* too many retry error */
			}
			UART_TxByte(ACK);
			continue;
		}
	REJECT:
		while (UART_RxByte(&nTmp, MSEC(10))){}
		UART_TxByte(NAK);
	}
}

int XM_Transmit(TreatFunc fTreat, void* pCtx, int nReqSize)
{
	uint8_t anTxBuf[1030]; /* 1024 for XModem 1k + 3 head chars + 2 crc + nul */
	int nDataSize;
	int bCRC = 0;
	uint8_t nPktNo = 1;
	int nSent = 0;
	char nTmp;

	while(1)
	{
		for (int nTry = 0; nTry < 16; nTry++)
		{
			if (UART_RxByte(&nTmp, SEC(2)))
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
					if(UART_RxByte(&nTmp, SEC(1)))
					{
						if(CAN == nTmp)
						{
							UART_TxByte(ACK);
							while (UART_RxByte(&nTmp, MSEC(10))){}
							return -1; /* canceled by remote */
						}
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
		while (UART_RxByte(&nTmp, MSEC(10))){}
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
				fTreat(pCtx, nSent, &anTxBuf[3], nTxByte);
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
					if(UART_RxByte(&nTmp, SEC(1)))
					{
						switch (nTmp)
						{
						case ACK:
							++nPktNo;
							nSent += nDataSize;
							goto TRANS;
						case CAN:
							if(UART_RxByte(&nTmp,SEC(1)))
							{
								if (CAN == nTmp)
								{
									UART_TxByte(ACK);
									while (UART_RxByte(&nTmp, MSEC(10)) >= 0){}
									return -1; /* canceled by remote */
								}
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
				while (UART_RxByte(&nTmp, MSEC(10)) >= 0){}
				return -4; /* xmit error */
			}
			else
			{
				for (int nTry = 0; nTry < 10; nTry++)
				{
					UART_TxByte(EOT);
					if(UART_RxByte(&nTmp,SEC(2)))
					{
						if (ACK == nTmp) break;
					}
				}
				char nDummy;
				while (UART_RxByte(&nDummy, MSEC(10))){}
				return (nTmp == ACK) ? nSent : -5;
			}
		}
	}
	return -6;
}

#define BUF_SIZE (3000)

uint8_t aRBuf[BUF_SIZE];
uint8_t aTBuf[BUF_SIZE];


void txData(void* pCtx,uint32_t nOffset,uint8_t* pDstBuf,uint32_t nBytes)
{
	XCtx* pXCtx = (XCtx*)pCtx;
	if(NULL != pXCtx->pFile)
	{
		fread(pDstBuf,1,nBytes,pXCtx->pFile);
	}
	else
	{
		memcpy(pDstBuf,&(aTBuf[nOffset]),nBytes);
	}
}

void rxData(void* pCtx,uint32_t nOffset,uint8_t* pSrcBuf,uint32_t nBytes)
{
	XCtx* pXCtx = (XCtx*)pCtx;
	pXCtx->nBytes += nBytes;
	if(NULL != pXCtx->pFile)
	{
		fwrite(pSrcBuf,1,nBytes,pXCtx->pFile);
	}
	else
	{
		memcpy(&(aRBuf[nOffset]),pSrcBuf,nBytes);
	}
}

void xm_Tx(uint8_t argc,char* argv[])
{
	printf("Start to T -> PC\n");
	int nRet = XM_Transmit(txData,NULL,BUF_SIZE);
	printf("Ret: %d\n",nRet);
}


void xm_Rx(uint8_t argc,char* argv[])
{
	printf("Start to PC -> T\n");
	XCtx stRxCtx;
	stRxCtx.pFile = fopen("/sf/test.bin","w");
	stRxCtx.nBytes = 0;
	int nRet = XM_Receive(rxData,&stRxCtx,BUF_SIZE);
	fclose(stRxCtx.pFile);
	printf("Ret: %d, %ld\n",nRet,stRxCtx.nBytes);
}

void buf_dump(uint8_t* pBuf,uint32_t nSize)
{
	for(uint32_t nIdx = 0; nIdx < nSize; nIdx++)
	{
		if(0 == (nIdx % 32))
			printf("\n %4lX: ",nIdx / 32);
		printf("%2X ",pBuf[nIdx]);
	}
}

void _dump_RBuf(uint8_t argc,char* argv[])
{
	printf("Dump R Buf\n");
	buf_dump(aRBuf,BUF_SIZE);
	printf("\n");
}

void XM_Init()
{
	CLI_Register("tx",xm_Tx);
	CLI_Register("rx",xm_Rx);
	CLI_Register("rdump",_dump_RBuf);
}