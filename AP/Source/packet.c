
//**********************************************************************************
//
// Управление протоколом обмена по радио каналу
// 
//**********************************************************************************

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "hc12ap.h"
#include "host.h"
#include "crc16.h"
#include "timers.h"
#include "host.h"
#include "packet.h"
#include "xtime.h"
#include "crypto.h"

//**********************************************************************************
// Локальные константы
//**********************************************************************************
//заголовок/окончание пакета
#define PACKET_HEADER       (uint16_t) 0x5555
#define PACKET_TAIL         (uint16_t) 0xAAAA

#define ADDR_NUM_DEVICE     0x1FFFF804      //адрес Data0, Data1
#define ADDR_ID_DEVICE      0x1FFFF7E8      //адрес ID CPU

#define SIZE_ID_DEV_CHARS   24              //размер идентификатора уст-ва (ключа)

//параметры блока шифрования
#define CRYPT_START_DATA    6               //начало блока
#define CRYPT_LEN_DATA      16              //размер блока

//параметры расчета контрольной суммы 1 (внутри блока шифрования)
#define CRC1_START_DATA     6               //начало блока
#define CRC1_LEN_DATA       14              //длинна блока подсчета КС

//параметры расчета контрольной суммы 2 (всего пакета)
#define CRC2_START_DATA     2               //начало блока
#define CRC2_END_DATA       4               //исключение из окончание пакета

//**********************************************************************************
// Форматы пакетов
//**********************************************************************************
#pragma pack( push, 1 )

//структура пакета данных запроса AP -> EPC
typedef struct {
    uint16_t header;                //заголовок
    uint16_t sender;                //отправитель
    uint16_t receiver;              //получатель
    //начало блока шифрования
    uint8_t command;                //команда
    uint8_t interval;               //номер интервального значения
    unsigned day : 5;               //день
    unsigned mon : 4;               //месяц
    unsigned year : 12;             //год
    unsigned hour : 5;              //часы
    unsigned minute : 6;            //минуты
    unsigned seconds : 6;           //секунды
    unsigned power1 : 30;           //тариф 1
    unsigned power2 : 30;           //тариф 2
    uint16_t crc1;                  //контрольная сумма блока данных
    //конец блока шифрования        
    uint16_t crc2;                  //общая контрольная сумма 
    uint16_t tail;                  //окончание пакета
} Packet_ApToEpc; 

//структура пакета данных ответа EPC -> AP
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
// Внешние переменные
//**********************************************************************************
extern RTC_HandleTypeDef hrtc;
extern UART_HandleTypeDef huart1;

//**********************************************************************************
// Локальные переменные
//**********************************************************************************
static uint16_t last_recv;                //номер уст-ва к которому отправлен 
                                          //последний запрос
static char last_id[30];                  //ID уст-ва к которому отправлен 
                                          //последний запрос
static Packet_ApToEpc Packet_ATE;
static Packet_EpcToAp Packet_ETA;

static uint8_t input[CRYPT_LEN_DATA];
static uint8_t output[CRYPT_LEN_DATA];
static uint8_t aes_key[CRYPT_LEN_DATA];

//**********************************************************************************
// Прототипы локальные функций
//**********************************************************************************
static uint32_t Encrypt( uint8_t *src, uint8_t *dest, uint8_t *key1, uint16_t key3 );
static uint32_t Decrypt( uint8_t *src, uint8_t *dest );

//**********************************************************************************
// Формирование пакета запроса
// uint16_t recv      - номер получателя
// uint8_t  *id       - указатель на строку содержащую ID получателя
// uint8_t  command   - код команды
// uint32_t param1    - значение параметра №1
// uint32_t param2    - значение параметра №1
// return   uint8_t * - указатель на буфер с данными пакетами для отправки
//          NULL      - пакет не сформирован
//**********************************************************************************
uint8_t *CrtPacket( uint16_t recv, uint8_t *id, uint8_t command, uint32_t param1, uint32_t param2, uint8_t interval ) {

    timedate td;
    static uint8_t *pdat, ldat;

    if ( id == NULL || strlen( (char *)id ) != 24 )
        return NULL;
    GetTimeDate( &td );
    pdat = (uint8_t*)&Packet_ATE;
    ldat = sizeof( Packet_ATE );
    //формируем пакет
    Packet_ATE.header = PACKET_HEADER;
    Packet_ATE.tail = PACKET_TAIL;
    Packet_ATE.sender = MASK_ACCESS_POINT | GetNumDev();
    Packet_ATE.receiver = recv;
    last_recv = recv;
    Packet_ATE.command = command;
    Packet_ATE.interval = interval;
    Packet_ATE.day = td.td_day;
    Packet_ATE.mon = td.td_month;
    Packet_ATE.year = td.td_year;
    Packet_ATE.hour = td.td_hour;
    Packet_ATE.minute = td.td_min;
    Packet_ATE.seconds = td.td_sec;
    Packet_ATE.power1 = param1;
    Packet_ATE.power2 = param2;
    Packet_ATE.crc1 = GetCRC16( pdat + CRC1_START_DATA, CRC1_LEN_DATA );
    //сохраним ID уст-ва из последнего запроса
    memset( last_id, 0x00, sizeof( last_id ) );
    memcpy( last_id, (char*)id, SIZE_ID_DEV_CHARS );
    //сохраним данные для шифрования
    memcpy( input, pdat + CRYPT_START_DATA, CRYPT_LEN_DATA );
    //шифруем данные
    if ( Encrypt( input, output, id, recv ) != AES_SUCCESS )
        return NULL;
    //зашифрованный блок перенесем обратно в структуру
    memcpy( pdat + CRYPT_START_DATA, output, CRYPT_LEN_DATA );
    //расчет контрольной суммы
    Packet_ATE.crc2 = GetCRC16( pdat + CRC2_START_DATA, ldat - CRC2_START_DATA - CRC2_END_DATA );
    return (uint8_t*) &Packet_ATE;
 }
 
