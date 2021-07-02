
//**********************************************************************************
//
// Обработка консольных команд
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
#include "timers.h"
#include "epc.h"
#include "epc_spi.h"
#include "epc_def.h"
#include "packet.h"
#include "fram.h"
#include "version.h"
#include "xtime.h"

#include <stm32f1xx_hal_uart.h>

//**********************************************************************************
// Внешние переменные
//**********************************************************************************
extern UART_HandleTypeDef huart1;

//**********************************************************************************
// Локальные константы
//**********************************************************************************
#define BUFFER_DBG          120

static char *const cmd_ok      = "OK\r";
static char *const cmd_err     = "ERROR\r";
static char *const cmd_err_par = "ERROR PARAM\r";
static char *const err_cmd     = "ERROR COMMAND\r";
static char *const save_ok     = "SAVE OK\r";
static char *const save_err    = "SAVE ERROR\r";
static char *const fram_ok     = "FRAM OK\r";
static char *const fram_err    = "FRAM ERROR\r";

//**********************************************************************************
// Локальные переменные
//**********************************************************************************
static bool recv_flg = false;
static char dathex[100];
static char dbg_recv[BUFFER_DBG];
static char dbg_send[BUFFER_DBG];

//**********************************************************************************
// Прототипы локальные функций
//**********************************************************************************
void GetVersion( void );
static int8_t ParseCommand( char *src, char *par );
static uint8_t FramPageOut( uint16_t page );
static void OutIntvlAll( void );
static void OutIntvl( uint16_t interval );

//**********************************************************************************
// Инициализация буферов обмена
//**********************************************************************************
void DbgInit( void ) {

    ClearDbgSend();
    ClearDbgRecv();
    HAL_UART_Receive_IT( &huart1, (uint8_t*)dbg_recv, BUFFER_DBG );
 }
 
//**********************************************************************************
// Чистим приемный буфер
//**********************************************************************************
void ClearDbgRecv( void ) {

    memset( dbg_recv, 0x00, BUFFER_DBG );
    huart1.RxXferCount = BUFFER_DBG;
    huart1.pRxBuffPtr = (uint8_t*)dbg_recv;
 }
 
//**********************************************************************************
// Чистим передающий буфер
//**********************************************************************************
void ClearDbgSend( void ) {

    memset( dbg_send, 0x00, BUFFER_DBG );
 }
 
//**********************************************************************************
// Вывод строки
//**********************************************************************************
void DbgOut( char *str ) {

    //return;
    //while ( huart1.TxXferCount ); //ждем завершения предыдущей передачи
    //задержка окончания вывода предыдущей строки
    SetTimers( TIMER_DEBUG, 100 );
    while ( GetTimers( TIMER_DEBUG ) );
    ClearDbgSend();
    strcpy( dbg_send, str );
    HAL_UART_Transmit_IT( &huart1, (uint8_t*)dbg_send, strlen( dbg_send ) );
 }
 
//**********************************************************************************
// Прием данных от сервера
// Вызов из stm32f1xx_it.c (USART1_IRQHandler)
//**********************************************************************************
void DbgRecv( void ) {

    /*if ( BUFFER_DBG - huart1.RxXferCount >= BUFFER_DBG ) {
        ClearDbgRecv();
        return;
       }*/
    //проверим последний принятый байт, если CR - обработка команды
    if ( dbg_recv[BUFFER_DBG - huart1.RxXferCount - 1] == '\r' ) {
        dbg_recv[BUFFER_DBG - huart1.RxXferCount - 1] = '\0'; //уберем код CR
        recv_flg = true;
       } 
 }
 
