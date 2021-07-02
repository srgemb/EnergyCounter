
//**********************************************************************************
//
// Управление внешней FRAM памятью
// 
//**********************************************************************************

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

#include "stm32f1xx.h"

#include "i2c.h"
#include "fram.h"
#include "crc16.h"
#include "packet.h"
#include "xtime.h"

//**********************************************************************************
// Локальные константы
//**********************************************************************************
#define FRAM_ADDR           0xA0                    //ID памяти FRAM
#define I2C_TIME_OUT        40000                   //время ожидания ответа по шине i2c

//**********************************************************************************
// Внешние переменные
//**********************************************************************************
extern I2C_HandleTypeDef hi2c1;

//**********************************************************************************
// Локальные переменные
//**********************************************************************************
uint16_t intvl_page = 0;    //фактический адрес первой свободной страницы 
                            //памяти в FRAM для хранения интервальных значений

#pragma pack( push, 1 )

//структура для проверки целостности данных на одной странице FRAM
typedef struct {
    uint8_t     data[FRAM_PAGE-sizeof(uint16_t)];
    uint16_t    crc;
} FRAM_DATA; 

//структура для хранения накопительных показаний расхода "ЭЭ" в FRAM
typedef struct {
    uint8_t     unused[FRAM_PAGE-sizeof(uint16_t)-sizeof(double)];
    double      counter;
    uint16_t    crc;
} TARIF_DATA; 

//структура для хранения интервальных показаний расхода "ЭЭ" в FRAM
//сохранение в 00 и 30 минут, каждый час, глубина: 5 суток
typedef struct {
    uint8_t     day;
    uint8_t     month;
    uint16_t    year;
    uint8_t     hour;
    uint8_t     minute;
    double      counter;
    uint16_t    crc;
} INTERVAL; 

#pragma pack( pop )

INTERVAL    interval;
FRAM_DATA   fdata;
TARIF_DATA  ftarif; 

//**********************************************************************************
// Прототипы локальные функций
//**********************************************************************************
static uint16_t PageInc( void );

//**********************************************************************************
// Проверяет контрольные суммы страниц в FRAM
// uint16_t = 0 - ошибок нет
//          > 0 - номер страницы (блока) содержащей ошибку (чтения или КС)
//                для получения фактического значения надо вычесть "1"
//**********************************************************************************
uint16_t FRAMCheck( void ) {

    uint16_t crc, page, addr;

    for ( page = 0; page < FRAM_PAGES; page++ ) {
        addr = FRAM_PAGE * page;
        //читаем страницу
        if ( FRAMReadPage( addr ) == NULL )
            return page + 1;  //ошибка чтения данных
        if ( fdata.crc != 0x0000 ) {
            //есть КС, блок заполнен, проверим КС
            crc = GetCRC16( (uint8_t *) &fdata, sizeof( fdata.data ) );
            if ( crc != fdata.crc )
                return page + 1; //КС не совпали
           }
       }
    return 0;
 } 

//**********************************************************************************
// Сохраняем в памяти FRAM значение показаний счетчиков
// uint8_t id_tarif - номер тарифа 
// uint32_t value   - значение расхода "ЭЭ" для указанного тарифа
//**********************************************************************************
uint8_t FRAMSavePwr( uint8_t id_tarif, double value ) {

    uint16_t addr, crc;
    
    if ( value < 0 )
        return ERROR;
    if ( id_tarif != TARIFF_DAILY && id_tarif != TARIFF_NIGHT )
        return ERROR;
    //очищаем блок данных
    memset( &ftarif, 0x00, sizeof( ftarif ) );
    //определим адрес тарифа
    if ( id_tarif == TARIFF_DAILY )
        addr = FRAM_PAGE_TAR1 * FRAM_PAGE;
    if ( id_tarif == TARIFF_NIGHT )
        addr = FRAM_PAGE_TAR2 * FRAM_PAGE;
    //читаем блок
    if ( HAL_I2C_Mem_Read( &hi2c1, FRAM_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)&ftarif, sizeof( ftarif ), I2C_TIME_OUT ) != HAL_OK )
        return ERROR;
    //проверим КС
    if ( ftarif.crc && ftarif.counter > 0 ) {
        crc = GetCRC16( (uint8_t *)&ftarif, sizeof( ftarif ) - 2 );
        if ( ftarif.crc != crc )
            return ERROR;
       }
    ftarif.counter = value;
    //пересчитаем КС и сохраним
    if ( value != 0 )
        ftarif.crc = GetCRC16( (uint8_t *)&ftarif, sizeof( ftarif ) - 2 );
    else ftarif.crc = 0x0000; //для нулевого значения КС не считаем
    if ( HAL_I2C_Mem_Write( &hi2c1, FRAM_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)&ftarif, sizeof( ftarif ), I2C_TIME_OUT ) != HAL_OK )
        return ERROR;
    return SUCCESS;
 } 

