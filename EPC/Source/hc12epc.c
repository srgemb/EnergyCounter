
//**********************************************************************************
//
// Управление радиомодулем HC12
// 
//**********************************************************************************

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hc12epc.h"
#include "debug.h"
#include "crc.h"
#include "timers.h"
#include "packet.h"
#include "cmnd.h"

//**********************************************************************************
// Внешние переменные
//**********************************************************************************
extern UART_HandleTypeDef huart2;
    
//**********************************************************************************
// Локальные константы
//**********************************************************************************
#define BUFFER_EPC          60      //размер буферов приемо-передачи

#define TIME_MODE_CFG       100     //время перехода радио модуля в режим конфигурирования (ms)
#define TIME_ANSWER_CFG     300     //время ожидания ответа радио модуля (ms)

#define TIME_OUT_RECV       1500    //время ожидания приема всего пакета (ms)

//режим энергопотребления
static char * const RF_PowerMode = "AT+FU3\r\n";
//канал связи
static char * const RF_Chanel = "AT+C010\r\n";
//мощность сигнала
static char * const RF_Power = "AT+P8\r\n";

//**********************************************************************************
// Локальные переменные
//**********************************************************************************
static bool recv_flg = false, init_flg = false, time_out_flg = false;
static char rf_send[BUFFER_EPC];
static char rf_recv[BUFFER_EPC];

//**********************************************************************************
// Прототипы локальные функций
//**********************************************************************************
static uint8_t HC12Init( void );
static void RFEnable( void );


//**********************************************************************************
// Инициализация последовательного порта и радио модуля
//**********************************************************************************
void RFInit( void ) {

    char str[40];
    uint8_t attempt;
    
    //инициализация последовательно порта радиомодуля
    ClearRFRecv();
    ClearRFSend();
    HAL_UART_Receive_IT( &huart2, (uint8_t*)rf_recv, BUFFER_EPC );
    //инициализация радио модуля
    for ( attempt = 0; attempt < 5; attempt++ ) {
        sprintf( str, "INIT HC12 %d ", attempt+1 );
        DbgOut( str );
        if ( HC12Init() == SUCCESS ) {
            RFEnable();
            LedMode( LED_MODE_SLOW );
            DbgOut( "OK\r" );
            break;
           }
        else DbgOut( "ERROR\r" );
       }
    if ( init_flg == false )
        LedMode( LED_MODE_INV );
 }
 
//**********************************************************************************
// Инициализация радио модуля
//**********************************************************************************
static uint8_t HC12Init( void ) {

    //перевод модуля в програмнный режим
    HAL_GPIO_WritePin( RF_SET_GPIO_Port, RF_SET_Pin, GPIO_PIN_RESET );
    //ожидание переходного процесса
    SetTimers( TIMER_READY_HC12, TIME_MODE_CFG );
    while ( GetTimers( TIMER_READY_HC12 ) );

    //шаг 1
    ClearRFRecv();
    HAL_UART_Transmit_IT( &huart2, (uint8_t*)RF_PowerMode, strlen( RF_PowerMode ) );
    SetTimers( TIMER_READY_HC12, TIME_ANSWER_CFG );
    while ( GetTimers( TIMER_READY_HC12 ) );
    //проверка ответа
    if ( strstr( rf_recv, "OK" ) == NULL ) {
        HAL_GPIO_WritePin( RF_SET_GPIO_Port, RF_SET_Pin, GPIO_PIN_SET );
        return ERROR;
       } 
    
    //шаг 2
    ClearRFRecv();
    HAL_UART_Transmit_IT( &huart2, (uint8_t*)RF_Chanel, strlen( RF_Chanel ) );
    SetTimers( TIMER_READY_HC12, TIME_ANSWER_CFG );
    while ( GetTimers( TIMER_READY_HC12 ) );
    //проверка ответа
    if ( strstr( rf_recv, "OK" ) == NULL ) {
        HAL_GPIO_WritePin( RF_SET_GPIO_Port, RF_SET_Pin, GPIO_PIN_SET );
        return ERROR;
       } 

    //шаг 3
    ClearRFRecv();
    HAL_UART_Transmit_IT( &huart2, (uint8_t*)RF_Power, strlen( RF_Power ) );
    SetTimers( TIMER_READY_HC12, TIME_ANSWER_CFG );
    while ( GetTimers( TIMER_READY_HC12 ) );
    //проверка ответа
    if ( strstr( rf_recv, "OK" ) == NULL ) {
        HAL_GPIO_WritePin( RF_SET_GPIO_Port, RF_SET_Pin, GPIO_PIN_SET );
        return ERROR;
       } 

    //переход в режим обмена данными
    HAL_GPIO_WritePin( RF_SET_GPIO_Port, RF_SET_Pin, GPIO_PIN_SET );
    //ожидание переходного процесса
    SetTimers( TIMER_READY_HC12, TIME_MODE_CFG );
    while ( GetTimers( TIMER_READY_HC12 ) );
    return SUCCESS;
 }

