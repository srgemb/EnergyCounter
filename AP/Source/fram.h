
#ifndef __fram_h
#define __fram_h

#include "stm32f1xx.h"

#define FRAM_SIZE           8192                        //размер памяти
#define FRAM_PAGE           16                          //размер 1 блока (страницы)
#define FRAM_PAGES          ( FRAM_SIZE / FRAM_PAGE )   //кол-во блоков (страниц)

uint16_t FRAMCheck( void );
uint8_t FRAMClear( void );
//uint8_t RestoreDate( void );
//uint8_t SaveDate( uint8_t date, uint8_t month, uint8_t year );
uint8_t *FRAMReadPage( uint16_t addr );
uint16_t FRAMAddPage( uint16_t epc_num, uint8_t *epc_id );
uint16_t FRAMGetDev( uint16_t index, uint16_t *epc_num, uint8_t **epc_id );

#endif

