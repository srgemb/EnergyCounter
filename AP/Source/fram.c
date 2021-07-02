
//**********************************************************************************
//
// Управление внешней FRAM памятью
// 
//**********************************************************************************

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "stm32f1xx.h"

#include "i2c.h"
#include "fram.h"
#include "crc16.h"
#include "packet.h"

//**********************************************************************************
// Локальные константы
//**********************************************************************************
//номера страниц для хранения данных
#define FRAM_PAGE_DATE      0               //текущая дата
#define FRAM_PAGE_DEV       0               //начальная страница для начала проверки CRC
#define FRAM_ADDR           0xA0            //ID памяти
#define FRAM_TIMEOUT        1000

//**********************************************************************************
// Внешние переменные
//**********************************************************************************
extern RTC_HandleTypeDef hrtc;
extern I2C_HandleTypeDef hi2c1;

//**********************************************************************************
// Локальные переменные
//**********************************************************************************
#pragma pack( push, 1 )

//структура для проверки КС
typedef struct {
    uint8_t     data[14];
    uint16_t    crc;
} FRAM_DATA; 

typedef struct {
    uint16_t    epc_num;
    uint8_t     epc_id[12];
    uint16_t    crc;
} FRAM_DEV_SCAN; 

#pragma pack( pop )

FRAM_DATA       fdata;
FRAM_DEV_SCAN   dscan;

//**********************************************************************************
// Прототипы локальных функций
//**********************************************************************************
static uint16_t FRAMLastFree( void );


//**********************************************************************************
// Проверяет контрольные суммы страниц в FRAM
// uint16_t = 0 - ошибок нет
//          > 0 - номер страницы (блока) содержащей ошибку (чтения или КС)
//                фактическое значения номера страницы = номер_страницы - 1
//**********************************************************************************
uint16_t FRAMCheck( void ) {

    uint16_t crc, page, addr;

    for ( page = FRAM_PAGE_DEV; page < FRAM_PAGES; page++ ) {
        addr = FRAM_PAGE * page;
        //читаем страницу
        if ( FRAMReadPage( addr ) == NULL )
            return page + 1;
        if ( fdata.crc != 0x0000 ) {
            //есть КС, блок заполнен, проверим КС
            crc = GetCRC16( (uint8_t *) &fdata, sizeof( fdata.data ) );
            if ( crc != fdata.crc )
                return page + 1;
           }
       }
    return 0;
 } 

//**********************************************************************************
// Сохраним дату в FRAM
//**********************************************************************************
/*uint8_t SaveDate( uint8_t date, uint8_t month, uint8_t year ) {

    //очищаем блок данных
    memset( &fdata, 0x00, sizeof( fdata ) );
    fdata.data[0] = date;
    fdata.data[1] = month;
    fdata.data[2] = year;
    //расчитаем КС
    fdata.crc = GetCRC16( (uint8_t *)&fdata, sizeof( fdata.data ) );
    //читаем данные
    if ( HAL_I2C_Mem_Write( &hi2c1, FRAM_ADDR, FRAM_PAGE_DATE, I2C_MEMADD_SIZE_16BIT, (uint8_t *)&fdata, sizeof( fdata ), FRAM_TIMEOUT ) != HAL_OK )
        return ERROR;
    return SUCCESS;
 }*/

//**********************************************************************************
// Читает одну страницу памяти из FRAM
// uint16_t addr - адрес (не номер страницы !!!) в памяти FRAM
//**********************************************************************************
uint8_t *FRAMReadPage( uint16_t addr ) {

    //очищаем блок данных
    memset( &fdata, 0x00, sizeof( fdata ) );
    if ( HAL_I2C_Mem_Read( &hi2c1, FRAM_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)&fdata, sizeof( fdata ), FRAM_TIMEOUT ) != HAL_OK )
        return NULL;
    return (uint8_t *) &fdata;
 }

