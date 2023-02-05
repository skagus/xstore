
#include <stdint.h>
#include <string.h>
#include "uart.h"
#include "console.h"

uint32_t CON_GetLine(char* pBuf, uint32_t nMaxLen)
{
	int16_t nRet;
	int nIdx = 0;
	nMaxLen -= 1;
	printf("\n$>");
	while(1)
	{
		nRet = UART_RxByte(SEC(1));
		if(nRet < 0)
		{
			continue; 
		}
		UART_TxByte((uint8_t)nRet);
		if(nRet == '\n' || nRet == '\r')
		{
			pBuf[nIdx] = 0;
			break;
		}
		pBuf[nIdx] = (uint8_t)nRet;
		nIdx++;
		if(nIdx >= nMaxLen)
		{
			break;
		}
	}
	return nIdx;
}


int CON_Token(char* pInput, char* argv[])
{
	char* token = strtok(pInput, " ");
	int nCnt = 0;
	while (token)
	{
//    	printf("token: %d: %s\n", nCnt, token);
		argv[nCnt++] = token;
    	token = strtok(NULL, " ");
	}
//	printf("Cnt Tok: %d\n", nCnt);
	return nCnt;
}
