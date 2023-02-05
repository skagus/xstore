#pragma once
#include <stdint.h>
#define MAX_RETRY		(16)

/**
 * Data를 준비하거나, 수신처리하는 함수.
*/
typedef void (*TreatFunc)(void* pCtx, uint32_t nOffset, uint8_t* pBuf, uint32_t nBytes);

int XM_Receive(TreatFunc fTreat, void* pCtx, int nReqSize);
int XM_Transmit(TreatFunc fTreat, void* pCtx, int nReqSize);
