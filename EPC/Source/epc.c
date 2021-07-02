
//**********************************************************************************
//
// Управление счетчиком электроэнергии STPM01
// 
//**********************************************************************************

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

#include "stm32f1xx.h"

#include "packet.h"
#include "fram.h"
#include "debug.h"
#include "timers.h"
#include "epc.h"
#include "epc_spi.h"
#include "epc_def.h"
#include "xtime.h"

//**********************************************************************************
// Внешние переменные
//**********************************************************************************
extern EPC_struct EPCData;

//**********************************************************************************
// Локальные константы
//**********************************************************************************
#define CONST_PULSE_INC     5       //кол-во импульсов для инкремента мощности
#define CONST_KWH           1000    //кол-во Вт на 1 кВт
#define TIME_READ_EPC       500     //период чтения данных из STPM01 (ms)
#define TIME_WAIT_READY     100     //время ожидания завершения сброса при включении (ms)

//**********************************************************************************
// Локальные переменные
//**********************************************************************************
EPC_CALIBRN Calibr;
uint32_t pulse_cnt = 0;             //счетчик импульсов потребленной мощности
double inc_pstep = 0;               //шаг приращения мощности для кол-ва импульсов
                                    //счетчика определенных в CONST_PULSE_INC
double power_save = 0;              //текущее значение потребленной мощности

//**********************************************************************************
// Инициализация (конфигурирование) STPM01
//**********************************************************************************
void EPCInit( void ) {

    //ожидание завершения переходных процессов после включения
    SetTimers( TIMER_WAIT_EPC, TIME_WAIT_READY );
    while ( GetTimers( TIMER_WAIT_EPC ) );
    EPC_ClrListReg();
    
    //Configuration bits originated by shadow latches
    EPC_AddReg( CONFIG_MODE_RD, CONFIG_MODE_RD_SZ,  1 );
    
    //APL=3: standalone, MOP:MON=stepper, LED=pulses according to KMOT
    EPC_AddReg( CONFIG_APL,     CONFIG_APL_SZ,      3 );
    
    //PST=3: primary is shunt x32, secondary is not used
    EPC_AddReg( CONFIG_PST,     CONFIG_PST_SZ,      3 );
    
    //Constant of stepper pulses/kWh selection when APL=2 or APL=3
    EPC_AddReg( CONFIG_KMOT,    CONFIG_KMOT_SZ,     3 );
    
    //Power calculation normal energy accumulation and power computation (p=u*i)
    EPC_AddReg( CONFIG_FRS,     CONFIG_FRS_SZ,      1 );
    
    //прочитаем калибровочные константы из FRAM
    FRAMReadEPC( &Calibr );
    //восстановим калибровочные константы из FRAM в STPM01
    EPC_AddReg( CONFIG_CPH, CONFIG_CPH_SZ, Calibr.cph );
    EPC_AddReg( CONFIG_CHV, CONFIG_CHV_SZ, Calibr.chv );
    EPC_AddReg( CONFIG_CHP, CONFIG_CHP_SZ, Calibr.chp );
    //запись в STPM01
    EPC_SaveReg();
 }

//**********************************************************************************
// Инициализация констант расчета мощности
//**********************************************************************************
void PwrCountInit( void ) {

    char stp[20];
    
    //рассчитаем шаг мощности для CONST_PULSE_INC импульсов
    inc_pstep = (double)( CONST_KWH * CONST_PULSE_INC ) / (double)Calibr.pulse_per_kwh;
    sprintf( stp, "%.3f", inc_pstep );
    //сохраним новое значение
    inc_pstep = atof( stp );
    if ( GetCountTarif() ) {
        //два тарифа, восстановим нужный по времени
        if ( GetTimeInterval() == RATE_DAILY )
            FRAMGetPwr( TARIFF_DAILY, &power_save );
        if ( GetTimeInterval() == RATE_NIGHT )
            FRAMGetPwr( TARIFF_NIGHT, &power_save );
       }
    else {
        //восстановим значение счетчика из FRAM для одного тарифа
        FRAMGetPwr( TARIFF_DAILY, &power_save );
       }
    //определим начальную страницу для хранения интервальных значений
    FRAMFindPage();
 }
 
