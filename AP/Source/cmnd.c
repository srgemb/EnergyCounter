
//**********************************************************************************
//
// Обработка команд полученных по радиоканалу
// 
//**********************************************************************************

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "stm32f1xx.h"

#include "hc12ap.h"
#include "host.h"
#include "timers.h"
#include "packet.h"
#include "fram.h"
#include "version.h"
#include "cmnd.h"


//**********************************************************************************
// Локальные консанты
//**********************************************************************************
#define MAX_EPC_POWER       999999999L              //максимальное значение счетчика
#define INTERVAL_MAX        2*24*5                  //максимальная страница для хранения 
                                                    //интервальных показаний

static char * const send_ok  = "SEND OK\r";
static char * const err_num  = "ERROR EPC_NUM\r";
static char * const err_id   = "ERROR EPC_ID\r";
static char * const err_pack = "ERROR PACKAGE\r";
static char * const err_pwr  = "ERROR EPC_PWR\r";
static char * const err_rel  = "ERROR EPC_RELAY\r";
static char * const err_apn  = "ERROR AP_NUM\r";
static char * const err_aid  = "ERROR AP_ID\r";
static char * const err_wait = "WAIT\r";

//ключ для широковещательной установки даты/времени
static char * const dtime_key = { "8721F4178FB39CA10B70FF50" };

//**********************************************************************************
// Формирование пакета запроса
// "Q NNNNN XXXXXXXXXXXXXXXXXXXXXXXX"
//**********************************************************************************
void EPC_QueryData1( uint16_t epc_num, uint8_t *epc_id ) {
    
    uint8_t *pdata, send;
    
    //проверка переданных значений
    if ( !epc_num || epc_num > MAX_EPC_NUM ) {
        HostOut( (char*)err_num );
        return;
       }
    if ( epc_id == NULL || strlen( (char *)epc_id ) != 24 ) {
        HostOut( (char*)err_id );
        return;
       }
    pdata = CrtPacket( epc_num, epc_id, COMMAND_READ | COMMAND_POWER1 | COMMAND_POWER2, 0, 0, 0 );
    if ( pdata == NULL ) {
        HostOut( (char*)err_pack );
        return;
       }
    send = RFSend( pdata, WAIT_ANSWER );
    if ( send == SUCCESS ) {
        HostOut( (char*)send_ok );
       }
    else HostOut( (char*)err_wait );
 }   

//**********************************************************************************
// Формирование пакета запроса
// "W III NNNNN XXXXXXXXXXXXXXXXXXXXXXXX"
//**********************************************************************************
void EPC_QueryData2( uint16_t epc_num, uint8_t interval, uint8_t *epc_id ) {
    
    uint8_t *pdata, send;
    
    //проверка переданных значений
    if ( !epc_num || epc_num > MAX_EPC_NUM || !interval || interval > INTERVAL_MAX ) {
        HostOut( (char*)err_num );
        return;
       }
    if ( epc_id == NULL || strlen( (char *)epc_id ) != 24 ) {
        HostOut( (char*)err_id );
        return;
       }
    pdata = CrtPacket( epc_num, epc_id, COMMAND_READ | COMMAND_POWER1 | COMMAND_POWER2, 0, 0, interval );
    if ( pdata == NULL ) {
        HostOut( (char*)err_pack );
        return;
       }
    send = RFSend( pdata, WAIT_ANSWER );
    if ( send == SUCCESS ) {
        HostOut( (char*)send_ok );
       }
    else HostOut( (char*)err_wait );
 }   

//**********************************************************************************
// Формирование пакета записи данных
// "S NNNNN XXXXXXXXXXXXXXXXXXXXXXXX P1 P2"
//**********************************************************************************
void EPC_SendData( uint16_t epc_num, uint8_t *epc_id, uint32_t param1, uint32_t param2 ) {

    uint8_t *pdata, send, cmnd;

    //проверка переданных значений
    if ( !epc_num || epc_num > MAX_EPC_NUM ) {
        HostOut( (char*)err_num );
        return;
       }
    if ( epc_id == NULL || strlen( (char *)epc_id ) != 24 ) {
        HostOut( (char*)err_id );
        return;
       }
    if ( param1 > MAX_EPC_POWER || param2 > MAX_EPC_POWER ) {
        HostOut( (char*)err_pwr );
        return;
       }
    cmnd = COMMAND_WRITE;
    if ( !param1 && !param2 )
        cmnd |= COMMAND_DATE_TIME;  //значения тарифов не указаны, пишем дату время
    //запишем значения тарифа 1
    if ( param1 )
        cmnd |= COMMAND_POWER1;     
    //запишем значения тарифа 2
    if ( param2 )
        cmnd |= COMMAND_POWER2;     
    pdata = CrtPacket( epc_num, epc_id, cmnd, param1, param2, 0 );
    if ( pdata == NULL ) {
        HostOut( (char*)err_pack );
        return;
       }
    send = RFSend( pdata, WAIT_ANSWER );
    if ( send == SUCCESS ) {
        HostOut( (char*)send_ok );
       }
    else HostOut( (char*)err_wait );
 }

