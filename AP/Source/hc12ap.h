
#ifndef __hc12_h
#define __hc12_h

#include "stm32f1xx.h"


#define NO_WAIT_ANSWER      0       //таймер ожидания ответа не запускаем
#define WAIT_ANSWER         1       //Запускаем таймер ожидания ответа

//**********************************************************************************
// Прототипы функций
//**********************************************************************************
void RFInit( void );
void RFEnable( void );
void RFAnswer( void );
void RFRecv( void );
uint8_t RFSend( uint8_t *data, uint8_t wait );
    
#endif
