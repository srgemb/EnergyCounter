
//**********************************************************************************
//
// Обработка команд полученных по радиоканалу
// 
//**********************************************************************************

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

#include "stm32f1xx.h"

#include "hc12epc.h"
#include "debug.h"
#include "packet.h"
#include "timers.h"
#include "fram.h"
#include "cmnd.h"
#include "epc.h"
#include "xtime.h"

//**********************************************************************************
// Локальные константы
//**********************************************************************************
#define RELAY_OFF               0
#define RELAY_ON                1

#define TIME_WAIT_SEND          300     //задержка отправки ответа на запрос
#define TIME_PULSE_RELAY        300     //длительность импулься управления реле (ms)

static char * const ans_ok   = "ANSWER OK\r";
static char * const datt_ok  = "DATE TIME SYNC\r";
static char * const err_pack = "ERROR PACKAGE\r";
static char * const err_pwrw = "ERROR POWER WR\r";
static char * const err_pwrd = "ERROR POWER RD\r";

//**********************************************************************************
// Локальные переменные
//**********************************************************************************
static bool relay = false, cmnd_flg = false, recv_flg = false;

//**********************************************************************************
// Локальные функции
//**********************************************************************************
static void RelayCtrl( uint8_t mode );
static void SetDateTime( void );


//**********************************************************************************
// Запуск ожидания обработки принятого пакета, формирование ответа
//**********************************************************************************
void RecvOK( void ) {

    SetTimers( TIMER_WAIT_SEND, TIME_WAIT_SEND );
    recv_flg = true; //запускаем следущий шаг проверки
 }

//**********************************************************************************
// Запуск обработки принятого пакета
//**********************************************************************************
void Answer( void ) {

    if ( recv_flg == false )
        return;
    if ( !GetTimers( TIMER_WAIT_SEND ) ) {
        recv_flg = false; //
        cmnd_flg = true;
       }
 }

//**********************************************************************************
// Обработка команд принятого пакета
// Вызов из main-while()
//**********************************************************************************
void Command( void ) {

    static uint16_t year, fram;
    static uint8_t cmd, res1, res2, res3, res4, stat, *pdata;
    static double counter1, counter2;
    static uint32_t param1, param2;
    static uint8_t day, month, hour, minute;
    
    if ( cmnd_flg == false )
        return;
    cmnd_flg = false;
    cmd = GetDataPack( ID_COMMAND );
    param1 = GetDataPack( ID_PARAM1 );
    param2 = GetDataPack( ID_PARAM2 );
    
    sprintf( tmp, "CMD: 0x%02X %02d.%02d.%04d %02d:%02d:%02d %09d %09d\r", cmd,
            GetDataPack( ID_DAY ), GetDataPack( ID_MON ), GetDataPack( ID_YEAR ), 
            GetDataPack( ID_HOUR ), GetDataPack( ID_MINUTE ), GetDataPack( ID_SECONDS ), 
            param1, param2 );
    
    SetDateTime();
    if ( cmd & COMMAND_WRITE ) {
        //запись данных
        if ( cmd & COMMAND_POWER1 || cmd & COMMAND_POWER2 ) {
            //запись новых значений счетчиков
            res1 = res2 = res3 = res4 = SUCCESS;
            if ( cmd & COMMAND_POWER1 ) {
                res1 = FRAMSavePwr( 1, counter1 );
                res2 = FRAMGetPwr( 1, &counter1 );
               }
            if ( cmd & COMMAND_POWER2 ) {
                res3 = FRAMSavePwr( 2, counter2 );
                res4 = FRAMGetPwr( 2, &counter2 );
               }
            if ( res1 == ERROR || res2 == ERROR || res3 == ERROR || res4 == ERROR ) {
                stat = 0;
                DbgOut( err_pwrw );
               }
            else stat = STAT_EPC_CMND_OK;
            pdata = CrtPacket1( param1, param2, stat );
            if ( pdata == NULL ) {
                DbgOut( err_pack );
                return;
               }
           }
        if ( cmd & COMMAND_RELAY ) {
            //команда управления реле;
            RelayCtrl( param1 );
            pdata = CrtPacket1( param1, param2, STAT_EPC_CMND_OK );
            if ( pdata == NULL ) {
                DbgOut( err_pack );
                return;
               }
           }
        if ( cmd & COMMAND_DATE_TIME ) {
            //синхронизация часов
            SetDateTime();
            DbgOut( datt_ok );
            return;
           }
       }
    else {
        //чтение данных
        if ( cmd & COMMAND_FRAM_CHK ) {
            //проверка памяти
            fram = FRAMCheck();
            if ( fram ) {
                fram--;
                stat = STAT_EPC_CMND_OK;
               }
            else stat = 0;
            pdata = CrtPacket1( fram, 0, stat );
            if ( pdata == NULL ) {
                DbgOut( err_pack );
                return;
               }
           }
        if ( cmd & COMMAND_FRAM_CHK ) {
            //состояние реле
            /*if ( ACVoltage() )
                res1 = 1;
            else res1 = 0;*/
            pdata = CrtPacket1( res1, 0, STAT_EPC_CMND_OK );
            if ( pdata == NULL ) {
                DbgOut( err_pack );
                return;
               }
           }
        if ( cmd & COMMAND_POWER1 || cmd & COMMAND_POWER2 ) {
            //чтение значений счетчиков
            res1 = res2 = SUCCESS;
            if ( cmd & COMMAND_POWER1 ) {
                res1 = FRAMGetPwr( 1, &counter1 );
                param1 = (uint32_t)counter1;
               }
            if ( cmd & COMMAND_POWER2 ) {
                res2 = FRAMGetPwr( 2, &counter2 );
                param2 = (uint32_t)counter2;
               }
            if ( res1 == ERROR || res2 == ERROR ) {
                stat = 0;
                DbgOut( err_pwrd );
               }
            else stat = STAT_EPC_CMND_OK;
            pdata = CrtPacket1( param1, param2, stat );
            if ( pdata == NULL ) {
                DbgOut( err_pack );
                return;
               }
           }
        if ( GetDataPack( ID_INTERVAL ) ) {
            //чтение интервальных значений счетчиков
            res1 = res2 = SUCCESS;
            res1 = FRAMReadIntrv( PageCalc( GetDataPack( ID_INTERVAL ) ), &day, &month, &year, &hour, &minute, &counter1 );
            param1 = (uint32_t)counter1;
            if ( res1 == ERROR ) {
                stat = 0;
                DbgOut( err_pwrd );
               }
            else stat = STAT_EPC_CMND_OK;
            pdata = CrtPacket2( param1, day, month, year, hour, minute, stat );
            if ( pdata == NULL ) {
                DbgOut( err_pack );
                return;
               }
           }
       }
    RFSend( pdata );
    DbgOut( ans_ok );
 }

