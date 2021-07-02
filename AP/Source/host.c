
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

#include "stm32f1xx.h"

#include "hc12ap.h"
#include "host.h"
#include "timers.h"
#include "packet.h"
#include "fram.h"
#include "cmnd.h"
#include "xtime.h"
#include "version.h"

//**********************************************************************************
// Внешние переменные
//**********************************************************************************
extern RTC_HandleTypeDef hrtc;
extern UART_HandleTypeDef huart1;

//**********************************************************************************
// Локальные константы
//**********************************************************************************
#define TIME_PER_SCAN           7000        //период сканирования уст-в (ms)
#define START_PAGE_SCAN         1           //стартовая страница списка уст-в

static char * const cmd_ok  = "OK\r";
static char * const cmd_err = "ERROR\r";
static char * const cmd_par = "ERROR PARAM\r";
static char * const err_cmd = "ERROR COMMAND\r";

//**********************************************************************************
// Локальные переменные
//**********************************************************************************
static bool recv_flg = false, scan_flg = false;
static uint16_t scan_page;

#ifdef DEBUG_VERSION
    static char dathex[100];
#endif

static char host_recv[BUFFER_HOST];
static char host_send[BUFFER_HOST];

//**********************************************************************************
// Локальные функции
//**********************************************************************************
static void ScanDevOff( void );
static void GetVersion( void );
static int8_t ParseCommand( char *src, char *par );
void ClearHostRecv( void );
void ClearHostSend( void );
static uint8_t SetTime( char *param );
static uint8_t SetDate( char *param );
static void GetTime( void );
static void GetDate( void );
static void GetDateTime( void );
static void StatReset( void );

#ifdef DEBUG_VERSION
    static uint8_t FRAMPageOut( uint16_t page );
#endif

//**********************************************************************************
// 
//**********************************************************************************
void HostInit( void ) {

    ClearHostSend();
    ClearHostRecv();
    HAL_UART_Receive_IT( &huart1, (uint8_t*)host_recv, BUFFER_HOST );
 }
 
//**********************************************************************************
// Чистим приемный буфер
//**********************************************************************************
void ClearHostRecv( void ) {

    memset( host_recv, 0x00, BUFFER_HOST );
    huart1.RxXferCount = BUFFER_HOST;
    huart1.pRxBuffPtr = (uint8_t*)host_recv;
 }
 
//**********************************************************************************
// Чистим передающий буфер
//**********************************************************************************
void ClearHostSend( void ) {

    memset( host_send, 0x00, BUFFER_HOST );
 }
 
//**********************************************************************************
// Прием данных от сервера
// вызов из stm32f1xx_it.c (USART1_IRQHandler)
//**********************************************************************************
void HostRecv( void ) {

    //проверим на переполнение приемного буфера
    if ( BUFFER_HOST - huart1.RxXferCount >= BUFFER_HOST ) {
        ClearHostRecv();
        return;
       }
    //проверим последний принятый байт, если CR - обработка команды
    if ( host_recv[BUFFER_HOST - huart1.RxXferCount - 1] == '\r' ) {
        host_recv[BUFFER_HOST - huart1.RxXferCount - 1] = '\0'; //уберем код CR
        recv_flg = ENABLE;
       }
 }
 
