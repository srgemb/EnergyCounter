
//**********************************************************************************
//
// Управление протоколом обмена по радио каналу
// 
//**********************************************************************************

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

#include "epc.h"
#include "crc16.h"
#include "timers.h"
#include "packet.h"
#include "crypto.h"
#include "debug.h"
#include "xtime.h"

//**********************************************************************************
// Локальные константы
//**********************************************************************************
//заголовок/окончание пакета
#define PACKET_HEADER       (uint16_t) 0x5555
#define PACKET_TAIL         (uint16_t) 0xAAAA

#define MASK_TARIF          0x40            //маска определения один/два тарифа
#define MASK_NUMB_DEV       0x3FFF          //маска номера уст-ва

#define ADDR_NUM_DEVICE     0x1FFFF804      //адрес Data0, адрес Data1 = +2
#define ADDR_ID_DEVICE      0x1FFFF7E8      //адрес ID CPU

#define MASK_ACCESS_POINT   0x8000          //маска/признак точки доступа

//параметры блока шифрования
#define CRYPT_START_DATA    6               //начало блока
#define CRYPT_LEN_DATA      16              //размер блока данных шифрования/расшифрования

//параметры расчета контрольной суммы 1 (данные внутри блока шифрования)
#define CRC1_START_DATA     6               //начало блока
#define CRC1_LEN_DATA       14              //длинна блока подсчета КС

//параметры расчета контрольной суммы 2 (всего пакета)
#define CRC2_START_DATA     2               //начало блока
#define CRC2_END_DATA       4               //исключение из окончание пакета

//ключ для широковещательной установки даты/времени
const static uint8_t dtime_key[] = { 0x87,0x21,0xF4,0x17,0x8F,0xB3,0x9C,0xA1,0x0B,0x70,0xFF,0x50 };

//**********************************************************************************
// Форматы пакетов
//**********************************************************************************
#pragma pack( push, 1 )

//**********************************************************************************
// Направление пакета: АСКУЭ -> Счетчик
//**********************************************************************************
typedef struct {
    uint16_t header;                //заголовок
    uint16_t sender;                //отправитель
    uint16_t receiver;              //получатель
    //начало блока шифрования
    uint8_t command;                //команда
    uint8_t interval;               //номер интервала
    unsigned day : 5;               //день
    unsigned mon : 4;               //месяц
    unsigned year : 12;             //год
    unsigned hour : 5;              //часы
    unsigned minute : 6;            //минуты
    unsigned seconds : 6;           //секунды
    unsigned power1 : 30;           //тариф 1
    unsigned power2 : 30;           //тариф 2
    uint16_t crc1;                  //контрольная сумма блока шифрования
    //конец блока шифрования        
    uint16_t crc2;                  //общая контрольная сумма 
    uint16_t tail;                  //окончание пакета
} Packet_ApToEpc; 


//**********************************************************************************
// Направление пакета: Счетчик -> АСКУЭ
//**********************************************************************************
typedef struct {
    uint16_t header;                //заголовок
    uint16_t sender;                //отправитель
    uint16_t receiver;              //получатель
    //начало блока шифрования
    unsigned status : 5;            //статус
    unsigned day : 5;               //день
    unsigned mon : 4;               //месяц
    unsigned year : 12;             //год
    unsigned hour : 5;              //часы
    unsigned minute : 6;            //минуты
    unsigned seconds : 6;           //секунды
    unsigned voltage : 12;          //напряжения
    unsigned power1 : 30;           //тариф 1
    unsigned power2 : 30;           //тариф 2
    uint16_t crc1;                  //контрольная сумма блока шифрования
    //конец блока шифрования        
    uint16_t crc2;                  //общая контрольная сумма 
    uint16_t tail;                  //окончание пакета
} Packet_EpcToAp; 

#pragma pack( pop )

//**********************************************************************************
// Локальные переменные
//**********************************************************************************
Packet_ApToEpc Packet_ATE;
Packet_EpcToAp Packet_ETA;

uint16_t AP_Numb = 0;
uint8_t  input[CRYPT_LEN_DATA];
uint8_t  output[CRYPT_LEN_DATA];
uint8_t  aes_key[CRYPT_LEN_DATA];