//**********************************************************************************
// Возвращает показания счетчика тарифа
// uint8_t id_tarif - номер тарифа 1,2 
// uint32_t counter - значение расхода "ЭЭ" для указанного тарифа
//**********************************************************************************
uint8_t FRAMGetPwr( uint8_t id_tarif, double *counter ) {

    uint16_t addr, crc;
    
    if ( id_tarif != TARIFF_DAILY && id_tarif != TARIFF_NIGHT )
        return ERROR;
    *counter = 0;
    //очищаем блок данных
    memset( &ftarif, 0x00, sizeof( ftarif ) );
    //определим адрес тарифа
    if ( id_tarif == TARIFF_DAILY )
        addr = FRAM_PAGE_TAR1 * FRAM_PAGE;
    if ( id_tarif == TARIFF_NIGHT )
        addr = FRAM_PAGE_TAR2 * FRAM_PAGE;
    //читаем блок
    if ( HAL_I2C_Mem_Read( &hi2c1, FRAM_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)&ftarif, sizeof( ftarif ), I2C_TIME_OUT ) != HAL_OK )
        return ERROR;
    //проверим КС
    if ( ftarif.crc ) {
        crc = GetCRC16( (uint8_t *)&ftarif, sizeof( ftarif ) - 2 );
        if ( ftarif.crc != crc )
            return ERROR;
       }
    *counter = ftarif.counter;
    return SUCCESS;
 }

//**********************************************************************************
// Читает один блок (16 байт) из FRAM
//**********************************************************************************
uint8_t *FRAMReadPage( uint16_t addr ) {

    //очищаем блок данных
    memset( &fdata, 0x00, sizeof( fdata ) );
    if ( HAL_I2C_Mem_Read( &hi2c1, FRAM_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)&fdata, sizeof( fdata ), I2C_TIME_OUT ) != HAL_OK )
        return NULL;
    return (uint8_t *)&fdata;
 }

//**********************************************************************************
// Очищаем данные в FRAM, начиная со страницы FRAM_ADDR_CLR
//**********************************************************************************
uint8_t FRAMClear( void ) {

    uint16_t page, addr;

    for ( page = FRAM_ADDR_CLR; page < FRAM_PAGES; page++ ) {
        addr = FRAM_PAGE * page;
        if ( FRAMReadPage( addr ) == NULL )
            return ERROR;
        //очищаем блок данных
        fdata.crc = 0;
        memset( &fdata, 0x00, sizeof( fdata ) );
        if ( HAL_I2C_Mem_Write( &hi2c1, FRAM_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)&fdata, sizeof( fdata ), I2C_TIME_OUT ) != HAL_OK )
            return ERROR;
       }
    intvl_page = 0; //обнулим номер страницы интервального хранения показаний
    return SUCCESS;
 }

//**********************************************************************************
// Сохраняем в памяти FRAM значение калибровочных констатант
// EPC_CALIBRN *epc_clb - указатель на структуру
// Result = ERROR       - ошибка сохранения данных
//          SUCCESS     - ОК
//**********************************************************************************
uint8_t FRAMSaveEPC( EPC_CALIBRN *epc_clb ) {

    uint16_t addr;
    
    //пересчитаем КС
    epc_clb->crc = GetCRC16( (uint8_t *)epc_clb, sizeof( EPC_CALIBRN ) - 2 );
    addr = FRAM_PAGE * FRAM_PAGE_EPC;
    if ( HAL_I2C_Mem_Write( &hi2c1, FRAM_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)epc_clb, sizeof( epc_clb ), I2C_TIME_OUT ) != HAL_OK )
        return ERROR;
    return SUCCESS;
 } 