//**********************************************************************************
// Обработка команды для реле
//**********************************************************************************
static void RelayCtrl( uint8_t mode ) {

    SetTimers( TIMER_RELAY, TIME_PULSE_RELAY );
    relay = true;
    if ( mode == RELAY_ON )
        HAL_GPIO_WritePin( GPIOB, RELAY1_Pin, GPIO_PIN_SET );
    if ( mode == RELAY_OFF )
        HAL_GPIO_WritePin( GPIOB, RELAY2_Pin, GPIO_PIN_SET );
 }

//**********************************************************************************
// Обработка задержки выключения реле
// Вызов из main-while()
//**********************************************************************************
void Relay( void ) {

    if ( relay == false )
        return;
    if ( GetTimers( TIMER_RELAY ) )
        return; //отсчет таймера не завершен
    relay = false;
    //выключаем сигнал обмоток реле
    HAL_GPIO_WritePin( GPIOB, RELAY1_Pin, GPIO_PIN_RESET );
    HAL_GPIO_WritePin( GPIOB, RELAY2_Pin, GPIO_PIN_RESET );
 }

//**********************************************************************************
// Проверка и синхронизация времени
//**********************************************************************************
static void SetDateTime( void ) {

    timedate td;
    uint16_t dd, mn, yy, hh, mm, ss;
    
    //текущие значения
    GetTimeDate( &td );
    //новые значения
    dd = GetDataPack( ID_DAY ); 
    mn = GetDataPack( ID_MON );
    yy = GetDataPack( ID_YEAR ); 
    hh = GetDataPack( ID_HOUR );
    mm = GetDataPack( ID_MINUTE );
    ss = GetDataPack( ID_SECONDS );
    //проверка расхождений
    if ( dd != td.td_day || mn != td.td_month || yy != td.td_year || hh != td.td_hour || mm != td.td_min || ss != td.td_sec ) {
        td.td_day = dd;
        td.td_month = mn;
        td.td_year = yy;
        td.td_hour = hh;
        td.td_min = mm;
        td.td_sec = ss;
        //установка даты
        SetTimeDate( &td );
       }
 }
