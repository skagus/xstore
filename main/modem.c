#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/uart.h"
#include "modem.h"

int UART_TxData(uint8_t *aBuf, uint32_t nSize)
{
	uart_tx_chars(CONFIG_ESP_CONSOLE_UART_NUM, (char*)aBuf, nSize);
	return (int)nSize;
}

int UART_RxData(uint8_t *aBuf, uint32_t nBufSize)
{
	return uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, aBuf, nBufSize, 100000);
}

int16_t _inbyte(uint32_t nDelay)
{
	uint8_t nRcv = 0xFF;
	int nCnt = uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, &nRcv, 1, nDelay);
	if (nCnt > 0)
		return nRcv;
	return -1;
}

void _outbyte(int c)
{
	uint8_t nVal = c;
	uart_tx_chars(CONFIG_ESP_CONSOLE_UART_NUM, (char *)&nVal, 1);
}

/***********************************************************************************************************************
 * Character constants
 **********************************************************************************************************************/
#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18
#define CTRLZ 0x1A

#ifdef XMODEM_1K
/* 1024 for XModem 1k + 3 head chars + 2 crc */
#define XBUF_SIZE (1024 + 3 + 2)
#else
/* 128 for XModem + 3 head chars + 2 crc */
#define XBUF_SIZE (128 + 3 + 2)
#endif

#ifndef HAVE_CRC16
/**
 * Calculate the CCITT-CRC-16 value of a given buffer
 * @param buffer Pointer to the byte buffer
 * @param length length of the byte buffer
 */

static unsigned short crc16_ccitt(const unsigned char *buffer, int length)
{
	unsigned short crc16 = 0;
	while (length != 0)
	{
		crc16 = (unsigned char)(crc16 >> 8) | (crc16 << 8);
		crc16 ^= *buffer;
		crc16 ^= (unsigned char)(crc16 & 0xff) >> 4;
		crc16 ^= (crc16 << 8) << 4;
		crc16 ^= ((crc16 & 0xff) << 4) << 1;
		buffer++;
		length--;
	}

	return crc16;
}
#endif

/***********************************************************************************************************************
 * Check block
 **********************************************************************************************************************/
static int check(int crc, const unsigned char *buf, int sz)
{
	if (crc)
	{
		unsigned short crc = crc16_ccitt(buf, sz);
		unsigned short tcrc = (buf[sz] << 8) + buf[sz + 1];
		if (crc == tcrc)
			return 1;
	}
	else
	{
		int i;
		unsigned char cks = 0;
		for (i = 0; i < sz; ++i)
		{
			cks += buf[i];
		}
		if (cks == buf[sz])
			return 1;
	}

	return 0;
}

/***********************************************************************************************************************
 * Flush input
 **********************************************************************************************************************/
static void flushinput(void)
{
	while (_inbyte(SEC(1)) >= 0)
		;
}

/**
 * XMODEM Receive
 * @param storeChunk Function pointer for storing the received chunks or NULL
 * @param ctx If storeChunk is NULL, pointer to the buffer to store the received
 * data, else function context pointer to pass to storeChunk()
 * @param destsz Number of bytes to receive
 * @param crc Checksum mode to request: 0 - arithmetic, 1 - CRC16, 2 - YMODEM-G
 * (CRC16 and no ACK)
 * @param mode Receive mode: 0 - normal, nonzero - receive YMODEM control packet
 */
int XmodemReceive(StoreChunkType storeChunk, void *ctx, int destsz, int crc,
				  int mode)
{
	uint8_t xbuff[XBUF_SIZE];
	uint8_t *p;
	int nBufSize;
	uint8_t trychar = (crc == 2) ? 'G' : (crc ? 'C' : NAK);
	uint8_t packetno = mode ? 0 : 1;
	int i, c, len = 0;
	int nTry, retrans = MAXRETRANS;

	while (1)
	{
		for (nTry = 0; nTry < 16; ++nTry)
		{
			if (trychar)
				_outbyte(trychar);
			if ((c = _inbyte(SEC(2))) >= 0)
			{
				switch (c)
				{
				case SOH:
					nBufSize = 128;
					goto start_recv;
#if (XMODEM_1K == 1)
				case STX:
					nBufSize = 1024;
					goto start_recv;
#endif
				case EOT:
					_outbyte(ACK);
					return len; /* normal end */
				case CAN:
					if ((c = _inbyte(SEC(1))) == CAN)
					{
						flushinput();
						_outbyte(ACK);
						return -1; /* canceled by remote */
					}
					break;
				default:
					break;
				}
			}
		}
		if (trychar == 'G')
		{
			trychar = 'C';
			crc = 1;
			continue;
		}
		else if (trychar == 'C')
		{
			trychar = NAK;
			crc = 0;
			continue;
		}
		flushinput();
		_outbyte(CAN);
		_outbyte(CAN);
		_outbyte(CAN);
		return -2; /* sync error */

	start_recv:
		trychar = 0;
		p = xbuff;
		*p++ = c;
		for (i = 0; i < (nBufSize + (crc ? 1 : 0) + 3); ++i)
		{
			if ((c = _inbyte(SEC(1))) < 0)
				goto reject;
			*p++ = c;
		}

		if (xbuff[1] == (unsigned char)(~xbuff[2]) &&
			(xbuff[1] == packetno || xbuff[1] == (unsigned char)packetno - 1) &&
			check(crc, &xbuff[3], nBufSize))
		{
			if (xbuff[1] == packetno)
			{
				int count = destsz - len;
				if (count > nBufSize)
					count = nBufSize;
				if (count > 0)
				{
					if (storeChunk)
					{
						storeChunk(ctx, &xbuff[3], count);
					}
					else
					{
						memcpy(&((unsigned char *)ctx)[len], &xbuff[3], count);
					}
					len += count;
				}
				++packetno;
				retrans = MAXRETRANS + 1;
			}
			if (--retrans <= 0)
			{
				flushinput();
				_outbyte(CAN);
				_outbyte(CAN);
				_outbyte(CAN);
				return -3; /* too many nTry error */
			}
			if (crc != 2)
				_outbyte(ACK);
			if (mode)
				return len; /* YMODEM control block received */
			continue;
		}
	reject:
		flushinput();
		_outbyte(NAK);
	}
	return 0;
}

