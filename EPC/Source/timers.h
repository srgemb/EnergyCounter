
#ifndef __TIMERS_H_
#define __TIMERS_H_

#include "stm32f1xx.h"

#define TIMER_READY_HC12        0       //Таймер ожидания ответа радио модуля
#define TIMER_RELAY             1       //Таймер длительности импульса управления реле
#define TIMER_START_RECV        2       //Таймер ожидания приема пакета
#define TIMER_DEBUG             3       //Таймер задержки вывода при отладке
#define TIMER_ACTION            4       //Таймер длительности индикации приема/передачи данных по радиоканалу
#define TIMER_WAIT_SEND         5       //Таймер задержки отправки ответа
#define TIMER_WAIT_EPC          6       //Таймер чтения данных STPM01
#define TIMER_WAIT_DUMP         7       //Таймер задержки вывода дампа интервальных таймеров

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


