
#ifndef __CMND_H
#define __CMND_H

#include "stm32f1xx.h"

#define MAX_EPC_NUM         32767

void EPC_QueryData1( uint16_t epc_num, uint8_t *epc_id );
void EPC_QueryData2( uint16_t epc_num, uint8_t interval, uint8_t *epc_id );
void EPC_Control( uint16_t epc_num, uint8_t *epc_id, uint8_t relay );
void EPC_SendData( uint16_t epc_num, uint8_t *epc_id, uint32_t param1, uint32_t param2 );
void EPC_FRAMCheck( uint16_t epc_num, uint8_t *epc_id );
void EPC_DateTime( uint16_t ap_num, uint8_t *ap_id );
void EPC_Answer( void );

#endif
