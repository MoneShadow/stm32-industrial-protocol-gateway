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
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "can.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

CAN_HandleTypeDef hcan;

/* CAN init function */
void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 6;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_6TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_5TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = ENABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = ENABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  CAN1_FilterBank_Init();
  if (HAL_CAN_Start(&hcan) != HAL_OK) {
    Error_Handler();
  }

  /* USER CODE END CAN_Init 2 */

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

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**CAN GPIO Configuration
    PA11     ------> CAN_RX
    PA12     ------> CAN_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* CAN1 interrupt Init */
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 9, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
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

    /**CAN GPIO Configuration
    PA11     ------> CAN_RX
    PA12     ------> CAN_TX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11|GPIO_PIN_12);

    /* CAN1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
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
  if (HAL_CAN_ConfigFilter(&hcan, &CAN1_FilterBank1) != HAL_OK) {
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
  if (HAL_CAN_AddTxMessage(&hcan, &CAN1_TxHeader1, TxDATA, &pTxMailboxNum) != HAL_OK) {
    Error_Handler();
  }
}

/* Receive */
CAN_RxHeaderTypeDef CAN1_RxHeader1;
void CAN1_RxDATA(uint8_t *RxDATA) {
  if (HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &CAN1_RxHeader1, RxDATA) != HAL_OK) {
    Error_Handler();
  }
}

/* USER CODE END 1 */