//**********************************************************************************
// Прототипы локальные функций
//**********************************************************************************
static uint32_t Encrypt( uint8_t *src, uint8_t *dest, uint16_t key3 );
static uint32_t Decrypt( uint8_t *src, uint8_t *dest, uint16_t key2, const uint8_t *ext_key );

//**********************************************************************************
// Формирование пакета ответа
// uint32_t param1 - значение параметра 1
// uint32_t param2 - значение параметра 2
// uint8_t stat    - статус выполнения команды
// return          - указатель на буфер с данными пакетами
//**********************************************************************************
uint8_t *CrtPacket1( uint32_t param1, uint32_t param2, uint8_t stat ) {

    timedate td;
    static uint8_t *pdat, ldat;

    GetTimeDate( &td );
    
    pdat = (uint8_t*)&Packet_ETA;
    ldat = sizeof( Packet_ETA );
    //формируем пакет
    Packet_ETA.header = PACKET_HEADER;
    Packet_ETA.tail = PACKET_TAIL;
    Packet_ETA.sender = GetNumDev();
    Packet_ETA.receiver = AP_Numb;
    Packet_ETA.status = StatReset() | stat;
    Packet_ETA.day = td.td_day;
    Packet_ETA.mon = td.td_month;
    Packet_ETA.year = td.td_year;
    Packet_ETA.hour = td.td_hour;
    Packet_ETA.minute = td.td_min;
    Packet_ETA.seconds = td.td_sec;
    //параметра пакета
    Packet_ETA.voltage = ACVoltage();
    Packet_ETA.power1 = param1;
    Packet_ETA.power2 = param2;
    Packet_ETA.crc1 = GetCRC16( pdat + CRC1_START_DATA, CRC1_LEN_DATA );
    //сохраним данные для шифрования
    memcpy( input, pdat + CRYPT_START_DATA, CRYPT_LEN_DATA );
    //шифруем данные пакета
    if ( Encrypt( input, output, ( AP_Numb & ~MASK_ACCESS_POINT ) ) != AES_SUCCESS )
        return NULL;
    //зашифрованный блок перенесем обратно в структуру
    memcpy( pdat + CRYPT_START_DATA, output, CRYPT_LEN_DATA );
    //расчет контрольной суммы
    Packet_ETA.crc2 = GetCRC16( pdat + CRC2_START_DATA, ldat - CRC2_START_DATA - CRC2_END_DATA );
    return (uint8_t*) &Packet_ETA;
 }
 
//**********************************************************************************
// Формирование пакета с интервальными показания
// uint32_t param1 - значение параметра 1
// uint8_t day     - день интервального значения
// uint8_t month   - месяц интервального значения
// uint8_t year    - год интервального значения
// uint8_t hour    - часы интервального значения
// uint8_t minute  - минуты (00/30) интервального значения
// uint8_t stat    - статус выполнения команды
// return          - указатель на буфер с данными пакетами
//**********************************************************************************
uint8_t *CrtPacket2( uint32_t param1, uint8_t day, uint8_t month, uint16_t year, uint8_t hour, uint8_t minute, uint8_t stat ) {

    static uint8_t *pdat, ldat;

    pdat = (uint8_t*)&Packet_ETA;
    ldat = sizeof( Packet_ETA );
    //формируем пакет
    Packet_ETA.header = PACKET_HEADER;
    Packet_ETA.tail = PACKET_TAIL;
    Packet_ETA.sender = GetNumDev();
    Packet_ETA.receiver = AP_Numb;
    Packet_ETA.status = StatReset() | stat;
    Packet_ETA.day = day;
    Packet_ETA.mon = month;
    Packet_ETA.year = year;
    Packet_ETA.hour = hour;
    Packet_ETA.minute = minute;
    Packet_ETA.seconds = 0;
    //параметра пакета
    Packet_ETA.voltage = ACVoltage();
    Packet_ETA.power1 = param1;
    Packet_ETA.power2 = 0;
    Packet_ETA.crc1 = GetCRC16( pdat + CRC1_START_DATA, CRC1_LEN_DATA );
    //сохраним данные для шифрования
    memcpy( input, pdat + CRYPT_START_DATA, CRYPT_LEN_DATA );
    //шифруем данные пакета
    if ( Encrypt( input, output, ( AP_Numb & ~MASK_ACCESS_POINT ) ) != AES_SUCCESS )
        return NULL;
    //зашифрованный блок перенесем обратно в структуру
    memcpy( pdat + CRYPT_START_DATA, output, CRYPT_LEN_DATA );
    //расчет контрольной суммы
    Packet_ETA.crc2 = GetCRC16( pdat + CRC2_START_DATA, ldat - CRC2_START_DATA - CRC2_END_DATA );
    return (uint8_t*) &Packet_ETA;
 }
 
