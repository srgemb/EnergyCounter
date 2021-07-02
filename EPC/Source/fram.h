
#ifndef __FRAM_H
#define __FRAM_H

#include "stm32f1xx.h"

#include "epc.h"

//номера страниц для хранения данных (1 страница = 16 байт)
#define FRAM_PAGE_RESERV    0                           //резерв
#define FRAM_PAGE_EPC       1                           //параметры калибровки STPM01
#define FRAM_PAGE_TAR1      2                           //тариф TARIFF_DAILY
#define FRAM_PAGE_TAR2      3                           //тариф TARIFF_NIGHT
#define FRAM_INTERVAL       4                           //начальная страница для хранения
                                                        //30 минутных показаний

#define FRAM_ADDR_CLR       4                           //начальная страница для очистки FRAM

#define FRAM_INTERVAL_MAX   (2*24*5)+FRAM_INTERVAL      //максимальная страница для хранения 
                                                        //интервальных показаний

#define FRAM_SIZE           8192                        //размер FRAM памяти байт
#define FRAM_PAGE           16                          //размер 1 блока (страницы)
#define FRAM_PAGES          ( FRAM_SIZE / FRAM_PAGE )   //кол-во блоков (страниц)

uint16_t FRAMCheck( void );
uint8_t SaveDate( uint8_t date, uint8_t month, uint8_t year );
uint8_t FRAMSavePwr( uint8_t id_tarif, double value );
uint8_t FRAMGetPwr( uint8_t id_tarif, double *counter );
uint8_t *FRAMReadPage( uint16_t addr );
uint8_t RestoreDate( void );
uint8_t FRAMClear( void );
uint8_t FRAMSaveEPC( EPC_CALIBRN *epc_clb );
uint8_t FRAMReadEPC( EPC_CALIBRN *epc_clb );
uint16_t FRAMSaveIntrv( double value );
uint8_t FRAMReadIntrv( uint16_t page, uint8_t *day, uint8_t *month, uint16_t *year, uint8_t *hour, uint8_t *minute, double *counter );
uint16_t PageCalc( uint16_t page );
uint16_t FRAMFindPage( void );

#endif