//**********************************************************************************
// Возвращает текущее среднеквадратичное значение напряжения
//**********************************************************************************
double StpmVRms( void ) {

    double result;
    
    result = ( 1 + STPM_R1 / STPM_R2 ) * (double)EPCData.u_rms * STPM_VREF /
        ( STPM_AU * STPM_KU * STPM_KINT_COMP * STPM_KINT * STPM_KDIF * STPM_LENU * STPM_KUT );
    return result;
 }

//**********************************************************************************
// Возвращает текущее среднеквадратичное значение тока
//**********************************************************************************
double StpmIRms( void ) {

    double result;
    
    result = EPCData.i_rms * STPM_VREF / ( STPM_KS * STPM_AI * STPM_KI * STPM_KINT *
        STPM_KINT_COMP * STPM_KDIF * STPM_LENI );
    return result;
 }

//**********************************************************************************
// Возвращает текущее значение напряжения
//**********************************************************************************
uint16_t ACVoltage( void ) {

    char temp[10];
    uint16_t volt;
    
    sprintf( temp, "%.1f", StpmVRms() );
    volt = (uint16_t)( atof( temp ) * 10 );
    return volt;
 }

//**********************************************************************************
// Расчет значения мощности по кол-ву импульсов от STPM01
// Вызов из HAL_GPIO_EXTI_Callback (main.c) 
//**********************************************************************************
void PulseInc( void ) {

    //отсчет импульсов
    pulse_cnt++;
    if ( !( pulse_cnt % CONST_PULSE_INC ) ) {
        //общее кол-во импульсов кратно CONST_PULSE_INC
        if ( GetCountTarif() ) {
            //запишем новое значение в память для двух тарифов
            if ( GetTimeInterval() == TARIFF_DAILY ) {
                FRAMGetPwr( TARIFF_DAILY, &power_save );
                power_save += inc_pstep;
                FRAMSavePwr( TARIFF_DAILY, power_save );
               }
            if ( GetTimeInterval() == RATE_NIGHT ) {
                FRAMGetPwr( TARIFF_NIGHT, &power_save );
                power_save += inc_pstep;
                FRAMSavePwr( TARIFF_NIGHT, power_save );
               }
           }
        else {
            //запишем новое значение в память для одного тарифа
            power_save += inc_pstep;
            FRAMSavePwr( TARIFF_DAILY, power_save );
           }
       }
 }

//**********************************************************************************
// Запись калибровочных констант в FRAM и STPM01
// uint8_t id    - ID константы
// uint8_t value - значение константы
//**********************************************************************************
uint8_t EPCSetCalibr( uint8_t id, uint16_t value ) {

    //проверка диапазона значений
    if ( id == CALIBRATION_CPH && value > 15 )
        return EPC_ERROR_VALUE;
    if ( id == CALIBRATION_CHV && value > 255 )
        return EPC_ERROR_VALUE;
    if ( id == CALIBRATION_CHP && value > 255 )
        return EPC_ERROR_VALUE;
    if ( id == CALIBRATION_IPK && value > 10000 )
        return EPC_ERROR_VALUE;
    if ( id == CALIBRATION_CPH || id == CALIBRATION_CHV || id == CALIBRATION_CHP )
        EPC_ClrListReg();
    //перед записью прочитаем текущие данные для обновления
    FRAMReadEPC( &Calibr );
    //Компенсация фазовой ошибки "CPH"
    if ( id == CALIBRATION_CPH ) {
        Calibr.cph = value;
        EPC_AddReg( CONFIG_CPH, CONFIG_CPH_SZ, value );
       }
    //Калибровка канала напряжения "CHV"
    if ( id == CALIBRATION_CHV ) {
        Calibr.chv = value;
        EPC_AddReg( CONFIG_CHV, CONFIG_CHV_SZ, value );
       }
    //Калибровка первичного токового канала "CHP"
    if ( id == CALIBRATION_CHP ) {
        Calibr.chp = value;
        EPC_AddReg( CONFIG_CHP, CONFIG_CHP_SZ, value );
       }
    //Кол-во импульсов на 1000 Вт*час
    if ( id == CALIBRATION_IPK )
        Calibr.pulse_per_kwh = value;
    //запись в STPM01
    if ( id == CALIBRATION_CPH || id == CALIBRATION_CHV || id == CALIBRATION_CHP )
        EPC_SaveReg();
    //запись результата в FRAM
    if ( FRAMSaveEPC( &Calibr ) == ERROR )
        return EPC_ERROR_SAVE;
    return EPC_CONST_OK;
 }

