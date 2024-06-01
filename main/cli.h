#pragma once
#include <stdint.h>

typedef void (CmdHandler)(uint8_t argc, char* argv[]);

void CLI_Init(void);
void CLI_Register(char* szCmd,CmdHandler* pHandle);
void CLI_RegUartEvt(void);
