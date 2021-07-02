
//**********************************************************************************
//
// Низкоуровневые функции управление счетчиком электроэнергии STPM01
// 
//**********************************************************************************

#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "stm32f1xx.h"

#include "tim.h"
#include "gpio.h"

#include "timers.h"
#include "epc.h"
#include "fram.h"
#include "epc_spi.h"
#include "epc_def.h"
#include "debug.h"

//**********************************************************************************
// Локальные константы
//**********************************************************************************
//Управление сигналами
#define SCS0            0x00    //управление сигналом SCS = 0
#define SCS1            0x01    //управление сигналом SCS = 1
#define SYN0            0x00    //управление сигналом SYN = 0
#define SYN1            0x02    //управление сигналом SYN = 1
#define SCL0            0x00    //управление сигналом SCL = 0
#define SCL1            0x04    //управление сигналом SCL = 1
#define SDA0            0x00    //управление сигналом SDA = 0
#define SDA1            0x08    //управление сигналом SDA = 1
#define SDA_RD          0x10    //читаем данные
#define SDA_WR          0x20    //выводим данные
#define SDA_LBL         0x40    //метка начала цикла чтения
#define SDA_RPT         0x80    //повторяем цикл чтение

//смещение блоков данных (регистров) в общем массиве epc_data[]
#define OFFSET_DAP      0
#define OFFSET_DRP      4
#define OFFSET_DSP      8
#define OFFSET_DFP      12
#define OFFSET_DEV      16
#define OFFSET_DMV      20
#define OFFSET_CFL      24
#define OFFSET_CFH      28

#define COUNT_REG       60      //макс. кол-во регистров для конфигурирования STPM01

//последовательность сброса
static uint8_t const epc_rs[] = {
                SCS1 | SYN1 | SCL1,
                SCS0 | SYN1 | SCL1,
                SCS0 | SYN0 | SCL1,
                SCS0 | SYN0 | SCL0,
                SCS0 | SYN0 | SCL0 | SDA_WR | SDA0,
                SCS0 | SYN0 | SCL0 | SDA_WR | SDA1,
                SCS0 | SYN0 | SCL0 | SDA_WR | SDA0,
                SCS0 | SYN0 | SCL0 | SDA_WR | SDA1,
                SCS0 | SYN0 | SCL0,
                SCS0 | SYN0 | SCL0,
                SCS0 | SYN0 | SCL0,
                SCS0 | SYN0 | SCL0,
                SCS0 | SYN0 | SCL1,
                SCS0 | SYN1 | SCL1,
                SCS1 | SYN1 | SCL1
            };

//последовательность чтения данных
static uint8_t const epc_rd[] = {
                //подготовка к чтению
                SCS1 | SYN1 | SCL1,
                SCS1 | SYN0 | SCL1,
                SCS1 | SYN0 | SCL1,
                SCS1 | SYN0 | SCL1,
                SCS1 | SYN1 | SCL1,
                SCS1 | SYN1 | SCL1,
                SCS0 | SYN1 | SCL1,
                SCS0 | SYN0 | SCL1,
                SCS0 | SYN1 | SCL1,
                //чтение данных
                SCS0 | SYN1 | SCL0 | SDA_LBL,
                SCS0 | SYN1 | SCL1 | SDA_RD, 
                SCS0 | SYN1 | SCL0,
                SCS0 | SYN1 | SCL1 | SDA_RD, 
                SCS0 | SYN1 | SCL0,
                SCS0 | SYN1 | SCL1 | SDA_RD, 
                SCS0 | SYN1 | SCL0,
                SCS0 | SYN1 | SCL1 | SDA_RD, 
                SCS0 | SYN1 | SCL0,
                SCS0 | SYN1 | SCL1 | SDA_RD, 
                SCS0 | SYN1 | SCL0,
                SCS0 | SYN1 | SCL1 | SDA_RD, 
                SCS0 | SYN1 | SCL0,
                SCS0 | SYN1 | SCL1 | SDA_RD, 
                SCS0 | SYN1 | SCL0,
                SCS0 | SYN1 | SCL1 | SDA_RD | SDA_RPT
            };

