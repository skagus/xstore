#pragma once
#include <stdint.h>

#define NOT_NUMBER			(0x7FFFFFFF)

typedef void (CmdHandler)(uint8_t argc, char* argv[]);

void CLI_Init(void);
void CLI_Register(char* szCmd,CmdHandler* pHandle);
void CLI_RegUartEvt(void);
uint32_t CLI_GetInt(char* szStr);