//**********************************************************************************
// Обработка данных
//**********************************************************************************
void DbgWork( void ) {

    static char res[40];
    static uint16_t ipage, fram;
    static int8_t cnt_par;
    static uint8_t tarif, result;
    static double counter1, counter2;
    static char param[MAX_CNT_PARAM][MAX_LEN_PARAM];
    
    if ( recv_flg == false )
        return;
    recv_flg = false;

    //**********************************************************************************
    // Разбор команды
    //**********************************************************************************
    cnt_par = ParseCommand( dbg_recv, *param );
    if ( cnt_par < 0 ) {
        DbgOut( cmd_err );
        return;
       } 
    #ifdef DEBUG_PARAM
    //вывод значений параметров после разбора команды
    for ( i = 0; i < cnt_par; i++ ) {
        sprintf( str, "P%d: %s\r", i, param[i] );
        DbgOut( str );
        SetTimers( TIMER_TEST, 100 );
        while ( GetTimers( TIMER_TEST ) );
       } 
    #endif
    ClearDbgRecv();

    //**********************************************************************************
    // Команды нет, вывод дата-время
    //**********************************************************************************
    if ( cnt_par == 0 ) {
        GetDateTime();
        return;
       }

    //**********************************************************************************
    // Принудительный перезапуск "K"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'K' && strlen( param[IDX_COMMAND] ) == 1 && cnt_par == 1 && !GetNumDev() ) {
        NVIC_SystemReset();
        return;
       }

    //**********************************************************************************
    // Текущая дата "D"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'D' && strlen( param[IDX_COMMAND] ) == 1 && cnt_par == 1 ) {
        GetDate();
        return;
       }

    //**********************************************************************************
    // Установка даты "D DD.MM.YY"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'D' && strlen( param[IDX_COMMAND] ) == 1 && cnt_par == 2 && !GetNumDev() ) {
        if ( SetDate( param[IDX_DATE] ) != SUCCESS )
            DbgOut( cmd_err );
        else DbgOut( cmd_ok );
        return;
       }

    //**********************************************************************************
    // Текущее время "T"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'T' && strlen( param[IDX_COMMAND] ) == 1 && cnt_par == 1 ) {
        GetTime();
        return;
       }

    //**********************************************************************************
    // Установка времени "T HH:MM:SS"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'T' && strlen( param[IDX_COMMAND] ) == 1 && cnt_par == 2 && !GetNumDev() ) {
        if ( SetTime( param[IDX_TIME] ) != SUCCESS )
            DbgOut( cmd_err );
        else DbgOut( cmd_ok );
        return;
       }

    //**********************************************************************************
    // Номер устройства "N"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'N' && strlen( param[IDX_COMMAND] ) == 1 && cnt_par == 1 && !GetNumDev() ) {
        sprintf( dbg_send, "%d/%05d\r", GetCountTarif() + 1, GetNumDev() );
        HAL_UART_Transmit_IT( &huart1, (uint8_t*)dbg_send, strlen( dbg_send ) );
        return;
       }

    //**********************************************************************************
    // Идентификатор процессора "I"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'I' && strlen( param[IDX_COMMAND] ) == 1 && cnt_par == 1 && !GetNumDev() ) {
        GetIdCPU();
        return;
       }

    //**********************************************************************************
    // Дата/время прошивки "V"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'V' && strlen( param[IDX_COMMAND] ) == 1 && cnt_par == 1 ) {
        GetVersion();
        return;
       }

    //**********************************************************************************
    // Проверка памяти FRAM "F"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'F' && strlen( param[IDX_COMMAND] ) == 1 ) {
        fram = FRAMCheck();
        if ( fram ) {
            sprintf( res, "FRAM ERROR PAGE: %03d\r", fram - 1 );
            DbgOut( res );
           }
        else DbgOut( fram_ok );
        return;
       }

    //**********************************************************************************
    // Вывод дампа страницы FRAM "E"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'E' && strlen( param[IDX_COMMAND] ) == 1 && !GetNumDev() ) {
        if ( cnt_par != 2 ) {
            DbgOut( cmd_err_par );
            return;
           }
        if ( FramPageOut( atoi( param[IDX_NUM_PAGE] ) ) == ERROR )
            DbgOut( fram_err );
        return;
       }

    //**********************************************************************************
    // Вывод дампа страницы FRAM "EI"
    //**********************************************************************************
    if ( !strcasecmp( param[IDX_COMMAND], "EI" ) && strlen( param[IDX_COMMAND] ) == 2 && !GetNumDev() ) {
        if ( cnt_par != 1 ) {
            DbgOut( cmd_err_par );
            return;
           }
        for ( ipage = FRAM_INTERVAL; ipage < FRAM_INTERVAL_MAX; ipage++ ) {
            if ( FramPageOut( ipage ) == ERROR )
                DbgOut( fram_err );
            SetTimers( TIMER_WAIT_DUMP, 50 );
            while ( GetTimers( TIMER_WAIT_DUMP ) );
           }
        return;
       }

    //**********************************************************************************
    // Очистка FRAM "C"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'C' && strlen( param[IDX_COMMAND] ) == 1 && !GetNumDev() ) {
        if ( FRAMClear() == SUCCESS )
            DbgOut( cmd_ok );
        else DbgOut( cmd_err );
        return;
       } 

    //**********************************************************************************
    // Запрос текущих показаний счетчиков "Q"
    //********************************************************************************** 
    if ( toupper( param[IDX_COMMAND][0] ) == 'Q' && strlen( param[IDX_COMMAND] ) == 1 && cnt_par == 1 ) {
        FRAMGetPwr( TARIFF_DAILY, &counter1 );
        FRAMGetPwr( TARIFF_NIGHT, &counter2 );
        sprintf( dbg_send, "%.2f %.2f\r", counter1, counter2 );
        HAL_UART_Transmit_IT( &huart1, (uint8_t*)dbg_send, strlen( dbg_send ) );
        return;
       }

    //**********************************************************************************
    // Запрос интервального показания счетчика "Q 1...240"
    //********************************************************************************** 
    if ( toupper( param[IDX_COMMAND][0] ) == 'Q' && strlen( param[IDX_COMMAND] ) == 1 && cnt_par == 2 && !GetNumDev() ) {
        ipage = atoi( param[IDX_NUM_PAGE] );
        if ( !ipage || ipage > ( FRAM_INTERVAL_MAX - FRAM_INTERVAL ) ) {
            DbgOut( cmd_err_par );
            return;
           }
        OutIntvl( ipage );
        return;
       }

    //**********************************************************************************
    // Запрос интервальных показаний счетчиков "W"
    //********************************************************************************** 
    if ( toupper( param[IDX_COMMAND][0] ) == 'W' && strlen( param[IDX_COMMAND] ) == 1 && cnt_par == 1 && !GetNumDev() ) {
        OutIntvlAll();
        return;
       }

    //**********************************************************************************
    // Установка значений расхода "S 1/2 val"
    //********************************************************************************** 
    if ( toupper( param[IDX_COMMAND][0] ) == 'S' && strlen( param[IDX_COMMAND] ) == 1 && !GetNumDev() ) {
        if ( cnt_par != 3 ) {
            DbgOut( cmd_err_par );
            return;
           } 
        tarif = atoi( param[IDX_TARIF] );
        counter1 = atof( param[IDX_POWER] );
        if ( counter1 > 999999.99 )
            DbgOut( cmd_err_par );
        if ( FRAMSavePwr( tarif, counter1 ) == SUCCESS )
            DbgOut( save_ok );
        else DbgOut( save_err );
        PwrCountInit();
        return;
       }

    //**********************************************************************************
    // Команды для STPM01 программный сброс STPM01 'Z'
    //********************************************************************************** 
    if ( toupper( param[IDX_COMMAND][0] ) == 'Z' && strlen( param[IDX_COMMAND] ) == 1 && cnt_par == 1 && !GetNumDev() ) {
        EPC_Reset();
        //EPCInit();
        DbgOut( cmd_ok );
        return;
       }

    //**********************************************************************************
    // Команды для STPM01 вывод калибровочных констант "L"
    //********************************************************************************** 
    if ( toupper( param[IDX_COMMAND][0] ) == 'L' && cnt_par == 1 && !GetNumDev() ) {
        EPCViewCalibr();
        return;
       }

    //**********************************************************************************
    // Команды для STPM01 конфигурация и статусы чтение данных STPM01 'X'
    //********************************************************************************** 
    if ( toupper( param[IDX_COMMAND][0] ) == 'X' && strlen( param[IDX_COMMAND] ) == 1 && cnt_par == 1 && !GetNumDev() ) {
        EPCViewConf();
        //DbgOut( cmd_ok );
        return;
       }

    //**********************************************************************************
    // Команды для STPM01 вывод данных по параметрам U/I/P 'B'
    //********************************************************************************** 
    if ( toupper( param[IDX_COMMAND][0] ) == 'B' && strlen( param[IDX_COMMAND] ) == 1 && cnt_par == 1 && !GetNumDev() ) {
        EPCViewParam();
        return;
       }

    //**********************************************************************************
    // Установка константы "кол-во импульсов на 1000 Вт*ч" "IPK"
    //********************************************************************************** 
    if ( !strcasecmp( param[IDX_COMMAND], "IPK" ) && cnt_par == 2 && !GetNumDev() ) {
        result = EPCSetCalibr( CALIBRATION_IPK, atoi( param[IDX_CALIB_CONST] ) );
        if ( result == EPC_ERROR_SAVE || result == EPC_ERROR_READ ) {
            DbgOut( cmd_err );
            return;
           }
        if ( result == EPC_ERROR_VALUE ) {
            DbgOut( cmd_err_par );
            return;
           }
        DbgOut( cmd_ok );
        return;
       }

    //**********************************************************************************
    // Команды для STPM01 Установка параметра: Калибровка канала напряжения "CHV"
    //********************************************************************************** 
    if ( !strcasecmp( param[IDX_COMMAND], "CHV" ) && cnt_par == 2 && !GetNumDev() ) {
        result = EPCSetCalibr( CALIBRATION_CHV, atoi( param[IDX_CALIB_CONST] ) );
        if ( result == EPC_ERROR_SAVE || result == EPC_ERROR_READ ) {
            DbgOut( cmd_err );
            return;
           }
        if ( result == EPC_ERROR_VALUE ) {
            DbgOut( cmd_err_par );
            return;
           }
        DbgOut( cmd_ok );
        return;
       }

    //**********************************************************************************
    // Команды для STPM01 Калибровка первичного токового канала "CHP"
    //********************************************************************************** 
    if ( !strcasecmp( param[IDX_COMMAND], "CHP" ) && cnt_par == 2 && !GetNumDev() ) {
        result = EPCSetCalibr( CALIBRATION_CHP, atoi( param[IDX_CALIB_CONST] ) );
        if ( result == EPC_ERROR_SAVE || result == EPC_ERROR_READ ) {
            DbgOut( cmd_err );
            return;
           }
        if ( result == EPC_ERROR_VALUE ) {
            DbgOut( cmd_err_par );
            return;
           }
        DbgOut( cmd_ok );
        return;
       }

    //Установка параметра: Компенсация фазовой ошибки "CPH"
    /*if ( !strcasecmp( param[IDX_COMMAND], "CPH" ) && cnt_par == 2 && !GetNumDev() ) {
        result = EPC_CalibrSet( CALIBRATION_CPH, atoi( param[IDX_CALIB_CONST] ) );
        if ( result == EPC_ERROR_SAVE || result == EPC_ERROR_READ ) {
            DbgOut( cmd_err );
            return;
           }
        if ( result == EPC_ERROR_VALUE ) {
            DbgOut( cmd_par );
            return;
           }
        DbgOut( cmd_ok );
        return;
       } */
    //Установка параметра: Номинальное напряжение для однопроводного измерителя "NOM"
    /*if ( !strcasecmp( param[IDX_COMMAND], "NOM" ) && cnt_par == 2 && !GetNumDev() ) {
        result = EPC_CalibrSet( CALIBRATION_NOM, atoi( param[IDX_CALIB_CONST] ) );
        if ( result == EPC_ERROR_SAVE || result == EPC_ERROR_READ ) {
            DbgOut( cmd_err );
            return;
           }
        if ( result == EPC_ERROR_VALUE ) {
            DbgOut( cmd_par );
            return;
           }
        DbgOut( cmd_ok );
        return;
       }*/
    DbgOut( err_cmd );
 }

