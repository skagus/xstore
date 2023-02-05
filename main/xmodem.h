#pragma once
#include <stdint.h>
#define MAX_RETRY		(16)

/**
 * Data를 준비하거나, 수신처리하는 함수.
*/
typedef void (*TreatFunc)(void* pParam, int32_t nIdx, uint8_t* pBuf, uint32_t nBytes);

int XM_Receive(uint8_t* pBuf, int nReqSize);
int XM_Transmit(uint8_t *pReqBuf, int nReqSize);