//последовательность записи
static uint8_t const epc_wr[] = {
                SCS1 | SYN1 | SCL1,
                SCS0 | SYN1 | SCL1,
                SCS0 | SYN0 | SCL1,
                SCS0 | SYN0 | SCL0 | SDA_WR,
                SCS0 | SYN0 | SCL1,
                SCS0 | SYN0 | SCL0 | SDA_WR,
                SCS0 | SYN0 | SCL1,
                SCS0 | SYN0 | SCL0 | SDA_WR,
                SCS0 | SYN0 | SCL1,
                SCS0 | SYN0 | SCL0 | SDA_WR,
                SCS0 | SYN0 | SCL1,
                SCS0 | SYN0 | SCL0 | SDA_WR,
                SCS0 | SYN0 | SCL1,
                SCS0 | SYN0 | SCL0 | SDA_WR,
                SCS0 | SYN0 | SCL1,
                SCS0 | SYN0 | SCL0 | SDA_WR,
                SCS0 | SYN0 | SCL1,
                SCS0 | SYN0 | SCL0 | SDA_WR,
                SCS0 | SYN0 | SCL1,
                SCS0 | SYN1 | SCL1,
                SCS1 | SYN1 | SCL1
            };

//**********************************************************************************
// Локальные переменные
//**********************************************************************************
EPC_struct EPCData;
static uint8_t seq_wrt, ind_rpt, cnt_byte_rd, data;
static uint8_t epc_mask, epc_reg_cnt, ind_wrt, epc_data[32], epc_reg[COUNT_REG];
static bool enb_read = true, epc_wrt = false;

//**********************************************************************************
// Локальные прототипы функций
//**********************************************************************************
static void EPCWrite( void );
static uint8_t CheckParity( uint8_t *bp );
static void EPCDataCheck( void );

//**********************************************************************************
// Формирование тактирования управляющей последовательности записи регистров STPM01
// Вызов из TIM2_IRQHandler stm32f1xx_it.c 
// T = 15us F = 
//**********************************************************************************
void EPC_Clk( void ) {

    //HAL_GPIO_TogglePin( GPIOA, EPC_SCL_Pin );
    if ( epc_wrt == true ) {
        //запретим периодическое чтение
        enb_read = false; 
        EPCWrite();
       }
 }

//**********************************************************************************
// Программный сброс счетчика STPM01
//**********************************************************************************
void EPC_Reset( void ) {

    static uint8_t seq_rst;
    GPIO_InitTypeDef GPIO_InitStruct;

    //начальное состояние выходов управления
    HAL_GPIO_WritePin( GPIOA, EPC_SCS_Pin, GPIO_PIN_SET );
    HAL_GPIO_WritePin( GPIOB, EPC_SYN_Pin, GPIO_PIN_SET );
    HAL_GPIO_WritePin( GPIOA, EPC_SCL_Pin, GPIO_PIN_SET );
    //перенастраиваем SDA на выход
    GPIO_InitStruct.Pin = EPC_SDA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init( EPC_SDA_GPIO_Port, &GPIO_InitStruct );
    HAL_GPIO_WritePin( EPC_SDA_GPIO_Port, EPC_SDA_Pin, GPIO_PIN_SET );
    //цикл последовательности сброса
    for ( seq_rst = 0; seq_rst < sizeof( epc_rs ); seq_rst++ )  {
        //управление сигналом SCS
        if ( epc_rs[seq_rst] & SCS1 )
            HAL_GPIO_WritePin( GPIOA, EPC_SCS_Pin, GPIO_PIN_SET );
        if ( !( epc_rs[seq_rst] & SCS1 ) )
            HAL_GPIO_WritePin( GPIOA, EPC_SCS_Pin, GPIO_PIN_RESET );
        //управление сигналом SYN
        if ( epc_rs[seq_rst] & SYN1 )
            HAL_GPIO_WritePin( GPIOB, EPC_SYN_Pin, GPIO_PIN_SET );
        if ( !( epc_rs[seq_rst] & SYN1 ) )
            HAL_GPIO_WritePin( GPIOB, EPC_SYN_Pin, GPIO_PIN_RESET );
        //управление сигналом SCL
        if ( epc_rs[seq_rst] & SCL1 )
            HAL_GPIO_WritePin( GPIOA, EPC_SCL_Pin, GPIO_PIN_SET );
        if ( !( epc_rs[seq_rst] & SCL1 ) )
            HAL_GPIO_WritePin( GPIOA, EPC_SCL_Pin, GPIO_PIN_RESET );
        //управление сигналом SDA
        if ( epc_rs[seq_rst] & SDA_WR ) {
            //разрешена запись
            if ( epc_rs[seq_rst] & SDA1 )
                HAL_GPIO_WritePin( EPC_SDA_GPIO_Port, EPC_SDA_Pin, GPIO_PIN_SET );
            if ( !( epc_rs[seq_rst] & SDA1 ) )
                HAL_GPIO_WritePin( EPC_SDA_GPIO_Port, EPC_SDA_Pin, GPIO_PIN_RESET );
           }
       }
    //начальная инициализация выводов управления
    HAL_GPIO_WritePin( GPIOA, EPC_SCS_Pin, GPIO_PIN_SET );
    HAL_GPIO_WritePin( GPIOA, EPC_SCL_Pin, GPIO_PIN_SET );
    HAL_GPIO_WritePin( GPIOB, EPC_SYN_Pin, GPIO_PIN_SET );
 }

