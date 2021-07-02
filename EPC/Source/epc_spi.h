
#ifndef __EPC_SPI_H
#define __EPC_SPI_H

#include "stm32f1xx.h"

void EPC_Clk( void );
void EPC_Reset( void );
void EPC_ReadData( void );
void EPC_ClrListReg( void );
void EPC_AddReg( uint8_t reg, uint8_t bits, uint8_t data );
void EPC_SaveReg( void );

#endif

