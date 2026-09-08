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
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO1_MSG_PENDING);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_TX_MAILBOX_EMPTY);

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
    HAL_NVIC_SetPriority(CAN1_TX_IRQn, 9, 0);
    HAL_NVIC_EnableIRQ(CAN1_TX_IRQn);
    HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 9, 0);
    HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
    HAL_NVIC_SetPriority(CAN1_RX1_IRQn, 9, 0);
    HAL_NVIC_EnableIRQ(CAN1_RX1_IRQn);
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
    HAL_NVIC_DisableIRQ(CAN1_TX_IRQn);
    HAL_NVIC_DisableIRQ(CAN1_RX0_IRQn);
    HAL_NVIC_DisableIRQ(CAN1_RX1_IRQn);
  /* USER CODE BEGIN CAN1_MspDeInit 1 */

  /* USER CODE END CAN1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* filter configuration */
void CAN1_FilterBank_Init(void) {
  CAN_FilterTypeDef CAN1_FilterBank1 = {0};
  CAN1_FilterBank1.FilterBank = 0;
  CAN1_FilterBank1.FilterScale = CAN_FILTERSCALE_16BIT;
  CAN1_FilterBank1.FilterMode = CAN_FILTERMODE_IDMASK;
  CAN1_FilterBank1.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  CAN1_FilterBank1.FilterIdHigh = 0x401 << 5;
  CAN1_FilterBank1.FilterIdLow = 0x000 << 5;
  CAN1_FilterBank1.FilterMaskIdHigh = 0x7FF << 5;
  CAN1_FilterBank1.FilterMaskIdLow = 0x7FF << 5;
  CAN1_FilterBank1.SlaveStartFilterBank = 14;
  CAN1_FilterBank1.FilterActivation = CAN_FILTER_ENABLE;
  if (HAL_CAN_ConfigFilter(&hcan1, &CAN1_FilterBank1) != HAL_OK) {
    Error_Handler();
  }

  CAN_FilterTypeDef CAN1_FilterBank2 = {0};
  CAN1_FilterBank2.FilterBank = 1;
  CAN1_FilterBank2.FilterScale = CAN_FILTERSCALE_16BIT;
  CAN1_FilterBank2.FilterMode = CAN_FILTERMODE_IDMASK;
  CAN1_FilterBank2.FilterFIFOAssignment = CAN_FILTER_FIFO1;
  CAN1_FilterBank2.FilterIdHigh = 0x201 << 5;
  CAN1_FilterBank2.FilterIdLow = 0x101 << 5;
  CAN1_FilterBank2.FilterMaskIdHigh = 0x7FF << 5;
  CAN1_FilterBank2.FilterMaskIdLow = 0x7FF << 5;
  CAN1_FilterBank2.SlaveStartFilterBank = 14;
  CAN1_FilterBank2.FilterActivation = CAN_FILTER_ENABLE;
  if (HAL_CAN_ConfigFilter(&hcan1, &CAN1_FilterBank2) != HAL_OK) {
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

/* 注册一个控制转速的命令帧 */
CAN_Frame_Tx register_rpm_command(uint16_t rpm, uint32_t command_num) {
  CAN_Frame_Tx rpm_command = {0};
  rpm_command.CAN_TxHeader.RTR = CAN_RTR_DATA;
  rpm_command.CAN_TxHeader.IDE = CAN_ID_STD;
  rpm_command.CAN_TxHeader.StdId = 0x301;
  rpm_command.CAN_TxHeader.ExtId = 0x12345301;
  rpm_command.CAN_TxHeader.DLC = 8;
  rpm_command.data[0] = 0x01;                                     // rpm command_code
  rpm_command.data[1] = rpm & 0x00FF;                             // rpm Lowbytevalue
  rpm_command.data[2] = ((rpm & 0xFF00) >> 8);                    // rpm Highbytevalue
  rpm_command.data[3] = command_num;                              // command_num
  for (uint8_t i = 0; i < 4; i++) rpm_command.data[4 + i] = 0;    // saved bit
  return rpm_command;
}

/* Receive FIFO0接收反馈 */
void CAN1_RxDATA_FIFO0(void) {
  CAN_Frame_Rx rx_frame;
  if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &rx_frame.CAN_RxHeader, rx_frame.data) != HAL_OK) {
    
  }
  if (rx_frame.CAN_RxHeader.IDE == 0) {
    if (rx_frame.CAN_RxHeader.StdId == 0x401) {  // ACK Frame
      BaseType_t pxHigherPriority = pdFALSE;
      xQueueSendFromISR(queue_feedback_rpm, &rx_frame, &pxHigherPriority);
      portYIELD_FROM_ISR(pxHigherPriority);
    }
  }
}

/* FIFO1接收状态 */
volatile uint32_t HeartTime = 0;
void CAN1_RxDATA_FIFO1(void) {
  CAN_Frame_Rx rx_frame;
  if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO1, &rx_frame.CAN_RxHeader, rx_frame.data) != HAL_OK) {
    
  }
  if (rx_frame.CAN_RxHeader.IDE == 0) {
    if (rx_frame.CAN_RxHeader.StdId == 0x201) {  // Heart Frame
      HeartTime = HAL_GetTick();
    }
    else if (rx_frame.CAN_RxHeader.StdId == 0x101) {
      BaseType_t pxHigherPriority = pdFALSE;
      xQueueSendFromISR(queue_node_state, &rx_frame, &pxHigherPriority);
      portYIELD_FROM_ISR(pxHigherPriority);
    }
  }
}

/* FIFO0 Received Message Callback */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
  if (hcan->Instance == CAN1) {
    CAN1_RxDATA_FIFO0();
  }
}

/* FIFO1 Received Message Callback */
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan) {
  if (hcan->Instance == CAN1) {
    CAN1_RxDATA_FIFO1();
  }
}

/* Txbox0 */
void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan) {
  if (hcan->Instance == CAN1) {
    tx_in_flight = 0;
  }
}

/* Txbox1 */
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan) {
  if (hcan->Instance == CAN1) {
    tx_in_flight = 0;
  }
}

/* Txbox2 */
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan) {
  if (hcan->Instance == CAN1) {
    tx_in_flight = 0;
  }
}

/* USER CODE END 1 */