//**********************************************************************************
// Проверка CRC принятого пакета
// buffer - буфер с принятым пакетом
// return - ERROR-пакет с ошибкой, SUCCESS-пакет прошел проверку
//**********************************************************************************
uint8_t CheckPacket( uint8_t *buffer ) {

    static uint16_t crc;
    static uint8_t *pdat, ldat;
    static uint32_t error;

    memcpy( &Packet_ETA, buffer, sizeof( Packet_ETA ) );
    //проверка: заголовок-хвост
    if ( Packet_ETA.header != PACKET_HEADER || Packet_ETA.tail != PACKET_TAIL )
        return ERROR_HEADTAIL;
    //проверка: получатель - точка доступа отправитель запроса
    if ( !( Packet_ETA.receiver & MASK_ACCESS_POINT ) || ( Packet_ETA.receiver & ~MASK_ACCESS_POINT ) != GetNumDev() )
        return ERROR_RECIVER;
    //проверка: отправитель - из запроса, получатель - мы (AP) 
    if ( Packet_ETA.sender != last_recv )
        return ERROR_SENDER;
    //проверка: контрольная сумма пакета
    pdat = (uint8_t*) &Packet_ETA;
    ldat = sizeof( Packet_ETA );
    crc = GetCRC16( pdat + CRC2_START_DATA, ldat - CRC2_START_DATA - CRC2_END_DATA );
    if ( Packet_ETA.crc2 != crc )
        return ERROR_CRC_PACK;
    //расшифровка, проверка КС данных
    memcpy( input, pdat + CRC1_START_DATA, CRYPT_LEN_DATA );
    error = Decrypt( input, output );
    if ( error != DES_SUCCESS )
        return ERROR_DECRYPT;
    //обновим данные в пакете
    memcpy( pdat + CRC1_START_DATA, output, CRYPT_LEN_DATA );
    //проверим КС данных
    crc = GetCRC16( pdat + CRC1_START_DATA, CRC1_LEN_DATA );
    if ( Packet_ETA.crc1 != crc )
        return ERROR_CRC_DATA;
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
// src  - исходный блок данных
// dest - результат шифрования
// key1  - ключ ширования1 (ID получателя)
// key3  - ключ ширования3 (номер получателя)
//**********************************************************************************
static uint32_t Encrypt( uint8_t *src, uint8_t *dest, uint8_t *key1, uint16_t key3 ) {

    static uint8_t *pda;
    static uint16_t i, dev;
    static int32_t out_length;
    static uint32_t error_status;
    static AESECBctx_stt AESctx;

    //очистим приемник
    memset( dest, 0x00, CRYPT_LEN_DATA );
    //формируем массив с ключом
    memset( aes_key, 0x00, CRYPT_LEN_DATA );
    //формируем 1 часть ключа, сворачиваем текстовый ID в двоичный
    for ( i = 0; i < 12; i++ )
        aes_key[i] = Hex2Int( key1 + i * 2 );
    //формируем 2 часть ключа, номер отправителя
    dev = GetNumDev();
    pda = ((uint8_t*)&dev) + 1;
    for ( i = 12; i < 14; i++, pda-- )
        aes_key[i] = *pda;
    //формируем 3 часть ключа, номер получателя 
    dev = key3;
    pda = ((uint8_t*)&dev) + 1;
    for ( i = 14; i < 16; i++, pda-- )
        aes_key[i] = *pda;

    //OutHexStr( (uint8_t*)"KEY: ", aes_key, CRYPT_LEN_DATA );
    //OutHexStr( (uint8_t*)"DAT: ", src, CRYPT_LEN_DATA );

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

    //OutHexStr( (uint8_t*)"CRP: ", dest, CRYPT_LEN_DATA );
    
    //обнулим массив ключей
    memset( aes_key, 0x00, CRYPT_LEN_DATA );
    return error_status;
 }
 
//**********************************************************************************
// Расшифровать блок данных
// uint8_t *src  -  
// uint8_t *dest - 
// uint8_t *key1 - 
// uint16_t key3 - 
//**********************************************************************************
static uint32_t Decrypt( uint8_t *src, uint8_t *dest ) {

    static uint8_t *pda;
    static uint16_t i, dev;
    static int32_t out_length;
    static uint32_t error_status;
    static AESECBctx_stt AESctx;

    //очистим приемник
    memset( dest, 0x00, CRYPT_LEN_DATA );
    //формируем массив с ключом
    memset( aes_key, 0x00, CRYPT_LEN_DATA );
    //формируем 1 часть ключа, сворачиваем текстовый ID отправителя в двоичный
    for ( i = 0; i < 12; i++ )
        aes_key[i] = Hex2Int( (uint8_t*)last_id + i * 2 );
    //формируем 2 часть ключа, номер отправителя
    dev = last_recv;
    pda = ((uint8_t*)&dev) + 1;
    for ( i = 12; i < 14; i++, pda-- )
        aes_key[i] = *pda;
    //формируем 3 часть ключа, номер получателя 
    dev = GetNumDev();
    pda = ((uint8_t*)&dev) + 1;
    for ( i = 14; i < 16; i++, pda-- )
        aes_key[i] = *pda;

    //OutHexStr( (uint8_t*)"KEY: ", des_key, CRYPT_LEN_DATA );
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
    
    //OutHexStr( (uint8_t*)"DEC: ", dest, CRYPT_LEN_DATA );
    
    //обнулим массив ключей
    memset( aes_key, 0x00, CRYPT_LEN_DATA );
    return error_status;
 }
 
//**********************************************************************************
// Возвращает логический номер уст-ва, параметр NumDevice
// Если значения номера вне диапазона 1...32767, возвращается "0"
//**********************************************************************************
uint16_t GetNumDev( void ) {
 
    uint8_t *pdev;
    uint16_t num0, num1, numdev;
    
    pdev = (uint8_t*)(ADDR_NUM_DEVICE);
    num0 = (uint16_t)*pdev;
    num1 = (uint16_t)*(pdev+2);
    num1 <<= 8;
    numdev = num1 | num0;
    if ( numdev > 0 && numdev < 32768 )
        return numdev;
    else return 0;
 }

//**********************************************************************************
// 
//**********************************************************************************
uint8_t Hex2Int( uint8_t *ptr ) {

    uint8_t val1, val2;
    
    val1 = toupper( *ptr );
    val2 = toupper( *(ptr+1) );
    //старшая тетрада
    if ( val1 >= 0x30 && val1 <= 0x39 ) {
        val1 &= 0x0F;
        val1 <<= 4;
       } 
    if ( val1 >= 0x41 && val1 <= 0x46 ) {
        val1 -= 0x37;
        val1 <<= 4;
       } 
    //младшая тетрада
    if ( val2 >= 0x30 && val2 <= 0x39 )
        val2 &= 0x0F;
    if ( val2 >= 0x41 && val2 <= 0x46 )
        val2 -= 0x37;
    return val1 | val2;
 }

//**********************************************************************************
// Возвращает данные из пакета Packet_EpcToAp
// uint8_t id_data - индекс доступа к данным (ID_*)
//**********************************************************************************
uint32_t GetDataPack( uint8_t id_data ) {

    if ( id_data == ID_SENDER )
        return Packet_ETA.sender;
    if ( id_data == ID_STATUS )
        return Packet_ETA.status;
    if ( id_data == ID_DAY )
        return Packet_ETA.day;
    if ( id_data == ID_MON )
        return Packet_ETA.mon;
    if ( id_data == ID_YEAR )
        return Packet_ETA.year;
    if ( id_data == ID_HOUR )
        return Packet_ETA.hour;
    if ( id_data == ID_MINUTE )
        return Packet_ETA.minute;
    if ( id_data == ID_SECONDS )
        return Packet_ETA.seconds;
    if ( id_data == ID_VOLTAGE )
        return Packet_ETA.voltage;
    if ( id_data == ID_PARAM1 )
        return Packet_ETA.power1;
    if ( id_data == ID_PARAM2 )
        return Packet_ETA.power2;
    return 0;
 }

//**********************************************************************************
// Развернем строку из массива двоичных данных в HEX
//**********************************************************************************
char *Int2Hex( char *dest, uint8_t size_dest, uint8_t *ptr, uint8_t size_ptr ) {

    uint8_t i;
    char tmp[10];
    
    memset( dest, 0x00, size_dest );
    for ( i = 0; i < size_ptr; i++ ) {
        sprintf( tmp, "%02X", *(ptr + i) );
        strcat( dest, tmp );
       }
    return dest;
 }

