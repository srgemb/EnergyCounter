
#ifndef __PACKET_H
#define __PACKET_H

#include "stm32f1xx.h"

#define MASK_ACCESS_POINT   0x8000

//идентифиакторы типа пакета
#define PACKET_AP_EPC       1               //точка доступа -> счетчик
#define PACKET_EPC_AP       2               //счетчик -> точка доступа

//код результата проверки принятого пакета данных
#define CHECK_SUCCESS       0               //проверка прошла
#define ERROR_HEADTAIL      1               //ошибка в заголовке/хвосте пакета
#define ERROR_CRC_DATA      2               //ошибка в КС данных (кривая расшифровка)
#define ERROR_CRC_PACK      4               //ошибка в КС пакета (битый пакет)
#define ERROR_DECRYPT       8               //ошибка расшифровки
#define ERROR_SENDER        16              //отправитель "левый"
#define ERROR_RECIVER       32              //получатель - "не мы"

//коды комманд
#define COMMAND_READ        0x00            //чтение данных
#define COMMAND_WRITE       0x80            //запись данных
#define COMMAND_DATE_TIME   0x01            //установка часов
#define COMMAND_POWER1      0x02            //чтение/установ счетчика 1
#define COMMAND_POWER2      0x04            //чтение/установ счетчика 2
#define COMMAND_FRAM_CHK    0x08            //проверка FRAM
#define COMMAND_RELAY       0x10            //управление реле

//флаги статусов Packet_EpcToAp.status      
#define STAT_EPC_PINRST     0x01            //сброс внешним сигналом
#define STAT_EPC_PORRST     0x02            //сброс от схемы POR
#define STAT_EPC_SFTRST     0x04            //софтовый сброс
#define STAT_EPC_WDTRST     0x08            //сброс по таймеру WDT
#define STAT_EPC_CMND_OK    0x10            //команда выполнена без ошибок

#define RECEIVER_ALL        0x7FFF          //широковещательный пакет
 
//индексы для получения данных из структуры Packet_EpcToAp
#define ID_SENDER           0               //отправитель
#define ID_STATUS           1               //код команды
#define ID_DAY              2               //день
#define ID_MON              3               //месяц
#define ID_YEAR             4               //год
#define ID_HOUR             5               //часы
#define ID_MINUTE           6               //минуты
#define ID_SECONDS          7               //секунды
#define ID_PARAM1           8               //значение параметра 1
#define ID_PARAM2           9               //значение параметра 1
#define ID_VOLTAGE          10              //значение напряжения

uint8_t *CrtPacket( uint16_t recv, uint8_t *id, uint8_t command, uint32_t param1, uint32_t param2, uint8_t interval );
uint8_t CheckPacket( uint8_t *buffer );
uint8_t *GetKey( void );
uint8_t PacketSize( uint8_t id );
uint16_t GetNumDev( void );
uint8_t Hex2Int( uint8_t *ptr );
char *Int2Hex( char *dest, uint8_t size_dest, uint8_t *ptr, uint8_t size_ptr );
uint32_t GetDataPack( uint8_t id_data );

#endif
