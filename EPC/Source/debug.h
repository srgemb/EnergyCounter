
#ifndef __DEBUG_H_
#define __DEBUG_H_

#include "stm32f1xx.h"

#define IDX_COMMAND         0
#define IDX_DATE            1
#define IDX_TIME            1
#define IDX_NUM_DEV         1
#define IDX_NUM_PAGE        1
#define IDX_CALIB_CONST     1
#define IDX_REG_NUMB        1
#define IDX_ID_DEV          2
#define IDX_REG_VALUE       2
#define IDX_RELAY           3
#define IDX_TARIF           1
#define IDX_POWER           2

#define MAX_CNT_PARAM       5   //максимальное кол-во параметров, включая команду
#define MAX_LEN_PARAM       30  //максимальный размер (длинна) параметра в коммандной строке

void DbgInit( void );
void DbgWork( void );
void ScanDev( void );
void DbgRecv( void );
void DbgOut( char *str );
void GetIdCPU( void );
void GetTime( void );
uint8_t SetTime( char *param );
void GetDate( void );
uint8_t SetDate( char *param );
void GetDateTime( void );
void ClearDbgRecv( void );
void ClearDbgSend( void );
void OutHexStr( uint8_t *prefix, uint8_t *data, uint8_t len );
//void DBG_IntStr( uint8_t *prefix, uint8_t value );

#endif
