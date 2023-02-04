#pragma once

/* Define this if you want XMODEM-1K support, it will increase stack usage by 896 bytes */
#define XMODEM_1K		(1)
#define SEC(x) 			((x)*1000)
#define MSEC(x)			(x)
#define MAXRETRANS		(25)

/**
 * Function prototype for storing the received chunks
 * @param funcCtx Pointer to the function context (can be used for anything) 
 * @param xmodemBuffer Pointer to the XMODEM receive buffer (store data from here)
 * @param xmodemSize Number of bytes received in the XMODEM buffer (and to be stored)
*/
typedef void (*StoreChunkType)(void* funcCtx, void* xmodemBuffer, int xmodemSize);

int XmodemReceive(StoreChunkType storeChunk, void *ctx, int destsz, int crc, int mode);

// Function shortcut - XMODEM Receive with checksum
#define XmodemReceiveCsum(storeChunk, ctx, destsz)	XmodemReceive(storeChunk, ctx, destsz, 0, 0)

// Function shortcut - XMODEM Receive with CRC-16
#define XmodemReceiveCrc(storeChunk, ctx, destsz)	XmodemReceive(storeChunk, ctx, destsz, 1, 0)

/**
 * Function prototype for fetching the data chunks
 * @param funcCtx Pointer to the function context (can be used for anything)
 * @param xmodemBuffer Pointer to the XMODEM send buffer (fetch data into here)
 * @param xmodemSize Number of bytes that should be fetched (and stored into the XMODEM send buffer)
*/
typedef void (*FetchChunkType)(void* funcCtx, void* xmodemBuffer, int xmodemSize);

int XmodemTransmit(FetchChunkType fetchChunk, void *ctx, int srcsz, int onek, int mode);

// Function shortcut - XMODEM Transmit with 128 bytes blocks
#define XmodemTransmit128b(fetchChunk, ctx, srcsz) XmodemTransmit(fetchChunk, ctx, srcsz, 0, 0)

// Function shortcut - XMODEM Transmit with 1K blocks
#define XmodemTransmit1K(fetchChunk, ctx, srcsz) XmodemTransmit(fetchChunk, ctx, srcsz, 1, 0)

int UART_TxData(uint8_t* aBuf, uint32_t nSize);
int UART_RxData(uint8_t* aBuf, uint32_t nBufSize);