//**********************************************************************************
// Вывод строки в консоль
//**********************************************************************************
void HostOut( char *str ) {

    while ( huart1.TxXferCount );  //ждем завершения предыдущей передачи
    ClearHostSend();
    strcpy( host_send, str );
    //задержка окончания вывода предыдущей строки
    SetTimers( TIMER_TEST, 5 );
    while ( GetTimers( TIMER_TEST ) );
    HAL_UART_Transmit_IT( &huart1, (uint8_t*)host_send, strlen( host_send ) );
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
// Обработка данных от сервера
//**********************************************************************************
void HostWork( void ) {

    uint8_t fram;
    char result[40];
    int8_t cnt_par, relay;
    uint32_t power1, power2;
    static char param[MAX_CNT_PARAM][MAX_LEN_PARAM];
    
    if ( recv_flg == false )
        return;
    recv_flg = false;

    //**********************************************************************************
    // Разбор команды
    //**********************************************************************************
    cnt_par = ParseCommand( host_recv, *param );
    if ( cnt_par < 0 ) {
        HostOut( (char*)cmd_err );
        return;
       } 
    #ifdef DEBUG_PARAM
    //вывод в консоль значений параметров после разбора команды
    for ( i = 0; i < cnt_par; i++ ) {
        sprintf( str, "P%d: %s\r", i, param[i] );
        HostOut( str );
        //задержка окончания вывода предыдущей строки
        SetTimers( TIMER_TEST, 100 );
        while ( GetTimers( TIMER_TEST ) );
       } 
    #endif
    ClearHostRecv(); //разбор завершен, почистим приемник
    
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
    if ( toupper( param[IDX_COMMAND][0] ) == 'K' ) {
        NVIC_SystemReset();
        return;
       }

    //**********************************************************************************
    // Текущая дата "D"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'D' && cnt_par == 1 ) {
        GetDate();
        return;
       }

    //**********************************************************************************
    // Установка даты "D DD.MM.YYYY"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'D' && cnt_par == 2 ) {
        if ( SetDate( param[IDX_DATE] ) != SUCCESS )
            HostOut( cmd_err );
        else HostOut( cmd_ok );
        return;
       } 

    //**********************************************************************************
    // Текущее время "T"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'T' && cnt_par == 1 ) {
        GetTime();
        return;
       }

    //**********************************************************************************
    // Установка времени "T HH:MM:SS"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'T' && cnt_par == 2 ) {
        ClearHostRecv();
        if ( SetTime( param[IDX_TIME] ) != SUCCESS )
            HostOut( cmd_err );
        else HostOut( cmd_ok );
        return;
       } 

    //**********************************************************************************
    // Номер устройства "N"
    //**********************************************************************************
    #ifdef DEBUG_VERSION
    if ( toupper( param[IDX_COMMAND][0] ) == 'N' && cnt_par == 1 ) {
        sprintf( host_send, "AP: %05d\r", GetNumDev() );
        HAL_UART_Transmit_IT( &huart1, (uint8_t*)host_send, strlen( host_send ) );
        return;
       }
    #endif

    //**********************************************************************************
    // Идентификатор процессора "I"
    //**********************************************************************************
    #ifdef DEBUG_VERSION
    if ( toupper( param[IDX_COMMAND][0] ) == 'I' ) {
        GetIdCPU();
        return;
       }
    #endif

    //**********************************************************************************
    // Дата/время прошивки "V"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'V' ) {
        GetVersion();
        return;
       }

    //**********************************************************************************
    // Вывод дампа страницы FRAM "E"
    //**********************************************************************************
    #ifdef DEBUG_VERSION
    if ( toupper( param[IDX_COMMAND][0] ) == 'E' ) {
        if ( cnt_par != 2 ) {
            HostOut( (char*)cmd_par );
            return;
           }
        if ( FRAMPageOut( atoi( param[IDX_NUM_PAGE] ) ) == ERROR )
            HostOut( "ERROR READ\r" );
        return;
       } 
    #endif

    //**********************************************************************************
    // Очистка FRAM "C"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'C' ) {
        if ( FRAMClear() == ERROR )
            HostOut( cmd_err );
        else HostOut( cmd_ok );
        return;
       } 

    //**********************************************************************************
    // Исчточник сброса контроллера "R"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'R' ) {
        StatReset();
        return;
       } 

    //**********************************************************************************
    // Добавление устр-ва для периодического сканирования "A NNNNN XXXXXXXXXXXXXXXXXXXXXXXX"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'A' ) {
        if ( cnt_par == 3 ) {
            if ( !atoi( param[IDX_NUM_DEV] ) || atoi( param[IDX_NUM_DEV] ) > MAX_EPC_NUM ) {
                HostOut( cmd_par );
                return;
               }
            if ( strlen( param[IDX_ID_DEV] ) != 24 ) {
                HostOut( cmd_par );
                return;
               }
            //добавляем счетчик в таблицу сканирования
            if ( FRAMAddPage( atoi( param[IDX_NUM_DEV] ), (uint8_t *)param[IDX_ID_DEV] ) == SUCCESS )
                HostOut( cmd_ok );
            else HostOut( "ERROR FRAM\r" );
            return;
           }
        if ( cnt_par == 1 ) {
            //запуск сканирования
            scan_flg = true;
            scan_page = START_PAGE_SCAN;
            SetTimers( TIMER_SCAN, TIME_PER_SCAN ); //стартуем таймер
            HostOut( "START\r" );
            return;
           } 
        HostOut( (char*)cmd_par );
        return;
       }

    //**********************************************************************************
    // Останавливаем сканирование уст-в "O"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'O' ) {
        if ( cnt_par != 1 ) {
            HostOut( (char*)cmd_par );
            return;
           } 
        HostOut( "STOP\r" );
        ScanDevOff();
        return;
       } 

    //**********************************************************************************
    // Запрос данных счетчика "Q NNNNN XXXXXXXXXXXXXXXXXXXXXXXX"
    //********************************************************************************** 
    if ( toupper( param[IDX_COMMAND][0] ) == 'Q' ) {
        if ( cnt_par != 3 ) {
            HostOut( cmd_par );
            return;
           } 
        EPC_QueryData1( atoi( param[IDX_NUM_DEV] ), (uint8_t *)param[IDX_ID_DEV] );
        return;
       } 

    //**********************************************************************************
    // Запрос данных счетчика "W III NNNNN XXXXXXXXXXXXXXXXXXXXXXXX"
    //********************************************************************************** 
    if ( toupper( param[IDX_COMMAND][0] ) == 'W' ) {
        if ( cnt_par != 4 ) {
            HostOut( cmd_par );
            return;
           } 
        EPC_QueryData2( atoi( param[IDX_NUM_DEV] ), atoi( param[IDX_NUM_INTERVAL] ), (uint8_t *)param[IDX_ID_DEV] );
        return;
       } 

    //**********************************************************************************
    // Установка параметров "S NNNNN XXXXXXXXXXXXXXXXXXXXXXXX P1 P2"
    //********************************************************************************** 
    if ( toupper( param[IDX_COMMAND][0] ) == 'S' ) {
        if ( cnt_par != 5 ) {
            HostOut( cmd_par );
            return;
           } 
        power1 = atoi( param[IDX_POWER1] );
        power2 = atoi( param[IDX_POWER2] );
        EPC_SendData( atoi( param[IDX_NUM_DEV] ), (uint8_t *)param[IDX_ID_DEV], power1, power2 );
        return;
       } 

    //**********************************************************************************
    // Управление реле "P NNNNN XXXXXXXXXXXXXXXXXXXXXXXX 0/1"
    //********************************************************************************** 
    if ( toupper( param[IDX_COMMAND][0] ) == 'P' ) {
        relay = atoi( param[IDX_RELAY] );
        if ( cnt_par != 4 ) {
            HostOut( cmd_par );
            return;
           } 
        EPC_Control( atoi( param[IDX_NUM_DEV] ), (uint8_t *)param[IDX_ID_DEV], relay );
        return;
       } 

    //**********************************************************************************
    // Синхронизация часов "U"
    //********************************************************************************** 
    if ( toupper( param[IDX_COMMAND][0] ) == 'U' ) {
        if ( cnt_par != 3 ) {
            HostOut( cmd_par );
            return;
           } 
        EPC_DateTime( atoi( param[IDX_NUM_DEV] ), (uint8_t *)param[IDX_ID_DEV] );
        return;
       } 

    //**********************************************************************************
    // Проверка памяти "F [NNNNN XXXXXXXXXXXXXXXXXXXXXXXX]"
    //**********************************************************************************
    if ( toupper( param[IDX_COMMAND][0] ) == 'F' ) {
        if ( cnt_par == 1 ) {
            //проверка локальной памяти
            fram = FRAMCheck();
            if ( fram ) {
                sprintf( result, "FRAM ERROR PAGE: %03d\r", fram - 1 );
                HostOut( result );
               }
            else HostOut( "FRAM OK\r" );
            return;
           }
        if ( cnt_par == 3 ) {
            //проверка памяти в счетчике
            EPC_FRAMCheck( atoi( param[IDX_NUM_DEV] ), (uint8_t *)param[IDX_ID_DEV] );
            return;
           }
        HostOut( cmd_par );
        return;
       }
    HostOut( err_cmd );
 }