/**
 * XMODEM Transmit
 * @return size of sent data(>0) for success, else error code (<0).
 * @param fetchChunk Function pointer for fetching the data chunks to be sent
 * @param ctx If fetchChunk is NULL, pointer to the buffer to be sent,
 * else function context pointer to pass to fetchChunk()
 * @param nReqSize Number of bytes to send
 * @param b1K If nonzero 1024 byte blocks are used (XMODEM-1K)
 * @param mode TX mode:[0:normal] [nonzero:transmit YMODEM control packet]
 */

int XmodemTransmit(FetchChunkType fFetch, void *pBuffOrCtx, int nReqSize,
				   int b1K, int eTxMode)
{
	uint8_t xbuff[XBUF_SIZE];
	int bufsz, nParityMode = -1;
	uint8_t packetno = eTxMode ? 0 : 1;
	int i, nTmp, nSentBytes = 0;
	int nTry;

	while (1)
	{
		for (nTry = 0; nTry < 16; ++nTry)
		{
			if ((nTmp = _inbyte(SEC(2))) >= 0)
			{
				switch (nTmp)
				{
				case 'G':
					nParityMode = 2;
					goto start_trans;
				case 'C':
					nParityMode = 1;
					goto start_trans;
				case NAK:
					nParityMode = 0;
					goto start_trans;
				case CAN:
					if ((nTmp = _inbyte(SEC(1))) == CAN)
					{
						_outbyte(ACK);
						flushinput();
						return -1; /* canceled by remote */
					}
					break;
				default:
					break;
				}
			}
		}
		_outbyte(CAN);
		_outbyte(CAN);
		_outbyte(CAN);
		flushinput();
		return -2; /* no sync */

		while (1)
		{
		start_trans:
			nTmp = nReqSize - nSentBytes;
#ifdef XMODEM_1K
			if (b1K && (nTmp > 128))
			{
				xbuff[0] = STX;
				bufsz = 1024;
			}
			else
#endif
			{
				xbuff[0] = SOH;
				bufsz = 128;
			}
			xbuff[1] = packetno;
			xbuff[2] = ~packetno;
			if (nTmp > bufsz)
				nTmp = bufsz;
			if (nTmp > 0)
			{
				memset(&xbuff[3], eTxMode ? 0 : CTRLZ, bufsz);
				if (fFetch)
				{
					fFetch(pBuffOrCtx, &xbuff[3], nTmp);
				}
				else
				{
					memcpy(&xbuff[3],
						   &((uint8_t *)pBuffOrCtx)[nSentBytes], nTmp);
				}
				if (nParityMode)
				{
					uint16_t ccrc = crc16_ccitt(&xbuff[3], bufsz);
					xbuff[bufsz + 3] = (ccrc >> 8) & 0xFF;
					xbuff[bufsz + 4] = ccrc & 0xFF;
				}
				else
				{
					uint8_t ccks = 0;
					for (i = 3; i < bufsz + 3; ++i)
					{
						ccks += xbuff[i];
					}
					xbuff[bufsz + 3] = ccks;
				}
				for (nTry = 0; nTry < MAXRETRANS; ++nTry)
				{
					for (i = 0; i < bufsz + 4 + (nParityMode ? 1 : 0); ++i)
					{
						_outbyte(xbuff[i]);
					}
					nTmp = (nParityMode == 2) ? ACK : _inbyte(SEC(1));
					if (nTmp >= 0)
					{
						switch (nTmp)
						{
						case ACK:
							++packetno;
							nSentBytes += bufsz;
							goto start_trans;
						case CAN:
							if ((nTmp = _inbyte(SEC(1))) == CAN)
							{
								_outbyte(ACK);
								flushinput();
								return -1; /* canceled by remote */
							}
							break;
						case NAK:
						default:
							break;
						}
					}
				}
				_outbyte(CAN);
				_outbyte(CAN);
				_outbyte(CAN);
				flushinput();
				return -4; /* xmit error */
			}
			else if (eTxMode)
			{
				return nSentBytes; /* YMODEM control block sent */
			}
			else
			{
				for (nTry = 0; nTry < 10; ++nTry)
				{
					_outbyte(EOT);
					if ((nTmp = _inbyte(SEC(2))) == ACK)
						break;
				}
				if (nTmp == ACK)
				{
					return nSentBytes; /* Normal exit */
				}
				else
				{
					flushinput();
					return -5; /* No ACK after EOT */
				}
			}
		}
	}
	return 0;
}