//**********************************************************************************
// Вывод идентификатора процессора
//**********************************************************************************
void GetIdCPU( void ) {
    
    uint8_t i, *pda;
    char temp[10];
    
    ClearDbgSend();
    pda = GetKey() + 11;
    for ( i = 0; i < 12; i++, pda-- ) {
        sprintf( temp, "%02X", *pda );
        strcat( dbg_send, temp );
       } 
    strcat( dbg_send, "\r" );
    HAL_UART_Transmit_IT( &huart1, (uint8_t*)dbg_send, strlen( dbg_send ) );
 }
 
//**********************************************************************************
// Вывод времени
//**********************************************************************************
void GetTime( void ) {

    timedate dt;
    
    GetTimeDate( &dt );
    sprintf( dbg_send, "%02d:%02d:%02d\r", dt.td_hour, dt.td_min, dt.td_sec );
    HAL_UART_Transmit_IT( &huart1, (uint8_t*)dbg_send, strlen( dbg_send ) );
 }

//**********************************************************************************
// Вывод текущей даты
//**********************************************************************************
void GetDate( void ) {

    timedate td;
    
    GetTimeDate( &td );
    sprintf( dbg_send, "%02d.%02d.%04d\r", td.td_day, td.td_month, td.td_year );
    HAL_UART_Transmit_IT( &huart1, (uint8_t*)dbg_send, strlen( dbg_send ) );
 }
 
