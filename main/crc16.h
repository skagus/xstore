#pragma once
#include <stdint.h>

uint16_t CRC16_ccitt(const uint8_t* aBuf, int nLen);


#define crc16_ccitt	CRC16_ccitt