//**********************************************************************************
// Периодическое чтение данных из STMP01 интервал чтения: TIME_READ_EPC
// Вызов из main - while()
//**********************************************************************************
void EPCDataRead( void ) {
    
    if ( GetTimers( TIMER_WAIT_EPC ) )
        return; //отсчет не завершен
    //установим следующий интервал
    SetTimers( TIMER_WAIT_EPC, TIME_READ_EPC );
    //чтение данных с проверкой
    EPC_ReadData();
 }

//**********************************************************************************
// По текущему времени определяет тариф: день/ночь
// Временной интервал день: 07:00 - 22:59:59
// Временной интервал ночь: 23:00 - 06:59:59
// result: RATE_DAILY - дневной тариф
//         RATE_NIGHT - ночной тариф
//**********************************************************************************
uint8_t GetTimeInterval( void ) {

    timedate td;

    GetTimeDate( &td );
    if ( td.td_hour >= 7 && td.td_hour <= 22 && td.td_min <= 59 && td.td_sec <= 59 )
        return RATE_DAILY;
    else return RATE_NIGHT;
 }

//**********************************************************************************
// Вывод значений калибровочных констант "L"
// Формат вывода: RRR/PPP
// RRR - значение из регистров STPM01
// PPP - значение из FRAM
//**********************************************************************************
void EPCViewCalibr( void ) {

    uint8_t rslt;
    char str[100];

    rslt = FRAMReadEPC( &Calibr );
    sprintf( str, "%s CHV=%u/%u CHP=%u/%u IPK=%u\r", rslt == SUCCESS ? "RAM-OK" : "RAM-ERR", 
            EPCData.config_chv, Calibr.chv, EPCData.config_chp, Calibr.chp, Calibr.pulse_per_kwh );
    DbgOut( str );
 }

//**********************************************************************************
// Вывод параметров конфигурации и статусов чтения данных 'X'
//**********************************************************************************
void EPCViewConf( void ) {

    char str[100];

    sprintf( str, "%d:%d ST=0x%02X MS=0x%02X APL=%u PST=%u FRS=%d KMOT=%u\r", 
            EPCData.data_parity, EPCData.data_valid, EPCData.status, EPCData.mode_signal,
            EPCData.config_apl, EPCData.config_pst, EPCData.config_frs, EPCData.config_kmot );
    DbgOut( str );
 }

//**********************************************************************************
// Вывод данных U/I/P 'B'
//**********************************************************************************
void EPCViewParam( void ) {

    char str[100];

    sprintf( str, "URMS=%.2f IRMS=%.2f KWH=%.2f PLS=%u SWH=%.3f\r", 
            StpmVRms(), StpmIRms(), power_save, pulse_cnt, inc_pstep );
    DbgOut( str );
 }

//**********************************************************************************
// Сохраняем интервальное значение 
// Вызов из main->HAL_RTCEx_RTCEventCallback
//**********************************************************************************
void EPCSaveInterval( void ) {

    timedate td;

    GetTimeDate( &td );
    //запись интервальных значений каждые 15 секунд
    /*if ( td.td_sec == 0 || td.td_sec == 15 || td.td_sec == 30 || td.td_sec == 45 )
        FRAMSaveIntrv( power_save );*/
    //запись интервальных значений каждые 30 минут
    if ( ( !td.td_min || td.td_min == 30 ) && !td.td_sec )
        FRAMSaveIntrv( power_save );
 }
