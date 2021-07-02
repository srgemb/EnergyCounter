
#ifndef __EPC_H
#define __EPC_H

#include "stm32f1xx.h"

//идентификаторы тарифов
#define TARIFF_DAILY        1       //дневной
#define TARIFF_NIGHT        2       //ночной

//идентификаторы калибровочных констант
#define CALIBRATION_CPH     1       //Компенсация фазовой ошибки
#define CALIBRATION_CHV     2       //Калибровка канала напряжения
#define CALIBRATION_CHP     3       //Калибровка первичного токового канала
#define CALIBRATION_IPK     4       //Кол-во импульсов на 1000 Вт*час

#define RATE_DAILY          1       //Дневной тариф
#define RATE_NIGHT          0       //Ночной тариф


//результат записи калибровочных констант
#define EPC_CONST_OK        0       //ОК
#define EPC_ERROR_SAVE      1       //Ошибка записи в FRAM
#define EPC_ERROR_READ      2       //Ошибка чтения из FRAM
#define EPC_ERROR_VALUE     3       //Выход за пределы допустимых значений

//калибровочные константы хранимые в FRAM
#pragma pack( push, 1 )
typedef struct {
    uint8_t     cph;                //Компенсация фазовой ошибки
    uint8_t     chv;                //Калибровка канала напряжения
    uint8_t     chp;                //Калибровка первичного токового канала
    uint8_t     chs;                //Калибровка вторичного токового канала
    uint8_t     nom;                //Номинальное напряжение для однопроводного измерителя
    uint16_t    pulse_per_kwh;      //Кол-во импульсов на 1000 Вт*час
    uint8_t     unused[7];          //Резерв
    uint16_t    crc;                //Контрольная сумма
} EPC_CALIBRN; 
#pragma pack( pop )

void EPCInit( void );
void EPCDataRead( void );
void PwrCountInit( void );
void PulseInc( void );
void EPCViewCalibr( void );
void EPCViewConf( void );
uint8_t EPCSetCalibr( uint8_t id, uint16_t value );
uint8_t GetTimeInterval( void );
double StpmVRms( void );
double StpmIRms( void );
uint16_t ACVoltage( void );
void EPCViewParam( void );
void EPCSaveInterval( void );

#endif