//**********************************************************************************
// Проверка принятого пакета
// buffer - буфер с принятым пакетом
// return - ERROR_*       - пакет с ошибкой
//          CHECK_SUCCESS - пакет прошел проверку
//**********************************************************************************
uint8_t CheckPacket( uint8_t *buffer ) {

    uint16_t crc;
    uint8_t *pdat, ldat;
    uint32_t error;

    memcpy( &Packet_ATE, buffer, sizeof( Packet_ATE ) );
    //проверка: заголовок-хвост
    if ( Packet_ATE.header != PACKET_HEADER || Packet_ATE.tail != PACKET_TAIL )
        return ERROR_HEADTAIL;
    //проверка: отправитель - точка доступа
    if ( !( Packet_ATE.sender & MASK_ACCESS_POINT ) )
        return ERROR_SENDER;
    //проверка: получатель - мы или общий для всех
    if ( Packet_ATE.receiver != GetNumDev() && Packet_ATE.receiver != RECEIVER_ALL )
        return ERROR_RECIVER;
    //проверка: контрольная сумма пакета
    pdat = (uint8_t*) &Packet_ATE;
    ldat = sizeof( Packet_ATE );
    crc = GetCRC16( pdat + CRC2_START_DATA, ldat - CRC2_START_DATA - CRC2_END_DATA );
    if ( Packet_ATE.crc2 != crc )
        return ERROR_CRC_PACK;
    //расшифровка, проверка КС данных
    memcpy( input, pdat + CRC1_START_DATA, CRYPT_LEN_DATA );
    if ( Packet_ATE.receiver == RECEIVER_ALL )
        error = Decrypt( input, output, ( Packet_ATE.sender & ~MASK_ACCESS_POINT ), dtime_key );
    else error = Decrypt( input, output, ( Packet_ATE.sender & ~MASK_ACCESS_POINT ), NULL );
    if ( error != DES_SUCCESS )
        return ERROR_DECRYPT;
    //обновим данные в пакете
    memcpy( pdat + CRC1_START_DATA, output, CRYPT_LEN_DATA );
    //проверим КС данных
    crc = GetCRC16( pdat + CRC1_START_DATA, CRC1_LEN_DATA );
    if ( Packet_ATE.crc1 != crc )
        return ERROR_CRC_DATA;
    AP_Numb = Packet_ATE.sender;
    return CHECK_SUCCESS;
 }
 
//**********************************************************************************
// Возвращает адрес идентификатора процессора, физический номер уст-ва,
// Порядок от младшего к старшему или (little-endian)
// запись начинается с младшего и заканчивается старшим. 
// Этот порядок записи принят в памяти персональных компьютеров с x86-процессорами
//**********************************************************************************
uint8_t *GetKey( void ) {
 
    uint8_t *Unique_ID = (uint8_t *)ADDR_ID_DEVICE;

    return Unique_ID;
 }

//**********************************************************************************
// Размерность пакета обмена данными
// id = PACKET_SENDER
// id = PACKET_RECEIVER
//**********************************************************************************
uint8_t PacketSize( uint8_t id ) {

    if ( id == PACKET_AP_EPC )
        return sizeof( Packet_ATE );
    if ( id == PACKET_EPC_AP )
        return sizeof( Packet_ETA );
    return 0;
 }

