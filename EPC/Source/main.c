/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  *
  * COPYRIGHT(c) 2017 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f1xx_hal.h"
#include "crc.h"
#include "i2c.h"
#include "rtc.h"
#include "iwdg.h"
#include "tim.h"
#include "epc_spi.h"
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */

#include <stdbool.h>

#include "timers.h"
#include "debug.h"
#include "packet.h"
#include "crc16.h"
#include "hc12epc.h"
#include "epc.h"
#include "fram.h"
#include "cmnd.h"

/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
bool enb_rf = false;
    
/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void Error_Handler(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

char res[40];
uint16_t fram;

/* USER CODE END 0 */

int main( void ) {

    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration----------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* Configure the system clock */
    SystemClock_Config();

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_RTC_Init();
    MX_I2C1_Init();
    MX_TIM1_Init();
    MX_TIM2_Init();
    MX_CRC_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    #ifndef DEBUG_VERSION
        MX_IWDG_Init();
    #endif

    /* USER CODE BEGIN 2 */
    //таблица значений для расчета CRC
    MakeCRC16Table();
    //светодиод ВКЛ
    LedMode( LED_MODE_ON );
    //проверка номера уст-ва
    if ( GetNumDev() ) {
        enb_rf = true;
        //номер уст-ва определен, начинаем мигать
        LedMode( LED_MODE_SLOW );
       }

    //запуск таймеров
    HAL_TIM_Base_Start( &htim1 );
    HAL_TIM_Base_Start_IT( &htim1 );
    HAL_TIM_Base_Start( &htim2 );
    HAL_TIM_Base_Start_IT( &htim2 );

    //инициализация интерейса отладки
    DbgInit();

    //инициализация STPM01
    EPCInit();
    PwrCountInit();
    //разрешаем секундные прерывания
    HAL_RTCEx_SetSecond_IT( &hrtc );

    #ifndef DEBUG_VERSION
        //проверка памяти FRAM
        fram = FRAMCheck();
        if ( fram ) {
            sprintf( res, "FRAM ERROR PAGE: %03d\r", fram - 1 );
            DbgOut( res );
           }
        else DbgOut( "FRAM OK\r" );
    #endif

    //инициализация радио модуля
    RFInit();

    #ifndef DEBUG_VERSION
        HAL_IWDG_Start( &hiwdg ); //вкл. сторожевой таймер
    #endif

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    
    while ( 1 ) {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */

        DbgWork();          //обработка команд с консоли
        EPCDataRead();      //чтение данных STPM01
        if ( enb_rf == true ) {
            //номер уст-ва определен, разрешаем работу радио модуля
            RFCheck();      //обработка запросов по радио каналу
            Answer();       //ответ по радио каналу
            Command();      //обработка комманд радио пакета
            Relay();        //управление реле
           }
        //перезапуск сторожевого таймера
        #ifndef DEBUG_VERSION
            HAL_IWDG_Refresh( &hiwdg );
        #endif
       }
    /* USER CODE END 3 */
 }

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit;

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE
                              |RCC_OSCILLATORTYPE_LSE; 
  //RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON; 
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* USER CODE BEGIN 4 */

// Обработка внешнего прерывания от сигнала LED_PWR счетчика STPM01
void HAL_GPIO_EXTI_Callback( uint16_t GPIO_Pin ) {

    if ( GPIO_Pin == LED_PWR_Pin )
        PulseInc();
 }

// Обработка прерывания от RTC
void HAL_RTCEx_RTCEventCallback( RTC_HandleTypeDef *hrtc ) {

    if ( RTC_IT_SEC )
        EPCSaveInterval();
 }

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler */
  /* User can add his own implementation to report the HAL error return state */
  while(1) 
  {
  }
  /* USER CODE END Error_Handler */ 
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
