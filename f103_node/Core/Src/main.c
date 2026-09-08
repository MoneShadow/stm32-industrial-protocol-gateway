/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "can.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

volatile uint8_t heartbeat_in_flight = 0;
volatile uint8_t stateframe_in_flight = 0;
state volatile state_frame = {0};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

volatile static uint16_t least_rpm = 0;
uint8_t Set_RPM(uint16_t rpm) {
  if (rpm == least_rpm) {
    return 2; // 目标转速重复，可以认为设置成功
  }
  if (0) {
    return 0; // 设置失败
  }
  least_rpm = rpm;
  HAL_GPIO_TogglePin(LED_TEST_GPIO_Port, LED_TEST_Pin);
  return 1; // 设置成功
}

uint8_t state_num = 0;
uint8_t Get_State(void) {
  if (0) {
    return 1; // 获取状态失败 以后再实现
  }
  state_frame.curentrpm = 930;
  state_frame.targetrpm = 1000;
  state_frame.voltage = 240;
  state_frame.temperature = 36;
  state_frame.state = 0x01;
  state_frame.errorcode = 0x00;
  state_frame.statenum = state_num++;
  return 0; // 获取状态成功
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CAN_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  volatile static uint32_t least_tick_heart = 0;
  volatile static uint32_t least_tick_command = 0;
  volatile static uint32_t least_tick_state = 0;
  volatile static uint32_t heart_num = 0;
  least_tick_heart =  HAL_GetTick();
  least_tick_command =  HAL_GetTick();
  least_tick_state =  HAL_GetTick();
  while (1)
  {
    if (((HAL_GetTick() - least_tick_heart) >= 500) && !heartbeat_in_flight) {
      least_tick_heart = HAL_GetTick();
      CAN_Heart(heart_num++);
    }

    /* 执行控制命令 */
    if (((HAL_GetTick() - least_tick_command) >= 10) && Event_Flats.Total_Event > 0) {
      least_tick_command = HAL_GetTick();
      uint8_t commandcode = 0, commanddata[8], commandnum = 0;
      if (Event_Flats.RPM_Event > 0) {
        Event_Flats.Total_Event--;
        Event_Flats.RPM_Event--;
        if (ReadCommandValue(0x01, commanddata)) {
          /* 命令空  */
          Event_Flats.Error_Event++;
        }
        else {
          uint16_t rpm = 0;
          uint8_t status = 0;
          commandcode = commanddata[0];
          rpm = commanddata[1] | commanddata[2] << 8; // 小端序保存
          commandnum = commanddata[3];
          status = Set_RPM(rpm);
          if (status) {
            /* 发送应答 */
            CAN_ACK(commandcode, 0x00, commandnum);
          }
          else {
            CAN_ACK(commandcode, 0x01, commandnum);
            Event_Flats.Error_Event++;
          }
        }
      }
    }

    if (((HAL_GetTick() - least_tick_state) >= 5000) && !stateframe_in_flight) {
      least_tick_state = HAL_GetTick();
      Get_State();
      CAN_State(state_frame);
    }

    if (heartbeat_in_flight == 1 && !HAL_CAN_IsTxMessagePending(&hcan, CAN_Heartbeat_Frame.mailbox)) {
      heartbeat_in_flight = 0;
    }

    if (stateframe_in_flight == 1 && !HAL_CAN_IsTxMessagePending(&hcan, CAN_State_Frame.mailbox)) {
      stateframe_in_flight = 0;
    }
    
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
