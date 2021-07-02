#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hc12ap.h"
#include "host.h"
#include "crc.h"
#include "cmnd.h"
#include "timers.h"
#include "packet.h"

//**********************************************************************************
// Внешние переменные
//**********************************************************************************
extern UART_HandleTypeDef huart2;

//**********************************************************************************
// Локальные константы
//**********************************************************************************
#define BUFFER_EPC          60      //размер буферов приемо-передачи

#define TIME_ANSWER_EPC     5000    //время ожидания ответа счетчика (ms)
#define TIME_ANSWER_POST    100     //задержка разрешения отправки следующего пакета
                                    //следующего пакета (ms)

#define TIME_MODE_CFG       100     //время перехода радио модуля в режим конфигурирования (ms)
#define TIME_ANSWER_CFG     300     //время ожидания ответа радио модуля (ms)
                                    
//режим энергопотребления
static char * const RF_PowerMode = "AT+FU3\r\n";
//канал связи
static char * const RF_Chanel = "AT+C010\r\n";
//мощность сигнала
static char * const RF_Power = "AT+P8\r\n";
//cкорость обмена
static char * const RF_Speed = "AT+B9600\r\n";

//**********************************************************************************
// Локальные переменные
//**********************************************************************************
char rf_send[BUFFER_EPC];
char rf_recv[BUFFER_EPC];
bool recv_flg = false, send_flg = false, init_flg = false;

//**********************************************************************************
// Прототипы локальные функций
//**********************************************************************************
static uint8_t RFRecvCnt( void );
static void ClearRFRecv( void );
static void ClearRFSend( void );
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
    //return SUCCESS;
    //инициализация радио модуля
    for ( attempt = 0; attempt < 5; attempt++ ) {
        sprintf( str, "INIT HC12 %d ", attempt+1 );
        HostOut( str );
        if ( HC12Init() == SUCCESS ) {
            RFEnable();
            HostOut( "OK\r" );
            break;
           }
        else HostOut( "ERROR\r" );
       }
    if ( init_flg == false )
        LedMode( LED_MODE_INV );
 }
 
