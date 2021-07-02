
//**********************************************************************************
//
// Управление программными таймерами
// 
//**********************************************************************************

#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "stm32f1xx.h"
#include "stm32f1xx_it.h"

#include "timers.h"

#define LED_ON_FAST         50
#define LED_OFF_FAST        50

#define LED_ON_SLOW         50
#define LED_OFF_SLOW        750

#define LED_ON_INV          750
#define LED_OFF_INV         50

#define TIME_ACTION         800

bool flg_act = false;
static uint8_t last_led_mode = LED_MODE_SLOW;
static uint8_t led_mode = LED_MODE_SLOW;
static uint16_t led_timer = LED_MODE_SLOW; 
static uint16_t timers[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

//**********************************************************************************
// Инкремент массима таймеров
//**********************************************************************************
void TicTimers( void ) {

    uint8_t index;
    
    for ( index = 0; index < ( sizeof( timers )/sizeof( uint16_t ) ); index++ ) 
        if ( timers[index] )
            timers[index]--;
   }

//**********************************************************************************
// Установка значений таймеров
// timer - номер таймера
// value - значение счетчика таймера
//**********************************************************************************
void SetTimers( uint8_t timer, uint16_t value ) {

    timers[timer] = value;
 }
 
//**********************************************************************************
// Чтение значение таймера
// timer - номер таймера
//**********************************************************************************
uint16_t GetTimers( uint8_t timer ) {

    return timers[timer];
 }
 
//**********************************************************************************
// Управление индикацией
// Вызов из TIM1_UP_IRQHandler stm32f1xx_it.c
//**********************************************************************************
void LedIndicator( void ) {
 
    GPIO_PinState state;

    if ( led_mode == LED_MODE_OFF ) {
        HAL_GPIO_WritePin( LED_GPIO_Port, LED_Pin, GPIO_PIN_SET );
        return;
       } 
    if ( led_mode == LED_MODE_ON ) {
        HAL_GPIO_WritePin( LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET );
        return;
       } 
    if ( led_timer-- )
        return;
    state = HAL_GPIO_ReadPin( LED_GPIO_Port, LED_Pin );

    if ( led_mode == LED_MODE_SLOW && state == GPIO_PIN_RESET )
        led_timer = LED_OFF_SLOW;
    if ( led_mode == LED_MODE_SLOW && state == GPIO_PIN_SET )
        led_timer = LED_ON_SLOW;

    if ( led_mode == LED_MODE_FAST && state == GPIO_PIN_RESET )
        led_timer = LED_OFF_FAST;
    if ( led_mode == LED_MODE_FAST && state == GPIO_PIN_SET )
        led_timer = LED_ON_FAST;

    if ( led_mode == LED_MODE_INV && state == GPIO_PIN_RESET )
        led_timer = LED_OFF_INV;
    if ( led_mode == LED_MODE_INV && state == GPIO_PIN_SET )
        led_timer = LED_ON_INV;

    HAL_GPIO_TogglePin( LED_GPIO_Port, LED_Pin );
    if ( flg_act == true && !GetTimers( TIMER_ACTION ) ) {
        flg_act = false;
        led_timer = 0;
        LedMode( last_led_mode );
       }
 }
 
//**********************************************************************************
// Установка режима индикации
//**********************************************************************************
void LedMode( uint8_t mode ) {

    led_mode = mode;
    if ( mode != LED_MODE_FAST )
        last_led_mode = mode; //сохраним режим индикации, кроме LED_MODE_FAST
 }

//**********************************************************************************
// Включить режим индикации LED_MODE_FAST на время TIME_ACTION
//**********************************************************************************
void LedAction( void ) {

    led_timer = 0;
    flg_act = true;
    LedMode( LED_MODE_FAST );
    SetTimers( TIMER_ACTION, TIME_ACTION );
 }