//**********************************************************************************
// Управление реле
// "P NNNNN XXXXXXXXXXXXXXXXXXXXXXXX 0/1"
//**********************************************************************************
void EPC_Control( uint16_t epc_num, uint8_t *epc_id, uint8_t relay ) {
    
    uint8_t *pdata, send;
    
    //проверка переданных значений
    if ( !epc_num || epc_num > MAX_EPC_NUM ) {
        HostOut( (char*)err_num );
        return;
       }
    if ( epc_id == NULL || strlen( (char *)epc_id ) != 24 ) {
        HostOut( (char*)err_id );
        return;
       }
    if ( relay > 1 ) {
        HostOut( (char*)err_rel );
        return;
       }
    pdata = CrtPacket( epc_num, epc_id, COMMAND_WRITE | COMMAND_RELAY, relay, 0, 0 );
    if ( pdata == NULL ) {
        HostOut( (char*)err_pack );
        return;
       }
    send = RFSend( pdata, WAIT_ANSWER );
    if ( send == SUCCESS ) {
        HostOut( (char*)send_ok );
       }
    else HostOut( (char*)err_wait );
 }   

//**********************************************************************************
// Формирование пакета проверки FRAM
// "F NNNNN XXXXXXXXXXXXXXXXXXXXXXXX"
//**********************************************************************************
void EPC_FRAMCheck( uint16_t epc_num, uint8_t *epc_id ) {
    
    uint8_t *pdata, send;
    
    //проверка переданных значений
    if ( !epc_id || epc_num > MAX_EPC_NUM ) {
        HostOut( (char*)err_num );
        return;
       }
    if ( epc_id == NULL || strlen( (char *)epc_id ) != 24 ) {
        HostOut( (char*)err_id );
        return;
       }
    pdata = CrtPacket( epc_num, epc_id, COMMAND_READ | COMMAND_FRAM_CHK, 0, 0, 0 );
    if ( pdata == NULL ) {
        HostOut( (char*)err_pack );
        return;
       }
    send = RFSend( pdata, WAIT_ANSWER );
    if ( send == SUCCESS ) {
        HostOut( (char*)send_ok );
       }
    else HostOut( (char*)err_wait );
 }   

//**********************************************************************************
// Формирование шировещательного пакета установки даты "U"
//**********************************************************************************
void EPC_DateTime( uint16_t ap_num, uint8_t *ap_id ) {
    
    uint8_t ind ,*pdata, *pda, send;
    
    if ( ap_num != GetNumDev() ) {
        HostOut( (char*)err_apn );
        return;
       }
    if ( ap_id == NULL || strlen( (char *)ap_id ) != 24 ) {
        HostOut( (char*)err_aid );
        return;
       }
    //проверка ID точки доступа
    pda = GetKey() + 11;
    for ( ind = 0; ind < 12; ind++, pda--, ap_id += 2 ) {
        if ( *pda != Hex2Int( ap_id ) ) {
            HostOut( (char*)err_aid );
            return;
           }
       }
    //формирование широковещательного пакета
    pdata = CrtPacket( RECEIVER_ALL, (uint8_t *)dtime_key, COMMAND_WRITE | COMMAND_DATE_TIME, 0, 0, 0 );
    if ( pdata == NULL ) {
        HostOut( (char*)err_pack );
        return;
       }
    send = RFSend( pdata, NO_WAIT_ANSWER );
    if ( send == SUCCESS )
        HostOut( (char*)send_ok );
 }   

//**********************************************************************************
// Вывод результата обработки запроса
//**********************************************************************************
void EPC_Answer( void ) {
    
    uint8_t result;
    char answ[80], res[30];
    
    result = GetDataPack( ID_STATUS );
    //результат выполнения команды на стороне счетчика
    if ( result & STAT_EPC_CMND_OK )
        strcpy( res, "CMD_OK [ " );
    else strcpy( res, "CMD_ERROR [ " );
    //источник сброса счетчика
    if ( result & STAT_EPC_PINRST )
        strcat( res, "RST " );
    if ( result & STAT_EPC_PORRST )
        strcat( res, "POR " );
    if ( result & STAT_EPC_SFTRST )
        strcat( res, "SFT " );
    if ( result & STAT_EPC_WDTRST )
        strcat( res, "WDT " );
    strcat( res, "]" );
    //вывод результата
    sprintf( answ, "%s %02d.%02d.%04d %02d:%02d:%02d %5.1f %09d %09d\r", res,
            GetDataPack( ID_DAY ), GetDataPack( ID_MON ), GetDataPack( ID_YEAR ), 
            GetDataPack( ID_HOUR ), GetDataPack( ID_MINUTE ), GetDataPack( ID_SECONDS ), 
            (float)GetDataPack( ID_VOLTAGE )/10, GetDataPack( ID_PARAM1 ), GetDataPack( ID_PARAM2 ) );
    HostOut( answ );
 }   