//**********************************************************************************
// Прочитаем из памяти FRAM значение калибровочных констатант в структуру EPC_CALIBRN
// EPC_CALIBRN *epc_clb - указатель на структуру
// Result = ERROR       - ошибка чтения или проверки контрольный суммы
//          SUCCESS     - ОК
//**********************************************************************************
uint8_t FRAMReadEPC( EPC_CALIBRN *epc_clb ) {

    static uint16_t addr, crc;
    
    if ( epc_clb == NULL )
        return ERROR;
    addr = FRAM_PAGE * FRAM_PAGE_EPC;
    memset( epc_clb, 0x00, sizeof( EPC_CALIBRN ) );
    //читаем данные
    if ( HAL_I2C_Mem_Read( &hi2c1, FRAM_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)epc_clb, sizeof( epc_clb ), I2C_TIME_OUT ) != HAL_OK )
        return ERROR;
    if ( epc_clb->crc == 0x0000 )
        return SUCCESS; //блок пустой, КС не проверяем
    //проверим КС
    crc = GetCRC16( (uint8_t *)epc_clb, sizeof( EPC_CALIBRN ) - 2 );
    if ( epc_clb->crc != crc )
        return ERROR;
    return SUCCESS;
 } 

//**********************************************************************************
// Сохраняем в памяти FRAM интервальное значение показаний счетчиков
// double value  - значение тарифа
// если intvl_page = 0 начинаем искать первую свободную страницу
// return = 0 - ошибка записи
//        > 0 - номер следующей свободной страницы
//**********************************************************************************
uint16_t FRAMSaveIntrv( double value ) {

    uint16_t addr;
    timedate td;
    
    if ( value < 0 )
        return ERROR;
    if ( intvl_page ) {
        //указана страница, очищаем блок данных
        memset( &interval, 0x00, sizeof( interval ) );
        GetTimeDate( &td );
        interval.day = td.td_day;
        interval.month = td.td_month;
        interval.year = td.td_year;
        interval.hour = td.td_hour;
        interval.minute = td.td_min;
        interval.counter = value;
        //пересчитаем КС и сохраним
        interval.crc = GetCRC16( (uint8_t *)&interval, sizeof( interval ) - 2 );
        //фактический адрес хранения
        addr = intvl_page * FRAM_PAGE;
        //фактический адрес хранения
        if ( HAL_I2C_Mem_Write( &hi2c1, FRAM_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)&interval, sizeof( interval ), I2C_TIME_OUT ) != HAL_OK )
            return 0;
        //следующий адрес
        PageInc();
        //очистим следующую страницу
        memset( &interval, 0x00, sizeof( interval ) );
        //пересчитаем КС и сохраним
        interval.crc = GetCRC16( (uint8_t *)&interval, sizeof( interval ) - 2 );
        //фактический адрес хранения
        addr = intvl_page * FRAM_PAGE;
        if ( HAL_I2C_Mem_Write( &hi2c1, FRAM_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)&interval, sizeof( interval ), I2C_TIME_OUT ) != HAL_OK )
            return 0;
        else return intvl_page;
       }
    //ищем свободную страницу
    intvl_page = FRAM_INTERVAL;
    do {
        //очищаем блок данных
        memset( &interval, 0x00, sizeof( interval ) );
        //фактический адрес хранения
        addr = intvl_page * FRAM_PAGE;
        //читаем страницу
        if ( HAL_I2C_Mem_Read( &hi2c1, FRAM_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)&interval, sizeof( interval ), I2C_TIME_OUT ) != HAL_OK )
            return 0;
        if ( interval.day || interval.month || interval.year || interval.hour || interval.minute ) {   
            //страница заполнена данными, идем на следующую страницу
            intvl_page++;
            if ( intvl_page >= FRAM_INTERVAL_MAX ) {
                intvl_page = FRAM_INTERVAL;
                return 0;
               }
            continue; //есть данные, идем на следующую страницу
           }
        else {
            //страница пустая, очищаем блок данных 
            memset( &interval, 0x00, sizeof( interval ) );
            GetTimeDate( &td );
            interval.day = td.td_day;
            interval.month = td.td_month;
            interval.year = td.td_year;
            interval.hour = td.td_hour;
            interval.minute = td.td_min;
            interval.counter = value;
            //пересчитаем КС и сохраним
            interval.crc = GetCRC16( (uint8_t *)&interval, sizeof( interval ) - 2 );
            //фактический адрес хранения
            addr = intvl_page * FRAM_PAGE;
            //фактический адрес хранения
            if ( HAL_I2C_Mem_Write( &hi2c1, FRAM_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)&interval, sizeof( interval ), I2C_TIME_OUT ) != HAL_OK )
                return 0;
            //следующий адрес
            PageInc();
            //очистим следующую страницу
            memset( &interval, 0x00, sizeof( interval ) );
            //пересчитаем КС и сохраним
            interval.crc = GetCRC16( (uint8_t *)&interval, sizeof( interval ) - 2 );
            //фактический адрес хранения
            addr = intvl_page * FRAM_PAGE;
            if ( HAL_I2C_Mem_Write( &hi2c1, FRAM_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)&interval, sizeof( interval ), I2C_TIME_OUT ) != HAL_OK )
                return 0;
            else return intvl_page;
           }
       } while( true );
 } 