//**********************************************************************************
// Чтение данных счетчика STPM01 (32 байта) 
// прочитанные данные помещаются в EPCData
//**********************************************************************************
void EPC_ReadData( void ) {

    static uint8_t seq_rd;
    GPIO_InitTypeDef GPIO_InitStruct;
    
    if ( enb_read == false )
        return; //идет запись, не читаем
    data = 0x00;
    seq_rd = 0;
    ind_rpt = 255;
    cnt_byte_rd = 0;
    memset( epc_data, 0x00, sizeof( epc_data ) );
    //начальная инициализация выводов управления
    HAL_GPIO_WritePin( GPIOA, EPC_SCS_Pin, GPIO_PIN_SET );
    HAL_GPIO_WritePin( GPIOA, EPC_SCL_Pin, GPIO_PIN_SET );
    HAL_GPIO_WritePin( GPIOB, EPC_SYN_Pin, GPIO_PIN_SET );
    //перенастраиваем SDA на вход
    GPIO_InitStruct.Pin = EPC_SDA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init( EPC_SDA_GPIO_Port, &GPIO_InitStruct );
    //запуск чтения
    for ( ; seq_rd < sizeof( epc_rd ); ) {
        //управление сигналом SCS
        if ( epc_rd[seq_rd] & SCS1 )
            HAL_GPIO_WritePin( GPIOA, EPC_SCS_Pin, GPIO_PIN_SET );
        if ( !( epc_rd[seq_rd] & SCS1 ) )
            HAL_GPIO_WritePin( GPIOA, EPC_SCS_Pin, GPIO_PIN_RESET );
        //управление сигналом SYN
        if ( epc_rd[seq_rd] & SYN1 )
            HAL_GPIO_WritePin( GPIOB, EPC_SYN_Pin, GPIO_PIN_SET );
        if ( !( epc_rd[seq_rd] & SYN1 ) )
            HAL_GPIO_WritePin( GPIOB, EPC_SYN_Pin, GPIO_PIN_RESET );
        //управление сигналом SCL
        if ( epc_rd[seq_rd] & SCL1 )
            HAL_GPIO_WritePin( GPIOA, EPC_SCL_Pin, GPIO_PIN_SET );
        if ( !( epc_rd[seq_rd] & SCL1 ) )
            HAL_GPIO_WritePin( GPIOA, EPC_SCL_Pin, GPIO_PIN_RESET );
        //проверка метки цикличности
        if ( epc_rd[seq_rd] & SDA_LBL && ind_rpt == 255 )
            ind_rpt = seq_rd; //зафиксируем метку для повтора
        //чтение данных
        if ( epc_rd[seq_rd] & SDA_RD ) {
            //чтение данных, считывается от старшего бита (MSB) к младшему (LSB)
            if ( HAL_GPIO_ReadPin( EPC_SDA_GPIO_Port, EPC_SDA_Pin ) ) {
                data <<= 1;
                data |= 0x01;
               }
            else data <<= 1;
           }
        if ( epc_rd[seq_rd] & SDA_RPT ) {
            //проверка перехода на начало цикла
            seq_rd = ind_rpt;  //переход на начало цикла повтора
            epc_data[cnt_byte_rd] = data;
            data = 0x00;
            cnt_byte_rd++;      //следующий байт
            if ( cnt_byte_rd >= 32 )
                break;
            continue;
           }
        seq_rd++;
       }
    //начальная инициализация выводов управления
    HAL_GPIO_WritePin( GPIOA, EPC_SCS_Pin, GPIO_PIN_SET );
    HAL_GPIO_WritePin( GPIOA, EPC_SCL_Pin, GPIO_PIN_SET );
    HAL_GPIO_WritePin( GPIOB, EPC_SYN_Pin, GPIO_PIN_SET );
    //данные прочитаны, разрешим обработку
    //epc_out = true;
    EPCDataCheck();
 }

