
#ifndef __crc16_h
#define __crc16_h

#include "stm32f1xx.h"

void MakeCRC16Table( void );
uint16_t GetCRC16( uint8_t *buf, uint16_t len );

    
#endif

