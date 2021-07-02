
//**********************************************************************************
//
// Управление часами/календарем
// 
//**********************************************************************************

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"

#include "xtime.h"

#include <stm32f1xx_hal_rtc.h>

//**********************************************************************************
// Внешние переменные
//**********************************************************************************
extern RTC_HandleTypeDef hrtc;

//**********************************************************************************
// Локальные константы
//**********************************************************************************
#define TBIAS_DAYS              ( ( 70 * (uint32_t)365 ) + 17 )         //смещение
#define TBIAS_SECS              ( TBIAS_DAYS * (uint32_t)86400 )
#define	TBIAS_YEAR		        1900                                    //начальный год

#define MONTAB( year )		    ((((year) & 03) || ((year) == 0)) ? mos : lmos)
#define	DaysTo32( year, mon )   (((year - 1) / 4) + MONTAB(year)[mon])
 
//кол-во дней в году по месяцам с накоплением (обычный год)
const uint16_t lmos[] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
//кол-во дней в году по месяцам с накоплением (високосный год)
const uint16_t mos[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
 
//**********************************************************************************
// Локальные функции
//**********************************************************************************
static void SecToDtime( uint32_t secsarg, timedate *ptr );
static uint32_t DtimeToSec( timedate *ptr );
static uint8_t DayOfWeek( uint8_t day, uint8_t month, uint16_t year );

static HAL_StatusTypeDef RTC_EnterInitMode( RTC_HandleTypeDef *hrtc ); 
static HAL_StatusTypeDef RTC_ExitInitMode( RTC_HandleTypeDef *hrtc );

//**********************************************************************************
// Возвращает текущее значение время/дата 
// struct timedate *ptr - структура содежащая текущее значение время-дата
//**********************************************************************************
void GetTimeDate( timedate *ptr ) {

    uint32_t high, low, secsarg; 
    
    high = READ_REG( hrtc.Instance->CNTH & RTC_CNTH_RTC_CNT );
    low = READ_REG( hrtc.Instance->CNTL & RTC_CNTL_RTC_CNT );
    secsarg = ( ( high << 16 ) | low );
    SecToDtime( secsarg, ptr );
 }

//**********************************************************************************
// Устанавливает новое значение время/дата 
// struct timedate *ptr - структура содежащая значение для установки время-дата
//**********************************************************************************
uint8_t SetTimeDate( timedate *ptr ) {

    uint32_t timecounter;
    HAL_StatusTypeDef status = HAL_OK;

    timecounter = DtimeToSec( ptr );
    //Set Initialization mode
    if ( RTC_EnterInitMode( &hrtc ) != HAL_OK ) 
        status = HAL_ERROR;
    else {
        //Set RTC COUNTER MSB word
        WRITE_REG( hrtc.Instance->CNTH, ( timecounter >> 16 ) );
        //Set RTC COUNTER LSB word
        WRITE_REG( hrtc.Instance->CNTL, ( timecounter & RTC_CNTL_RTC_CNT ) );
        //Wait for synchro 
        if ( RTC_ExitInitMode( &hrtc ) != HAL_OK )
            status = HAL_ERROR;
       }
    return status;
 }

//**********************************************************************************
// Преобразует кол-во секунд прошедших от 1900 года в значение время/дата.
// struct tm *ptr   -
// uint32_t secsarg -
//**********************************************************************************
static void SecToDtime( uint32_t secsarg, timedate *ptr ) {

    uint32_t i, secs, days, mon, year;
    const uint16_t *pm;
 
    secs = (uint32_t)secsarg + TBIAS_SECS;
    days = 0;
    //расчет дней, часов, минут
	days += secs/86400;
    secs = secs % 86400;
	ptr->td_hour = secs / 3600;
    secs %= 3600;
	ptr->td_min = secs / 60;
    ptr->td_sec = secs % 60;
    //расчет года
	for ( year = days / 365; days < ( i = DaysTo32( year, 0 ) + 365*year ); ) 
        --year;
	days -= i;
	ptr->td_year = year + TBIAS_YEAR;
    //расчет месяца
	pm = MONTAB( year );
	for ( mon = 12; days < pm[--mon]; );
	ptr->td_month = mon + 1;
	ptr->td_day = days - pm[mon] + 1;
    //расчет дня недели
    ptr->td_dow = DayOfWeek( ptr->td_day, ptr->td_month, ptr->td_year );
 }

//**********************************************************************************
// Преобразует значение время/дата в кол-во секунд прошедших от 1900 года.
// struct timedate *ptr - структура содежащая текущее значение время-дата
// return               - значение секунд
//**********************************************************************************
static uint32_t DtimeToSec( timedate *ptr ) {	

    uint32_t days, secs, mon, year;
 
	//перевод даты в кол-во дней от 1900 года
	mon = ptr->td_month - 1;
	year = ptr->td_year - TBIAS_YEAR;
	days  = DaysTo32( year, mon ) - 1;
	days += 365 * year;
	days += ptr->td_day;
	days -= TBIAS_DAYS;
	//перевод текущего времени в секунды и дней в секунды
	secs  = 3600 * ptr->td_hour;
	secs += 60 * ptr->td_min;
	secs += ptr->td_sec;
	secs += (days * (uint32_t)86400);
	return secs;
 }
 
//*********************************************************************************************
// Расчет дня недели по дате
// Все деления целочисленные (остаток отбрасывается).
// Результат: 0 — воскресенье, 1 — понедельник и т.д.
//*********************************************************************************************
static uint8_t DayOfWeek( uint8_t day, uint8_t month, uint16_t year ) {
 
    uint16_t a, y, m;
    
    a = (14 - month) / 12;
    y = year - a;
    m = month + 12 * a - 2;
    return (7000 + (day + y + y / 4 - y / 100 + y / 400 + (31 * m) / 12)) % 7;
 }
 
//**********************************************************************************
// Включение режима инициализации RTC
// @file    stm32f1xx_hal_rtc.c
// @author  MCD Application Team
// @version V1.0.4
// @date    29-April-2016
// @brief   RTC HAL module driver.
//**********************************************************************************
static HAL_StatusTypeDef RTC_EnterInitMode( RTC_HandleTypeDef *hrtc ) {

    uint32_t tickstart = 0;

    tickstart = HAL_GetTick();
    //Wait till RTC is in INIT state and if Time out is reached exit
    while ( ( hrtc->Instance->CRL & RTC_CRL_RTOFF ) == (uint32_t)RESET ) {
        if ( ( HAL_GetTick() - tickstart ) > RTC_TIMEOUT_VALUE )
            return HAL_TIMEOUT;
       }
    //Disable the write protection for RTC registers
    __HAL_RTC_WRITEPROTECTION_DISABLE(hrtc);
    return HAL_OK;  
 }

//**********************************************************************************
// Завершение режима инициализации RTC
// @file    stm32f1xx_hal_rtc.c
// @author  MCD Application Team
// @version V1.0.4
// @date    29-April-2016
// @brief   RTC HAL module driver.
//**********************************************************************************
static HAL_StatusTypeDef RTC_ExitInitMode( RTC_HandleTypeDef *hrtc ) {
    
    uint32_t tickstart = 0;

    //Disable the write protection for RTC registers
    __HAL_RTC_WRITEPROTECTION_ENABLE(hrtc);
    tickstart = HAL_GetTick();
    //Wait till RTC is in INIT state and if Time out is reached exit
    while ( ( hrtc->Instance->CRL & RTC_CRL_RTOFF ) == (uint32_t)RESET ) {
        if ( (HAL_GetTick() - tickstart ) >  RTC_TIMEOUT_VALUE )
            return HAL_TIMEOUT;
       }
    return HAL_OK;  
 }