//**********************************************************************************
// Установка времени
// Входной формат:      HH:MM:SS 
// Предельные значения: 0-23:0-59:0-59
//**********************************************************************************
uint8_t SetTime( char *param ) {

    timedate dt;
    uint8_t idx, chk = 0;
    char mask[] = "NN:NN:NN";

    //проверка формата
    for ( idx = 0; idx < strlen( mask ); idx++ ) {
        if ( mask[idx] == 'N' && isdigit( *(param+idx) ) )
            chk++;
        if ( mask[idx] == ':' && ispunct( *(param+idx) ) )
            chk++;
       } 
    if ( chk != strlen( mask ) )
        return ERROR;
    //получим текущее значение даты, т.к. дата не меняется
    GetTimeDate( &dt );
    dt.td_hour = atoi( param );
    dt.td_min = atoi( param + 3 );
    dt.td_sec = atoi( param + 6 );
    //проверка значений
    if ( dt.td_hour > 23 || dt.td_min > 59 || dt.td_sec > 59 )
        return ERROR;
    if ( SetTimeDate( &dt ) != HAL_OK )
        return ERROR;
    return SUCCESS;
 }

//**********************************************************************************
// Установка времени
// Входной формат DD.MM.YYYY 1-31.1-12.1970-2050
//**********************************************************************************
uint8_t SetDate( char *param ) {
 
    timedate dt;
    int idx, chk = 0;
    char mask[] = "NN.NN.NNNN";

    //проверка формата
    for ( idx = 0; idx < strlen( mask ); idx++ ) {
        if ( mask[idx] == 'N' && isdigit( *(param+idx) ) )
            chk++;
        if ( mask[idx] == '.' && ispunct( *(param+idx) ) )
            chk++;
       } 
    if ( chk != strlen( mask ) )
        return ERROR;
    //получим текущее значение время, т.к. время не меняется
    GetTimeDate( &dt );
    //новые значения даты
    dt.td_day = atoi( param );
    dt.td_month = atoi( param + 3 );
    dt.td_year = atoi( param + 6 );
    //проверка допустимость значений
    if ( dt.td_day < 1 || dt.td_day > 31 || dt.td_month < 1 || dt.td_month > 12 || dt.td_year < 1900 || dt.td_year > 2050 )
        return ERROR;
    if ( SetTimeDate( &dt ) != HAL_OK )
        return ERROR;
    return SUCCESS;
 }