//**********************************************************************************
// Находит первую свободную страницу в FRAM
// result = 0 - все занято
//        = N - номер страницы 1 ... 511
//**********************************************************************************
static uint16_t FRAMLastFree( void ) {

    uint16_t page, addr;

    for ( page = 1; page < FRAM_PAGES; page++ ) {
        addr = FRAM_PAGE * page;
        if ( FRAMReadPage( addr ) == NULL )
            return 0;
        if ( fdata.crc == 0x0000 )
            return page; //страница свободна
       }
    return 0;
 }

//**********************************************************************************
// Добавляем данные страницу в FRAM
// uint16_t epc_num - номер уст-ва 
// uint8_t *epc_id  - указатель на строку содержащию ID уст-ва "8721233657488651066EFF50"
//**********************************************************************************
uint16_t FRAMAddPage( uint16_t epc_num, uint8_t *epc_id ) {

    uint8_t i;
    uint16_t page, addr;
    
    page = FRAMLastFree();
    if ( !page || page > 511 )
        return ERROR;
    if ( strlen( (char *)epc_id ) != 24 )
        return ERROR;
    addr = FRAM_PAGE * page;
    //очищаем блок данных
    memset( &fdata, 0x00, sizeof( fdata ) );
    //номер уст-ва
    fdata.data[0] = (uint8_t) epc_num & 0x00FF;
    fdata.data[1] = (uint8_t) ( ( epc_num & 0xFF00 ) >> 8 );
    //сворачиваем строковой ID уст-ва в HEX
    for ( i = 0; i < 12; i++ )
        fdata.data[i+2] = Hex2Int( epc_id + i * 2 );
    //расчитаем КС
    fdata.crc = GetCRC16( (uint8_t *)&fdata, sizeof( fdata.data ) );
    //записываем данные
    if ( HAL_I2C_Mem_Write( &hi2c1, FRAM_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)&fdata, sizeof( fdata ), FRAM_TIMEOUT ) != HAL_OK )
        return ERROR;
    return SUCCESS;
 }

//**********************************************************************************
// Очищаем данные в FRAM, кроме FRAM_ADDR_DATE
//**********************************************************************************
uint8_t FRAMClear( void ) {

    uint16_t page, addr;

    for ( page = 1; page < FRAM_PAGES; page++ ) {
        addr = FRAM_PAGE * page;
        if ( FRAMReadPage( addr ) == NULL )
            return ERROR;
        if ( fdata.crc != 0x0000 ) {
            //блок заполнен, т.е. есть КС
            //очищаем блок данных
            memset( &fdata, 0x00, sizeof( fdata ) );
            if ( HAL_I2C_Mem_Write( &hi2c1, FRAM_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)&fdata, sizeof( fdata ), FRAM_TIMEOUT ) != HAL_OK )
                return ERROR;
           }
       }
    return SUCCESS;
 }

//**********************************************************************************
// Возвращает параметры уст-ва для сканирования
// uint16_t index - номер уст-ва в списке (номер страницы)
// uint16_t *epc_num, 
// uint8_t *epc_id
//**********************************************************************************
uint16_t FRAMGetDev( uint16_t index, uint16_t *epc_num, uint8_t **epc_id ) {

    uint16_t addr, crc;
    
    //рассчитаем номер страницы
    addr = FRAM_PAGE * index;
    //проверка КС
    if ( FRAMReadPage( addr ) == NULL )
        return ERROR;
    if ( fdata.crc != 0x0000 ) {
        //есть КС, блок заполнен
        crc = GetCRC16( (uint8_t *)&fdata, sizeof( fdata.data ) );
        if ( crc != fdata.crc )
            return ERROR;
       }
    //копируем данные из FRAM_DATA в FRAM_DEV_SCAN
    memcpy( &dscan, &fdata, sizeof( dscan ) );
    *epc_num = dscan.epc_num;
    *epc_id = &dscan.epc_id[0];
    return SUCCESS;
 }

