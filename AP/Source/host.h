
#ifndef __HOST_H
#define __HOST_H

#include "stm32f1xx.h"

#define BUFFER_HOST         100  //размер приемного/передающего буфера

#define IDX_COMMAND         0
#define IDX_DATE            1
#define IDX_TIME            1
#define IDX_NUM_DEV         1
#define IDX_ID_DEV          2
#define IDX_NUM_INTERVAL    2
#define IDX_NUM_PAGE        1
#define IDX_RELAY           3
#define IDX_POWER1          3
#define IDX_POWER2          4

#define MAX_CNT_PARAM       5   //максимальное кол-во параметров, включая команду
#define MAX_LEN_PARAM       30  //максимальный размер (длинна) параметра в коммандной строке

void HostInit( void );
void HostWork( void );
void HostRecv( void );
void HostOut( char *str );
void GetIdCPU( void );
void ScanDev( void );

void OutHexStr( uint8_t *prefix, uint8_t *data, uint8_t len );

#endif