//**********************************************************************************
// Вывод значения "дата-время"
//**********************************************************************************
void GetDateTime( void ) {

    timedate dt;
    char weekday[][7] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
    
    GetTimeDate( &dt );
    sprintf( dbg_send, "RST:0x%02X %02d.%02d.%04d %s %02d:%02d:%02d\r", StatReset(),
            dt.td_day, dt.td_month, dt.td_year, weekday[dt.td_dow], dt.td_hour, dt.td_min, dt.td_sec );
    HAL_UART_Transmit_IT( &huart1, (uint8_t*)dbg_send, strlen( dbg_send ) );
 }
 
//**********************************************************************************
// Вывод значения "дата-время" прошивки
//**********************************************************************************
void GetVersion( void ) {

    sprintf( dbg_send, "%s %s %s\r", version, compiler_date, compiler_time );
    HAL_UART_Transmit_IT( &huart1, (uint8_t*)dbg_send, strlen( dbg_send ) );
 }
 
//**********************************************************************************
// Разбор параметров команды
// char *src    - строка с параметрами
// char *par    - указатель на массив параметров param[][]
// result       - количество параметров, в т.ч. команда
//**********************************************************************************
static int8_t ParseCommand( char *src, char *par ) {

    uint8_t row, i = 0;
    char *str; 
   
    //обнулим предыдущие параметры
    for ( row = 0; row < MAX_CNT_PARAM; row++ )
        memset( par + row * MAX_LEN_PARAM, 0x00, MAX_LEN_PARAM );
    //разбор параметров
    str = strtok( src, " " );
    while ( str != NULL ) {
        strcpy( par + i * MAX_LEN_PARAM, str );
        str = strtok( NULL, " " );
        i++;
        if ( i > MAX_CNT_PARAM )
            return -1;
       }
    return i;
 }

