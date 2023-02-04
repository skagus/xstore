#pragma once

#define MAX_RETRY		(16)

int XM_Receive(uint8_t* pBuf, int nReqSize);
int XM_Transmit(uint8_t *pReqBuf, int nReqSize);
