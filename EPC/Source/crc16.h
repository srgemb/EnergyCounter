
#ifndef __CRC16_H_
#define __CRC16_H_

#include "stm32f1xx.h"

void MakeCRC16Table( void );
uint16_t GetCRC16( uint8_t *buf, uint16_t len );

    
#endif