//**********************************************************************************
// Очистка списка регистров конфигурации STPM01
//**********************************************************************************
void EPC_ClrListReg( void ) {

    static uint8_t i;
    
    //обнулим массив регистров
    epc_reg_cnt = 0;
    for ( i = 0; i < COUNT_REG; i++ )
        epc_reg[i] = 0;
   }

//**********************************************************************************
// Формируем список адресов регистров и данных для конфигурации STPM01
// uint8_t reg  - номер регистра (ID)
// uint8_t bits - кол-во бит регистра (разрядность)
// uint8_t data - данные 0-255
//**********************************************************************************
void EPC_AddReg( uint8_t reg, uint8_t bits, uint8_t data ) {

    static uint8_t addr, ind, bmsk;
    
    if ( epc_reg_cnt >= COUNT_REG )
        return;
    if ( !bits )
        return;
    for ( ind = 0, bmsk = 0x01; ind < bits; ind++, reg++, bmsk <<= 1 ) {
        if ( epc_reg_cnt >= COUNT_REG )
            break;
        //формируем адрес регистра
        addr = reg & 0x3F;
        addr <<= 1;
        //формируем данные регистра
        if ( data & bmsk )
            addr |= 0x80;
        epc_reg[epc_reg_cnt] = addr;
        epc_reg_cnt++;
       }
 }

//**********************************************************************************
// Запись блока регистров конфигурации в STPM01
//**********************************************************************************
void EPC_SaveReg( void ) {

    GPIO_InitTypeDef GPIO_InitStruct;
    
    seq_wrt = 0;
    ind_wrt = 0;
    epc_mask = 0x80; //начинаем передавать со старшего бита
    //начальное состояние выходов управления
    HAL_GPIO_WritePin( GPIOA, EPC_SCS_Pin, GPIO_PIN_SET );
    HAL_GPIO_WritePin( GPIOB, EPC_SYN_Pin, GPIO_PIN_SET );
    HAL_GPIO_WritePin( GPIOA, EPC_SCL_Pin, GPIO_PIN_SET );
    //перенастраиваем SDA на выход
    GPIO_InitStruct.Pin = EPC_SDA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init( EPC_SDA_GPIO_Port, &GPIO_InitStruct );
    HAL_GPIO_WritePin( EPC_SDA_GPIO_Port, EPC_SDA_Pin, GPIO_PIN_SET );
    //разрешаем запись
    epc_wrt = true;
 }