//**********************************************************************************
// Выводит одну страницу дампа памяти FRAM
//**********************************************************************************
static uint8_t FramPageOut( uint16_t page ) {

    char msg[10];
    uint8_t *ptr;
    uint16_t addr;
    
    if ( page >= FRAM_PAGES )
        return ERROR;
    addr = FRAM_PAGE * page;
    ptr = FRAMReadPage( addr );
    if ( ptr == NULL )
        return ERROR;
    sprintf( msg, "0x%04X: ", addr );
    OutHexStr( (uint8_t*)msg, ptr, 16 );
    return SUCCESS;
 }

//**********************************************************************************
// Вывод блока данных в формате HEX
// prefix   - строка заголовок
// data     - данные для вывода
// len      - размерность данных
//**********************************************************************************
void OutHexStr( uint8_t *prefix, uint8_t *data, uint8_t len ) { 

    uint8_t i;
    char temp[10];

    //while ( huart1.TxXferCount ); //ждем завершения предыдущей передачи
    memset( dathex, 0x00, BUFFER_DBG );
    if ( prefix != NULL )
        sprintf( dathex, "%s", (char*)prefix );
    for ( i = 0; i < len; i++ ) {
        if ( i < len-1 )
            sprintf( temp, "%02X ", (char)*(data+i) );
        else sprintf( temp, "%02X\r", (char)*(data+i) );
        strcat( dathex, temp );
       }
    DbgOut( dathex );
 }  

//**********************************************************************************
// Вывод всех интервальных показаний
// Формат вывода: логический номер интервала/фактический номер интервала DD.MM.YYYY HH:MM значение
//**********************************************************************************
static void OutIntvlAll( void ) { 

    double counter;
    uint16_t year, page;
    uint8_t prev = 0, day, month, hour, minute;
    
    for ( page = FRAM_INTERVAL; page < FRAM_INTERVAL_MAX; page++ ) {
        FRAMReadIntrv( page, &day, &month, &year, &hour, &minute, &counter );
        if ( !day || !month || !year )
            prev++;
        else prev = 0;
        if ( prev > 1 )
            break;
        sprintf( dbg_send, "%03d/%03d: %02d.%02d.%04d %02d:%02d %.2f\r", page-FRAM_INTERVAL+1, page, day, month, year, hour, minute, counter );
        HAL_UART_Transmit_IT( &huart1, (uint8_t*)dbg_send, strlen( dbg_send ) );
        SetTimers( TIMER_WAIT_DUMP, 50 );
        while ( GetTimers( TIMER_WAIT_DUMP ) );
       }
 }

//**********************************************************************************
// Вывод конкретного интервального показания
// uint16_t interval - логический номер интервала
//**********************************************************************************
static void OutIntvl( uint16_t interval ) { 

    double counter;
    uint16_t year;
    uint8_t day, month, hour, minute;
    
    FRAMReadIntrv( PageCalc( interval ), &day, &month, &year, &hour, &minute, &counter );
    sprintf( dbg_send, "%03d/%03d: %02d.%02d.%04d %02d:%02d %.2f\r", interval, PageCalc( interval ), day, month, year, hour, minute, counter );
    HAL_UART_Transmit_IT( &huart1, (uint8_t*)dbg_send, strlen( dbg_send ) );
 }
