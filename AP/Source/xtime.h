
#ifndef __XTIME_H
#define __XTIME_H

#include "stm32f1xx.h"

//Структура для хранения время/дата
typedef struct {
	uint8_t	    td_sec;         //секунды
	uint8_t     td_min;         //минуты
    uint8_t     td_hour;        //часы
	uint8_t     td_day;         //день
	uint8_t     td_month;       //месяц
	uint16_t    td_year;        //год
    uint8_t     td_dow;         //день недели
} timedate;
 
 
void GetTimeDate( timedate *ptr );
uint8_t SetTimeDate( timedate *ptr );

#endif
