#include <stdint.h>

#define NEW_LINE		('\n')
#define MAX_CMD_TOKEN	(32)

#define CMD_COMP(input, cmd)		(0 == strncmp((input), (cmd), strlen(cmd)))

uint32_t CON_GetLine(char* pBuf, uint32_t nMaxLen);
int CON_Token(char* pInput, char* argv[]);
void CON_Run(int argc, char* argv[]);