//**********************************************************************************
// Инициализация последовательного порта радио модуля
//**********************************************************************************
static uint8_t HC12Init( void ) {
 
    //перевод модуля в програмнный режим
    HAL_GPIO_WritePin( RF_SET_GPIO_Port, RF_SET_Pin, GPIO_PIN_RESET );
    //ожидание переходного процесса
    SetTimers( TIMER_READY_HC12, TIME_MODE_CFG );
    while ( GetTimers( TIMER_READY_HC12 ) );
    
    ClearRFRecv();
    HAL_UART_Transmit_IT( &huart2, (uint8_t*)RF_Speed, strlen( RF_Speed ) );
    SetTimers( TIMER_READY_HC12, TIME_ANSWER_CFG );
    while ( GetTimers( TIMER_READY_HC12 ) );

    //шаг 0
    #ifdef HC12_SPEED_1200
        ClearRFRecv();
        HAL_UART_Transmit_IT( &huart2, (uint8_t*)RF_Speed, strlen( RF_Speed ) );
        SetTimers( TIMER_READY_HC12, TIME_ANSWER_CFG );
        while ( GetTimers( TIMER_READY_HC12 ) );
        //проверка ответа
        /*if ( strstr( rf_recv, "OK+" ) == NULL ) {
            HAL_GPIO_WritePin( RF_SET_GPIO_Port, RF_SET_Pin, GPIO_PIN_SET );
            return ERROR;
           }*/ 
        //смена скорости порта
        huart2.Instance = USART2;
        huart2.Init.BaudRate = 1200;
        huart2.Init.WordLength = UART_WORDLENGTH_8B;
        huart2.Init.StopBits = UART_STOPBITS_1;
        huart2.Init.Parity = UART_PARITY_NONE;
        huart2.Init.Mode = UART_MODE_TX_RX;
        huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
        huart2.Init.OverSampling = UART_OVERSAMPLING_16;
        if ( HAL_UART_Init(&huart2) != HAL_OK )
            Error_Handler();
    #endif

    //шаг 1 режим энергосбержения
    ClearRFRecv();
    HAL_UART_Transmit_IT( &huart2, (uint8_t*)RF_PowerMode, strlen( RF_PowerMode ) );
    SetTimers( TIMER_READY_HC12, TIME_ANSWER_CFG );
    while ( GetTimers( TIMER_READY_HC12 ) );
    //проверка ответа
    if ( strstr( rf_recv, "OK+" ) == NULL ) {
        HAL_GPIO_WritePin( RF_SET_GPIO_Port, RF_SET_Pin, GPIO_PIN_SET );
        return ERROR;
       } 
    
    //шаг 2 номер канала
    ClearRFRecv();
    HAL_UART_Transmit_IT( &huart2, (uint8_t*)RF_Chanel, strlen( RF_Chanel ) );
    SetTimers( TIMER_READY_HC12, TIME_ANSWER_CFG );
    while ( GetTimers( TIMER_READY_HC12 ) );
    //проверка ответа
    if ( strstr( rf_recv, "OK+" ) == NULL ) {
        HAL_GPIO_WritePin( RF_SET_GPIO_Port, RF_SET_Pin, GPIO_PIN_SET );
        return ERROR;
       } 
    
    //шаг 3 выходная мощность 
    ClearRFRecv();
    HAL_UART_Transmit_IT( &huart2, (uint8_t*)RF_Power, strlen( RF_Power ) );
    SetTimers( TIMER_READY_HC12, TIME_ANSWER_CFG );
    while ( GetTimers( TIMER_READY_HC12 ) );
    //проверка ответа
    if ( strstr( rf_recv, "OK+" ) == NULL ) {
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
static void ClearRFRecv( void ) {

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
// Возвращает кол-во принятых байт от радио модуля
//**********************************************************************************
static uint8_t RFRecvCnt( void ) {

    return huart2.pRxBuffPtr - (uint8_t *)rf_recv;
 }
 
//**********************************************************************************
// Передача пакета
// uint8_t *data - указатель на блок данных для отправки
// uint8_t wait  - признак ожидания ответа
//**********************************************************************************
uint8_t RFSend( uint8_t *data, uint8_t wait ) {

    if ( init_flg == false )
        return ERROR; //инициализация радио модуля не прошла
    if ( data == NULL )
        return ERROR;
    if ( GetTimers( TIMER_SEND_EPC ) )
        return ERROR;   //предыдущий пакет отправлен но еще нет ответа
    LedAction();        //индикация активности обмена с радио модулем
    ClearRFRecv();
    ClearRFSend();
    if ( wait == WAIT_ANSWER ) {
        //включим признак ожидания ответа
        send_flg = true;
        //запустим таймер ожидания ответа
        SetTimers( TIMER_SEND_EPC, TIME_ANSWER_EPC );
       }
    memcpy( rf_send, data, PacketSize( PACKET_AP_EPC ) );    
    HAL_UART_Transmit_IT( &huart2, (uint8_t*) rf_send, PacketSize( PACKET_AP_EPC ) );
    return SUCCESS;
 }
 
//**********************************************************************************
// Прием данных по радио каналу
// Вызов из stm32f1xx_it.c (USART2_IRQHandler)
//**********************************************************************************
void RFRecv( void ) {

    LedAction(); //индикация активности обмена с радио модулем
    if ( RFRecvCnt() > BUFFER_EPC - 2 ) {
        ClearRFRecv();
        return;
       }
    if ( init_flg == true && send_flg == true && RFRecvCnt() == PacketSize( PACKET_EPC_AP ) ) {
        //пакет принят, на проверку
        send_flg = false;
        recv_flg = true;
        SetTimers( TIMER_SEND_EPC, TIME_ANSWER_POST );
       } 
 }
 
//**********************************************************************************
// Ожидание/обработка ответа
// Вызов из main-while()
//**********************************************************************************
void RFAnswer( void ) {

    uint8_t chk_pack;
    
    //проверка принятого пакета
    if ( recv_flg == true ) {
        recv_flg = false;
        chk_pack = CheckPacket( (uint8_t*)rf_recv );
        ClearRFRecv();
        if ( chk_pack == CHECK_SUCCESS ) {
            HostOut( "RECV OK\r" );
            EPC_Answer();
            return;
           }
        if ( chk_pack == ERROR_HEADTAIL )
            HostOut( "ERROR_HEADTAIL\r" );
        if ( chk_pack == ERROR_CRC_DATA )
            HostOut( "ERROR_CRC_DATA\r" );
        if ( chk_pack == ERROR_CRC_PACK )
            HostOut( "ERROR_CRC_PACK\r" );
        if ( chk_pack == ERROR_SENDER )
            HostOut( "ERROR_SENDER\r" );
        if ( chk_pack == ERROR_RECIVER )
            HostOut( "ERROR_RECIVER\r" );
        if ( chk_pack == ERROR_DECRYPT )
            HostOut( "ERROR_DECRYPT\r" );
       }
    //проверка таймера ожидания ответа
    if ( send_flg == true && !GetTimers( TIMER_SEND_EPC ) ) {
        send_flg = false; //данных нет, выход по time-out
        HostOut( "TIME OUT\r" );
       }
 }
