
#ifndef __timers_h
#define __timers_h

#include "stm32f1xx.h"

#define TIMER_SEND_EPC          0
#define TIMER_RECV_EPC          1
#define TIMER_READY_HC12        2
#define TIMER_TEST              3
#define TIMER_SCAN              4
#define TIMER_ACTION            5

// режим работы индикатора состояния
#define LED_MODE_OFF            0       //выключен
#define LED_MODE_ON             1       //включен
#define LED_MODE_SLOW           2       //короткие импульсы 1:3
#define LED_MODE_FAST           3       //меандр 5 Hz
#define LED_MODE_INV            4       //короткие импульсы 3:1

void TicTimers( void );
void SetTimers( uint8_t timer, uint16_t value );
uint16_t GetTimers( uint8_t timer );
void LedIndicator( void );
void LedMode( uint8_t mode );
void LedAction( void );


#endif