//**********************************************************************************
// Чистим приемный буфер
//**********************************************************************************
void ClearRFRecv( void ) {

    //LedMode( LED_MODE_SLOW );
    memset( rf_recv, 0x00, BUFFER_EPC );
    huart2.RxXferCount = BUFFER_EPC;
    huart2.pRxBuffPtr = (uint8_t*)rf_recv;
 }
 
//**********************************************************************************
// Чистим передающий буфер
//**********************************************************************************
void ClearRFSend( void ) {

    memset( rf_send, 0x00, BUFFER_EPC );
 }

//**********************************************************************************
// Разрешаем обмен данными по радиоканалу
//**********************************************************************************
static void RFEnable( void ) {

    ClearRFRecv();
    ClearRFSend();
    init_flg = true;
 }
 
//**********************************************************************************
// Кол-во принятых байт
//**********************************************************************************
uint8_t RFRecvCnt( void ) {

    return huart2.pRxBuffPtr - (uint8_t *)rf_recv;
 }
 
//**********************************************************************************
// Передача пакета
//**********************************************************************************
uint8_t RFSend( uint8_t *data ) {

    if ( init_flg == false )
        return ERROR; //инициализация радио модуля не прошла
    if ( data == NULL )
        return ERROR; //нет данных для передачи
    LedAction();
    ClearRFSend();
    memcpy( rf_send, data, PacketSize( PACKET_EPC_AP) );    
    HAL_UART_Transmit_IT( &huart2, (uint8_t*)rf_send, PacketSize( PACKET_EPC_AP ) );
    return SUCCESS;
 }
 
//**********************************************************************************
// Прием данных по радио каналу
// Вызов из stm32f1xx_it.c (USART2_IRQHandler)
//**********************************************************************************
void RFRecv( void ) {

    LedAction();
    if ( RFRecvCnt() > BUFFER_EPC - 2 ) {
        ClearRFRecv();
        return;
       }
    if ( init_flg == true && RFRecvCnt() ) {
        //LedMode( LED_MODE_FAST );
        //начало приема, заводим таймер time-out
        SetTimers( TIMER_START_RECV, TIME_OUT_RECV );  
        time_out_flg = true;
       }
    if ( init_flg == true && RFRecvCnt() == PacketSize( PACKET_AP_EPC ) ) {
        //пакет принят, на проверку
        recv_flg = true;
        time_out_flg = false;
       } 
 }
 
//**********************************************************************************
// Обработка запроса от точки доступа
//**********************************************************************************
void RFCheck( void ) {

    uint8_t chk_pack;
    
    //проверка таймера и размера принятого пакета
    if ( time_out_flg == true && !GetTimers( TIMER_START_RECV ) ) {
        time_out_flg = false;
        //пакет не пришел, время вышло
        ClearRFRecv();
        DbgOut( "TIME OUT RECV\r" );
       } 
    //проверка флага готовности пакета
    if ( recv_flg == true && RFRecvCnt() == PacketSize( PACKET_AP_EPC ) ) {
        recv_flg = false;
        //проверка пакета
        chk_pack = CheckPacket( (uint8_t*)rf_recv );
        ClearRFRecv();
        if ( chk_pack == CHECK_SUCCESS ) {
            //включаем обработку принятого пакета
            RecvOK();
            DbgOut( "RECV OK\r" );
            return;
           }
        if ( chk_pack == ERROR_HEADTAIL )
            DbgOut( "ERROR_HEADTAIL\r" );
        if ( chk_pack == ERROR_CRC_DATA )
            DbgOut( "ERROR_CRC_DATA\r" );
        if ( chk_pack == ERROR_CRC_PACK )
            DbgOut( "ERROR_CRC_PACK\r" );
        if ( chk_pack == ERROR_SENDER )
            DbgOut( "ERROR_SENDER\r" );
        if ( chk_pack == ERROR_RECIVER )
            DbgOut( "ERROR_RECIVER\r" );
        if ( chk_pack == ERROR_DECRYPT )
            DbgOut( "ERROR_DECRYPT\r" );
       } 
 }
