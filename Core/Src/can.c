/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.c
  * @brief   This file provides code for the configuration
  *          of the CAN instances.
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
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "app.h"
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "can.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

CAN_HandleTypeDef hcan1;

/* CAN1 init function */
void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 7;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_2TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_6TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_5TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = ENABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = ENABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  CAN1_FilterBank_Init();
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
  if (HAL_CAN_Start(&hcan1) != HAL_OK) {
    Error_Handler();
  }

  /* USER CODE END CAN1_Init 2 */

}

void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspInit 0 */

  /* USER CODE END CAN1_MspInit 0 */
    /* CAN1 clock enable */
    __HAL_RCC_CAN1_CLK_ENABLE();

    __HAL_RCC_GPIOD_CLK_ENABLE();
    /**CAN1 GPIO Configuration
    PD0     ------> CAN1_RX
    PD1     ------> CAN1_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_CAN1;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* CAN1 interrupt Init */
    HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 9, 0);
    HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
  /* USER CODE BEGIN CAN1_MspInit 1 */

  /* USER CODE END CAN1_MspInit 1 */
  }
}

void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{

  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspDeInit 0 */

  /* USER CODE END CAN1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CAN1_CLK_DISABLE();

    /**CAN1 GPIO Configuration
    PD0     ------> CAN1_RX
    PD1     ------> CAN1_TX
    */
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_0|GPIO_PIN_1);

    /* CAN1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(CAN1_RX0_IRQn);
  /* USER CODE BEGIN CAN1_MspDeInit 1 */

  /* USER CODE END CAN1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* filter configuration */
void CAN1_FilterBank_Init(void) {
  CAN_FilterTypeDef CAN1_FilterBank1 = {0};
  CAN1_FilterBank1.FilterBank = 0;
  CAN1_FilterBank1.FilterScale = CAN_FILTERSCALE_32BIT;
  CAN1_FilterBank1.FilterMode = CAN_FILTERMODE_IDMASK;
  CAN1_FilterBank1.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  CAN1_FilterBank1.FilterIdHigh = 0x0000;
  CAN1_FilterBank1.FilterIdLow = 0x0000;
  CAN1_FilterBank1.FilterMaskIdHigh = 0x0000;
  CAN1_FilterBank1.FilterMaskIdLow = 0x0000;
  CAN1_FilterBank1.SlaveStartFilterBank = 14;
  CAN1_FilterBank1.FilterActivation = CAN_FILTER_ENABLE;
  if (HAL_CAN_ConfigFilter(&hcan1, &CAN1_FilterBank1) != HAL_OK) {
    Error_Handler();
  }
}

/* Transmit */
CAN_TxHeaderTypeDef CAN1_TxHeader1;
void CAN1_TxDATA(uint8_t *TxDATA, uint8_t len) {
  uint32_t pTxMailboxNum;
  CAN1_TxHeader1.RTR = CAN_RTR_DATA;
  CAN1_TxHeader1.IDE = CAN_ID_STD;
  CAN1_TxHeader1.StdId = 0x123;
  CAN1_TxHeader1.ExtId = 0x12345673;
  CAN1_TxHeader1.DLC = len;
  if (HAL_CAN_AddTxMessage(&hcan1, &CAN1_TxHeader1, TxDATA, &pTxMailboxNum) != HAL_OK) {
    Error_Handler();
  }
}

/* Control Frame */
CAN_TxHeaderTypeDef Ctrl_Header;
void CAN1_Ctrl(uint8_t command_code, uint16_t rpm, uint32_t command_num) {
  uint32_t pTxMailboxNum;
  uint8_t data[8], i;
  Ctrl_Header.RTR = CAN_RTR_DATA;
  Ctrl_Header.IDE = CAN_ID_STD;
  Ctrl_Header.StdId = 0x301;
  Ctrl_Header.ExtId = 0x12345301;
  Ctrl_Header.DLC = 8;
  data[0] = command_code;                     // command_code
  data[1] = rpm & 0x00FF;                     // rpm Lowbytevalue
  data[2] = ((rpm & 0xFF00) >> 8);            // rpm Highbytevalue
  data[3] = command_num;                      // command_num
  for (i = 0; i < 4; i++) data[4 + i] = 0;    // saved bit
  if (HAL_CAN_AddTxMessage(&hcan1, &Ctrl_Header, data, &pTxMailboxNum) != HAL_OK) {
    Error_Handler();
  }
}

/* Receive */
CAN_RxHeaderTypeDef CAN1_RxHeader1;
void CAN1_RxDATA(uint8_t *RxDATA) {
  if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &CAN1_RxHeader1, RxDATA) != HAL_OK) {
    Error_Handler();
  }
}

/* Received Message Callback */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
  if (hcan->Instance == CAN1) {
    CAN_Frame data1 = {0};
    CAN1_RxDATA(data1.data);
    data1.id = CAN1_RxHeader1.StdId;
    data1.dlc = CAN1_RxHeader1.DLC;
    BaseType_t pxHigherPriority = pdFALSE;
    xQueueSendFromISR(queue1, &data1, &pxHigherPriority);
    portYIELD_FROM_ISR(pxHigherPriority);
  }
}

/* USER CODE END 1 */