//**********************************************************************************
// Вывод идентификатор процессора
//**********************************************************************************
void GetIdCPU( void ) {
    
    uint8_t i, *pda;
    char temp[10];
    
    ClearHostSend();
    pda = GetKey() + 11;
    strcpy( host_send, "ID: " );
    for ( i = 0; i < 12; i++, pda-- ) {
        sprintf( temp, "%02X", *pda );
        strcat( host_send, temp );
       } 
    strcat( host_send, "\r" );
    HAL_UART_Transmit_IT( &huart1, (uint8_t*)host_send, strlen( host_send ) );
 }
 
//**********************************************************************************
// Вывод времени
//**********************************************************************************
static void GetTime( void ) {

    
    timedate dt;
    
    GetTimeDate( &dt );
    sprintf( host_send, "%02d:%02d:%02d\r", dt.td_hour, dt.td_min, dt.td_sec );
    HAL_UART_Transmit_IT( &huart1, (uint8_t*)host_send, strlen( host_send ) );
 }

//**********************************************************************************
// Установка времени
// Входной формат:      HH:MM:SS 
// Предельные значения: 0-23:0-59:0-59
//**********************************************************************************
static uint8_t SetTime( char *param ) {

    timedate dt;
    int idx, chk = 0;
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
    //проверка значений
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
// Вывод текущей даты
//**********************************************************************************
static void GetDate( void ) {

    timedate td;
    
    GetTimeDate( &td );
    sprintf( host_send, "%02d.%02d.%04d\r", td.td_day, td.td_month, td.td_year );
    HAL_UART_Transmit_IT( &huart1, (uint8_t*)host_send, strlen( host_send ) );
 }
 
//**********************************************************************************
// Установка времени
// Входной формат DD.MM.YYYY 1-31.1-12.1900-2050
//**********************************************************************************
static uint8_t SetDate( char *param ) {
 
    int idx, chk = 0;
    char mask[] = "NN.NN.NNNN";
    timedate td;

    //проверка формата
    for ( idx = 0; idx < strlen( mask ); idx++ ) {
        if ( mask[idx] == 'N' && isdigit( *(param+idx) ) )
            chk++;
        if ( mask[idx] == '.' && ispunct( *(param+idx) ) )
            chk++;
       } 
    if ( chk != strlen( mask ) )
        return ERROR;
    //текущие значения
    GetTimeDate( &td );
    //проверка значений
    td.td_day = atoi( param );
    td.td_month = atoi( param + 3 );
    td.td_year = atoi( param + 6 );
    //расчет дня недели
    if ( td.td_day < 1 || td.td_day > 31 || td.td_month < 1 || td.td_month > 12 || td.td_year < 1900 || td.td_year > 2050 )
        return ERROR;
    if ( SetTimeDate( &td ) != HAL_OK )
        return ERROR;
    return SUCCESS;
 }

//**********************************************************************************
// Вывод значения "дата-время"
//**********************************************************************************
static void GetDateTime( void ) {

    timedate dt;
    char weekday[][7] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
    
    GetTimeDate( &dt );
    sprintf( host_send, "%02d.%02d.%04d %s %02d:%02d:%02d\r", 
            dt.td_day, dt.td_month, dt.td_year, weekday[dt.td_dow], dt.td_hour, dt.td_min, dt.td_sec );
    HAL_UART_Transmit_IT( &huart1, (uint8_t*)host_send, strlen( host_send ) );
 }
 
//**********************************************************************************
// Вывод значения "дата-время" прошивки
//**********************************************************************************
static void GetVersion( void ) {

    sprintf( host_send, "%s %s %s\r", version, compiler_date, compiler_time );
    HAL_UART_Transmit_IT( &huart1, (uint8_t*)host_send, strlen( host_send ) );
 }
 
//**********************************************************************************
// Выводит одну страницу дампа памяти FRAM
//**********************************************************************************
#ifdef DEBUG_VERSION
    static uint8_t FRAMPageOut( uint16_t page ) {

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
#endif
 
//**********************************************************************************
// Вывод блока данных в формате HEX
// prefix   - строка заголовок
// data     - данные для вывода
// len      - размерность данных
//**********************************************************************************
void OutHexStr( uint8_t *prefix, uint8_t *data, uint8_t len ) { 

    #ifdef DEBUG_VERSION
    uint8_t i;
    char temp[10];

    while ( huart1.TxXferCount ); //ждем завершения предыдущей передачи
    memset( dathex, 0x00, sizeof( dathex ) );
    if ( prefix != NULL )
        sprintf( dathex, "%s", prefix );
    for ( i = 0; i < len; i++ ) {
        if ( i < len-1 )
            sprintf( temp, "%02X ", (char)*(data+i) );
        else sprintf( temp, "%02X\r", (char)*(data+i) );
        strcat( dathex, temp );
       }
    HostOut( dathex );
    #endif
 }  

//**********************************************************************************
// Останавлиывает процесс сканирования
//**********************************************************************************
static void ScanDevOff( void ) {

    scan_page = START_PAGE_SCAN;
    SetTimers( TIMER_SCAN, 0 );
    scan_flg = false;
 }

//**********************************************************************************
// Последовательное сканирование уст-в в сети
//**********************************************************************************
void ScanDev( void ) {

    uint8_t *id_dev;
    uint16_t num_dev;
    char str[60], id_str[30];
    
    if ( scan_flg == false )
        return;
    if ( !GetTimers( TIMER_SCAN ) ) {
        //таймер закончил отсчет одного цикла
        SetTimers( TIMER_SCAN, TIME_PER_SCAN );
        FRAMGetDev( scan_page, &num_dev, &id_dev );
        //проверим наличие уст-в в списке
        if ( scan_page == START_PAGE_SCAN && !num_dev ) {
            //уст-в в списке нет, выключаем сканирование
            scan_flg = false;
            SetTimers( TIMER_SCAN, 0 );
            HostOut( "STOP\r" );
            return;
           }
        if ( !num_dev ) {
            scan_page = START_PAGE_SCAN; //список закончился, начнем сначала
            return;
           }
        sprintf( str, "SCAN: %03d %05d ", scan_page, num_dev );
        Int2Hex( id_str, sizeof( id_str ), id_dev, 12 ); //развернем ключ из двоичного в HEX
        strcat( str, id_str );
        strcat( str, "\r" );
        HostOut( str );
        EPC_QueryData1( num_dev, (uint8_t *)id_str );
        scan_page++;
        if ( scan_page > 512 )
            scan_page = START_PAGE_SCAN;
       } 
 }

//**********************************************************************************
// Устанавливает флаги источника сброса процессора
//**********************************************************************************
static void StatReset( void ) {

    char res[30];
    uint32_t res_src;
    
    res_src = RCC->CSR;
    memset( res, 0x00, 30 );
    //источник сброса счетчика
    strcat( res, "[ " );
    if ( res_src & RCC_CSR_PINRSTF )
        strcat( res, "RST " );
    if ( res_src & RCC_CSR_PORRSTF )
        strcat( res, "POR " );
    if ( res_src & RCC_CSR_SFTRSTF )
        strcat( res, "SFT " );
    if ( res_src & RCC_CSR_IWDGRSTF )
        strcat( res, "WDT " );
    strcat( res, "]\r" );
    HostOut( res );
 }