//**********************************************************************************
// Поиск свободной страницы для записи интервального значения
// Поиск выполняется только при intvl_page = 0
//**********************************************************************************
uint16_t FRAMFindPage( void ) {

    uint16_t addr;

    if ( intvl_page )
        return intvl_page;
    //ищем свободную страницу
    intvl_page = FRAM_INTERVAL;
    do {
        //очищаем блок данных
        memset( &interval, 0x00, sizeof( interval ) );
        //фактический адрес хранения
        addr = intvl_page * FRAM_PAGE;
        //читаем страницу
        if ( HAL_I2C_Mem_Read( &hi2c1, FRAM_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)&interval, sizeof( interval ), I2C_TIME_OUT ) != HAL_OK )
            return 0;
        if ( interval.day || interval.month || interval.year || interval.hour || interval.minute ) {   
            //страница заполнена данными, идем на следующую страницу
            intvl_page++;
            if ( intvl_page >= FRAM_INTERVAL_MAX ) {
                intvl_page = FRAM_INTERVAL;
                return 0;
               }
            continue; //есть данные, идем на следующую страницу
           }
        else return intvl_page; //нашли пустую страницу
       } while( true );
 }

//**********************************************************************************
// Читаем из памяти FRAM интервальное значение показаний счетчиков
// uint16_t page    - фактическая страница
// uint8_t  day     - день
// uint8_t  month   - месяц
// uint8_t  year    - год
// uint8_t  hour    - часы
// uint8_t  minute  - часы
// uint8_t  counter - показания
//**********************************************************************************
uint8_t FRAMReadIntrv( uint16_t page, uint8_t *day, uint8_t *month, uint16_t *year, uint8_t *hour, uint8_t *minute, double *counter ) {

    uint16_t addr, crc;
    
    //обулим показания
    *day = 0;
    *month = 0;
    *year = 0;
    *hour = 0;
    *minute = 0;
    *counter = 0;
    //обнулим массив
    memset( &interval, 0x00, sizeof( interval ) );
    //фактический адрес
    addr = page * FRAM_PAGE;
    if ( HAL_I2C_Mem_Read( &hi2c1, FRAM_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)&interval, sizeof( interval ), I2C_TIME_OUT ) != HAL_OK )
        return ERROR;
    //проверим КС
    crc = GetCRC16( (uint8_t *)&interval, sizeof( interval ) - 2 );
    if ( interval.crc != crc )
        return ERROR;
    *day = interval.day;
    *month = interval.month;
    *year = interval.year;
    *hour = interval.hour;
    *minute = interval.minute;
    *counter = interval.counter;
    return SUCCESS;
 } 

//**********************************************************************************
// Расчитывает физический номер страницы из логицеского номера при циклическом хранении
// uint16_t page - номер страницы 1 ... FRAM_INTERVAL_MAX - FRAM_INTERVAL
//**********************************************************************************
uint16_t PageCalc( uint16_t page ) {

    uint16_t calc;
    
    calc = intvl_page;
    while ( page-- ) {
        calc--;
        if ( calc < FRAM_INTERVAL )
            calc = FRAM_INTERVAL_MAX;
       }
    return calc;
 }

//**********************************************************************************
// Увеличит номер страницы, цикличность FRAM_INTERVAL ... FRAM_INTERVAL_MAX ... FRAM_INTERVAL
//**********************************************************************************
static uint16_t PageInc( void ) {

    intvl_page++;
    if ( intvl_page >= FRAM_INTERVAL_MAX )
        intvl_page = FRAM_INTERVAL;
    return intvl_page;
 }