//**********************************************************************************
// Зашифровать блок данных
// src  - адрес исходный блок данных
// dest - адрес результата шифрования
// key3 - часть ключа ширования (номер получателя)
//**********************************************************************************
static uint32_t Encrypt( uint8_t *src, uint8_t *dest, uint16_t key3 ) {

    uint8_t *pda;
    uint16_t i, dev;
    int32_t out_length;
    uint32_t error_status;
    AESECBctx_stt AESctx;

    //очистим приемник
    memset( dest, 0x00, CRYPT_LEN_DATA );
    //формируем массив с ключом
    memset( aes_key, 0x00, CRYPT_LEN_DATA );
    //формируем 1 часть ключа, ID процессора
    pda = GetKey() + 11;
    for ( i = 0; i < 12; i++, pda-- )
        aes_key[i] = *pda;
    //формируем 2 часть ключа, номер отправителя
    dev = GetNumDev(); ;
    pda = ( (uint8_t*)&dev ) + 1;
    for ( i = 12; i < 14; i++, pda-- )
        aes_key[i] = *pda;
    //формируем 3 часть ключа, номер получателя 
    dev = key3;
    pda = ((uint8_t*)&dev) + 1;
    for ( i = 14; i < 16; i++, pda-- )
        aes_key[i] = *pda;

    //DataToHexStr( (uint8_t*)"KEY: ", des_key, CRYPT_LEN_DATA );
    //DataToHexStr( (uint8_t*)"DAT: ", src, CRYPT_LEN_DATA );

    //шифрование
    AESctx.mFlags = E_SK_DEFAULT;
    AESctx.mKeySize = CRYPT_LEN_DATA;
    error_status = AES_ECB_Encrypt_Init( &AESctx, aes_key, NULL );
    if ( error_status != AES_SUCCESS )
        return error_status;
    error_status = AES_ECB_Encrypt_Append( &AESctx, src, CRYPT_LEN_DATA, dest, &out_length );
    if ( error_status != AES_SUCCESS )
        return error_status;
    error_status = AES_ECB_Encrypt_Finish( &AESctx, dest + CRYPT_LEN_DATA, &out_length ); 
    if ( error_status != AES_SUCCESS )
        return error_status;

    //DataToHexStr( (uint8_t*)"CRP: ", dest, CRYPT_LEN_DATA );
    
    //обнулим массив ключей
    memset( aes_key, 0x00, CRYPT_LEN_DATA );
    return error_status;
 }
 
//**********************************************************************************
// Расшифровать блок данных
// uint8_t *src  - адрес зашифрованного блока данных
// uint8_t *dest - адрес расшифрованного блока данных
// uint16_t key2 - часть ключа ширования (номер оправителя, без признака AP)
//**********************************************************************************
static uint32_t Decrypt( uint8_t *src, uint8_t *dest, uint16_t key2, const uint8_t *ext_key ) {

    uint8_t *pda;
    const uint8_t *pdc;
    uint16_t i, dev;
    int32_t out_length;
    uint32_t error_status;
    AESECBctx_stt AESctx;

    //очистим приемник
    memset( dest, 0x00, CRYPT_LEN_DATA );
    //формируем ключ
    memset( aes_key, 0x00, CRYPT_LEN_DATA );
    if ( ext_key == NULL ) {
        //формируем 1 часть ключа, ID процессора счетчика
        pda = GetKey() + 11;
        for ( i = 0; i < 12; i++, pda-- )
            aes_key[i] = *pda;
        //формируем 3 часть ключа, номер получателя 
        dev = GetNumDev();
        pda = ((uint8_t*)&dev) + 1;
        for ( i = 14; i < 16; i++, pda-- )
            aes_key[i] = *pda;
       }
    else {
        //формируем 1 часть на основе общего ключа
        pdc = ext_key;
        for ( i = 0; i < 12; i++, pdc++ )
            aes_key[i] = *pdc;
        //формируем 3 часть ключа, номер получателя 
        dev = RECEIVER_ALL;
        pda = ((uint8_t*)&dev) + 1;
        for ( i = 14; i < 16; i++, pda-- )
            aes_key[i] = *pda;
       }
    //формируем 2 часть ключа, номер отправителя
    dev = key2;
    pda = ( (uint8_t*)&dev ) + 1;
    for ( i = 12; i < 14; i++, pda-- )
        aes_key[i] = *pda;

    //OutHexStr( (uint8_t*)"KEY: ", aes_key, CRYPT_LEN_DATA );
    //OutHexStr( (uint8_t*)"DEC: ", src, CRYPT_LEN_DATA );

    //дешифровка
    AESctx.mFlags = E_SK_DEFAULT;
    AESctx.mKeySize = CRYPT_LEN_DATA;
    
    error_status = AES_ECB_Decrypt_Init( &AESctx, aes_key, NULL );
    if ( error_status != AES_SUCCESS )
        return error_status;
    error_status = AES_ECB_Decrypt_Append( &AESctx, src, CRYPT_LEN_DATA, dest, &out_length );
    if ( error_status != AES_SUCCESS )
        return error_status;
    error_status = AES_ECB_Decrypt_Finish( &AESctx, dest + CRYPT_LEN_DATA, &out_length );
    if ( error_status != AES_SUCCESS )
        return error_status;
    
    //DataToHexStr( (uint8_t*)"DEC: ", dest, CRYPT_LEN_DATA );

    //обнулим массив ключей
    memset( aes_key, 0x00, CRYPT_LEN_DATA );
    //DataToHexStr( (uint8_t*)"RES: ", dest, CRYPT_LEN_DATA );
    return error_status;
 }
 
