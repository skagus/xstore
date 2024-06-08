
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "macro.h"
#include "uart.h"
#include "cli.h"

static const char* TAG = "CLI";

//////////////////////////////////
#define LEN_LINE            (64)
#define MAX_CMD_COUNT       (16)
#define MAX_ARG_TOKEN       (8)

#define COUNT_LINE_BUF       (8)

/**
 * history.
*/
char gaHistBuf[COUNT_LINE_BUF][LEN_LINE];
uint8_t gnPrvHist;
uint8_t gnHistRef;

typedef struct _CmdInfo
{
	char* szCmd;
	CmdHandler* pHandle;
} CmdInfo;

CmdInfo gaCmds[MAX_CMD_COUNT];
uint8_t gnCmds;

void cli_RunCmd(char* szCmdLine);

void cli_CmdHelp(uint8_t argc,char* argv[])
{
	if(argc > 1)
	{
		int32_t nNum = atoi(argv[1]);
		printf("help with %lX\n", nNum);
		char* aCh = (char*)&nNum;
		printf("help with %02X %02X %02X %02X\n",
			aCh[0], aCh[1], aCh[2], aCh[3]);
	}
	else
	{
		for(uint8_t nIdx = 0; nIdx < gnCmds; nIdx++)
		{
			printf("%d: %s\n",nIdx,gaCmds[nIdx].szCmd);
		}
	}
}

void cli_CmdHistory(uint8_t argc,char* argv[])
{
	if(argc > 1) // Run the indexed command.
	{
		uint8_t nSlot = atoi(argv[1]);
		if(nSlot < COUNT_LINE_BUF)
		{
			if(strlen(gaHistBuf[nSlot]) > 0)
			{
				cli_RunCmd(gaHistBuf[nSlot]);
			}
		}
	}
	else // 
	{
		uint8_t nIdx = gnPrvHist;
		do
		{
			nIdx = (nIdx + 1) % COUNT_LINE_BUF;
			if(strlen(gaHistBuf[nIdx]) > 0)
			{
				printf("%2d> %s\n",nIdx,gaHistBuf[nIdx]);
			}

		} while(nIdx != gnPrvHist);
	}
}

void lb_NewEntry(char* szCmdLine)
{
	if(0 != strcmp(gaHistBuf[gnPrvHist],szCmdLine))
	{
		gnPrvHist = (gnPrvHist + 1) % COUNT_LINE_BUF;
		strcpy(gaHistBuf[gnPrvHist],szCmdLine);
	}
	gnHistRef = (gnPrvHist + 1) % COUNT_LINE_BUF;
}

uint8_t lb_GetNextEntry(bool bInc,char* szCmdLine)
{
	uint8_t nStartRef = gnHistRef;
	szCmdLine[0] = 0;
	int nGab = (true == bInc) ? 1 : -1;
	do
	{
		gnHistRef = (gnHistRef + COUNT_LINE_BUF + nGab) % COUNT_LINE_BUF;
		if(strlen(gaHistBuf[gnHistRef]) > 0)
		{
			strcpy(szCmdLine,gaHistBuf[gnHistRef]);
			return strlen(szCmdLine);
		}
	} while(nStartRef != gnHistRef);
	return 0;
}

uint32_t cli_Token(char* aTok[],char* pCur)
{
	uint32_t nIdx = 0;
	if(0 == *pCur) return 0;
	while(1)
	{
		while(' ' == *pCur) // remove front space.
		{
			pCur++;
		}
		aTok[nIdx] = pCur;
		nIdx++;
		while((' ' != *pCur) && (0 != *pCur)) // remove back space
		{
			pCur++;
		}
		if(0 == *pCur) return nIdx;
		*pCur = 0;
		pCur++;
	}
}

void cli_RunCmd(char* szCmdLine)
{
	char* aTok[MAX_ARG_TOKEN];
	uint8_t nCnt = cli_Token(aTok,szCmdLine);
	bool bExecute = false;
	if(nCnt > 0)
	{
		for(uint8_t nIdx = 0; nIdx < gnCmds; nIdx++)
		{
			if(0 == strcmp(aTok[0],gaCmds[nIdx].szCmd))
			{
				gaCmds[nIdx].pHandle(nCnt,aTok);
				bExecute = true;
				break;
			}
		}
	}

	if(false == bExecute)
	{
		printf("Unknown command: %s\n",szCmdLine);
	}
}

char aLine[LEN_LINE];

void cli_Run(void* pParam)
{
	UNUSED(pParam);
	uint8_t nLen = 0;
	char nCh;

	printf("Start CLI\n");

	while(1)
	{
		while(UART_RxByte(&nCh, 0))
		{
			if(' ' <= nCh && nCh <= '~')
			{
				putchar(nCh);
				aLine[nLen] = nCh;
				nLen++;
			}
			else if(('\n' == nCh) || ('\r' == nCh))
			{
				if(nLen > 0)
				{
					aLine[nLen] = 0;
					printf("\n");
					lb_NewEntry(aLine);
					cli_RunCmd(aLine);
					nLen = 0;
				}
				printf("\n$> ");
			}
			else if(0x08 == nCh) // backspace, DEL
			{
				if(nLen > 0)
				{
					printf("\b \b");
					nLen--;
				}
			}
			else if(0x1B == nCh) // Escape sequence.
			{
				char nCh2,nCh3;
				while(0 == UART_RxByte(&nCh2, SEC(1)));
				if(0x5B == nCh2) // direction.
				{
					while(0 == UART_RxByte(&nCh3, SEC(1)));
					if(0x41 == nCh3) // up.
					{
						nLen = lb_GetNextEntry(false,aLine);
						printf("\r                          \r->");
						if(nLen > 0) printf(aLine);
					}
					else if(0x42 == nCh3) // down.
					{
						nLen = lb_GetNextEntry(true,aLine);
						printf("\r                          \r+>");
						if(nLen > 0) printf(aLine);
					}
				}
			}
			else
			{
				printf("~ %X\n",nCh);
			}
		}
	}
}

/////////////////////

void CLI_Register(char* szCmd,CmdHandler* pHandle)
{
	gaCmds[gnCmds].szCmd = szCmd;
	gaCmds[gnCmds].pHandle = pHandle;
	gnCmds++;
}

void tmp_Run(void* pParam)
{
	int nCnt = 0;
	while(1)
	{
		vTaskDelay(137);
		printf("In Sub: %d\n",nCnt);
		nCnt++;
	}
}

///////////////////////
#define STK_SIZE	(1024 * 2)	// DW.

void CLI_Init(void)
{
	CLI_Register("help",cli_CmdHelp);
	CLI_Register("hist",cli_CmdHistory);
	xTaskCreate(cli_Run,"CLI",STK_SIZE,(void*)1,tskIDLE_PRIORITY,NULL);
}
