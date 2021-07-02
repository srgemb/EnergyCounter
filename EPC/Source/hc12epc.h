
#ifndef __HC12EPC_H_
#define __HC12EPC_H_

#include "stm32f1xx.h"

//**********************************************************************************
// Прототипы функций
//**********************************************************************************
void RFInit( void );
uint8_t RFSend( uint8_t *data );
void RFCheck( void );
void RFRecv( void );
uint8_t RFRecvCnt( void );
void ClearRFRecv( void );
void ClearRFSend( void );
    
#endif