//**********************************************************************************
// Возвращает логический номер уст-ва, параметр NumDevice
// Если значения номера вне диапазона 1...16383, возвращается "0"
//**********************************************************************************
uint16_t GetNumDev( void ) {
 
    uint8_t *pdev;
    uint16_t num0, num1, numdev;
    
    pdev = (uint8_t*)(ADDR_NUM_DEVICE);
    num0 = (uint16_t)*pdev;
    num1 = (uint16_t)*(pdev+2);
    if ( num0 == 0x00FF && num1 == 0x00FF )
        return 0; //регистры очищены после полного стирания кристалла
    num1 <<= 8;
    numdev = ( num1 | num0 ) & MASK_NUMB_DEV;
    if ( numdev > 0 && numdev < 16384 )
        return numdev;
    else return 0;
 }

//**********************************************************************************
// Возвращает кол-во тарифов учета
// result = 0 - один тариф
//        = 1 - два тарифа
//**********************************************************************************
uint8_t GetCountTarif( void ) {
 
    uint8_t *pdev;
    uint16_t num;
    
    //указатель на адрес уст-ва
    pdev = (uint8_t*)(ADDR_NUM_DEVICE);
    num = (uint16_t)*(pdev+2);
    if ( num & MASK_TARIF )
        return 1;
    else return 0;
 }

//**********************************************************************************
// Возвращает данные из пакета Packet_ApToEpc
// uint8_t id_data - индекс доступа к данным (ID_*)
//**********************************************************************************
uint32_t GetDataPack( uint8_t id_data ) {

    if ( id_data == ID_SENDER )
        return Packet_ATE.sender;
    if ( id_data == ID_COMMAND )
        return Packet_ATE.command;
    if ( id_data == ID_DAY )
        return Packet_ATE.day;
    if ( id_data == ID_MON )
        return Packet_ATE.mon;
    if ( id_data == ID_YEAR )
        return Packet_ATE.year;
    if ( id_data == ID_HOUR )
        return Packet_ATE.hour;
    if ( id_data == ID_MINUTE )
        return Packet_ATE.minute;
    if ( id_data == ID_SECONDS )
        return Packet_ATE.seconds;
    if ( id_data == ID_PARAM1 )
        return Packet_ATE.power1;
    if ( id_data == ID_PARAM2 )
        return Packet_ATE.power2;
    if ( id_data == ID_INTERVAL )
        return Packet_ATE.interval;
    return 0;
 }

//**********************************************************************************
// Устанавливает флаги источника сброса процессора
//**********************************************************************************
uint8_t StatReset( void ) {

    uint8_t  reset = 0;
    uint32_t res_src;
    
    res_src = RCC->CSR;
    if ( res_src & RCC_CSR_PINRSTF )
        reset |= STAT_EPC_PINRST;
    if ( res_src & RCC_CSR_PORRSTF )
        reset |= STAT_EPC_PORRST;
    if ( res_src & RCC_CSR_SFTRSTF )
        reset |= STAT_EPC_SFTRST;
    if ( res_src & RCC_CSR_IWDGRSTF )
        reset |= STAT_EPC_WDTRST;
    return reset;
 }