//**********************************************************************************
// Запись регистров конфигурации STPM01
//**********************************************************************************
static void EPCWrite( void ) {

    //управление сигналом SCS
    if ( epc_wr[seq_wrt] & SCS1 )
        HAL_GPIO_WritePin( GPIOA, EPC_SCS_Pin, GPIO_PIN_SET );
    if ( !( epc_wr[seq_wrt] & SCS1 ) )
        HAL_GPIO_WritePin( GPIOA, EPC_SCS_Pin, GPIO_PIN_RESET );
    //управление сигналом SYN
    if ( epc_wr[seq_wrt] & SYN1 )
        HAL_GPIO_WritePin( GPIOB, EPC_SYN_Pin, GPIO_PIN_SET );
    if ( !( epc_wr[seq_wrt] & SYN1 ) )
        HAL_GPIO_WritePin( GPIOB, EPC_SYN_Pin, GPIO_PIN_RESET );
    //управление сигналом SCL
    if ( epc_wr[seq_wrt] & SCL1 )
        HAL_GPIO_WritePin( GPIOA, EPC_SCL_Pin, GPIO_PIN_SET );
    if ( !( epc_wr[seq_wrt] & SCL1 ) )
        HAL_GPIO_WritePin( GPIOA, EPC_SCL_Pin, GPIO_PIN_RESET );
    //управление сигналом SDA
    if ( epc_wr[seq_wrt] & SDA_WR ) {
        //разрешена запись
        if ( epc_reg[ind_wrt] & epc_mask )  
            HAL_GPIO_WritePin( EPC_SDA_GPIO_Port, EPC_SDA_Pin, GPIO_PIN_SET );
        else HAL_GPIO_WritePin( EPC_SDA_GPIO_Port, EPC_SDA_Pin, GPIO_PIN_RESET );
        epc_mask >>= 1; //передаем от старшего к младшему
       }
    seq_wrt++;
    if ( seq_wrt >= sizeof( epc_wr ) ) {
        //конец цикла записи одного регистра
        ind_wrt++;       //следующий регистр
        epc_mask = 0x80; //начинаем передавать со старшего бита
        if ( ind_wrt >= epc_reg_cnt ) {
            //конец цикла записи всех регистров
            EPC_ClrListReg();
            //выключаем процедуру записи
            epc_wrt = false;
            //начальная инициализация выводов управления
            HAL_GPIO_WritePin( GPIOA, EPC_SCS_Pin, GPIO_PIN_SET );
            HAL_GPIO_WritePin( GPIOA, EPC_SCL_Pin, GPIO_PIN_SET );
            HAL_GPIO_WritePin( GPIOB, EPC_SYN_Pin, GPIO_PIN_SET );
            //разрешим периодическое чтение данных
            enb_read = true;
            return;
           }
         else seq_wrt = 0; //продолжаем со следующего регистра
       }
 }

//**********************************************************************************
// Проверка четности прочитанных данных из STMP01
//**********************************************************************************
static void EPCDataCheck( void ) {

    static uint8_t chk = 0, conf_chv = 0;
    
    memcpy( &EPCData, epc_data, sizeof( epc_data ) );
    //проверка четности данных
    if ( CheckParity( epc_data + OFFSET_DAP ) )
        chk++;
    if ( CheckParity( epc_data + OFFSET_DRP ) )
        chk++;
    if ( CheckParity( epc_data + OFFSET_DSP ) )
        chk++;
    if ( CheckParity( epc_data + OFFSET_DFP ) )
        chk++;
    if ( CheckParity( epc_data + OFFSET_DEV ) )
        chk++;
    if ( CheckParity( epc_data + OFFSET_DMV ) )
        chk++;
    if ( CheckParity( epc_data + OFFSET_CFL ) )
        chk++;
    if ( CheckParity( epc_data + OFFSET_CFH ) )
        chk++;
    //проверка валидности данных для всех регистров
    if ( chk )
        EPCData.data_parity = 0;        //четность нарушена
    else EPCData.data_parity = 1;       //четность не нарушена
    if ( EPCData.status & STATUS_HLT )
        EPCData.data_valid = 0;         //данные выданные STPM01 не достоверны
    else EPCData.data_valid = 1;        //данные выданные STPM01 достоверны
    //сформируем полное значение config_chv
    conf_chv = EPCData.config_chv2;
    conf_chv <<= 4;
    EPCData.config_chv = conf_chv | EPCData.config_chv1;
 }

//**********************************************************************************
// Контроль четности данных регистров
// uint8_t *bp - адрес регистра
// return      = 0 - четность не нарушена
//               1 - четность нарушена
//**********************************************************************************
static uint8_t CheckParity( uint8_t *bp ) {

    static uint8_t prty;
    
    prty = *bp,                         //take the 1st byte of data
    prty ^= *(bp+1),                    //XOR it with the 2nd byte
    prty ^= *(bp+2),                    //and with the 3rd byte
    prty ^= *(bp+3),                    //and with the 4th byte
    prty ^= prty << 4, prty &= 0xF0;    //combine and remove the lower nibble
    return ( prty != 0xF0 );            //возвращает 1 - четность нарушена
 }

