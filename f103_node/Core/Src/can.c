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
  CAN_Frame_Register();
  HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
  HAL_CAN_ActivateNotification(&hcan, CAN_IT_TX_MAILBOX_EMPTY);
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
    HAL_NVIC_SetPriority(USB_HP_CAN1_TX_IRQn, 9, 0);
    HAL_NVIC_EnableIRQ(USB_HP_CAN1_TX_IRQn);
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
    HAL_NVIC_DisableIRQ(USB_HP_CAN1_TX_IRQn);
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
  CAN1_FilterBank1.FilterIdHigh = 0x301 << 5;
  CAN1_FilterBank1.FilterIdLow = 0x0000;
  CAN1_FilterBank1.FilterMaskIdHigh = 0x7FF << 5;
  CAN1_FilterBank1.FilterMaskIdLow = 0x0000;
  CAN1_FilterBank1.SlaveStartFilterBank = 14;
  CAN1_FilterBank1.FilterActivation = CAN_FILTER_ENABLE;
  if (HAL_CAN_ConfigFilter(&hcan, &CAN1_FilterBank1) != HAL_OK) {
    Error_Handler();
  }
}

/* 定义各种can帧 */
volatile CAN_Frame_Tx CAN_Heartbeat_Frame;
volatile CAN_Frame_Tx CAN_ACK_Frame;
void CAN_Frame_Register(void) {
  /* 从机心跳帧 */
  CAN_Heartbeat_Frame.CAN_TxHeader.RTR = CAN_RTR_DATA;
  CAN_Heartbeat_Frame.CAN_TxHeader.IDE = CAN_ID_STD;
  CAN_Heartbeat_Frame.CAN_TxHeader.StdId = 0x201;
  CAN_Heartbeat_Frame.CAN_TxHeader.ExtId = 0x12345201;
  CAN_Heartbeat_Frame.CAN_TxHeader.DLC = 2;

  /* ACK应答帧 具体数据不填写 */
  CAN_ACK_Frame.CAN_TxHeader.RTR = CAN_RTR_DATA;
  CAN_ACK_Frame.CAN_TxHeader.IDE = CAN_ID_STD;
  CAN_ACK_Frame.CAN_TxHeader.StdId = 0x401;
  CAN_ACK_Frame.CAN_TxHeader.ExtId = 0x12345401;
  CAN_ACK_Frame.CAN_TxHeader.DLC = 3;
}

/* Transmit */
void CAN1_TxDATA(CAN_Frame_Tx TxFrame) {
  if (TxFrame.CAN_TxHeader.StdId == 0x401) {  // 应答帧
    if (HAL_CAN_AddTxMessage(&hcan, &TxFrame.CAN_TxHeader, TxFrame.data, &TxFrame.mailbox) != HAL_OK) {
      /* 发送应答失败 这个暂时不讨论 */
    }
  }
  else if (TxFrame.CAN_TxHeader.StdId == 0x201) { // 心跳帧
    if (HAL_CAN_AddTxMessage(&hcan, &TxFrame.CAN_TxHeader, TxFrame.data, &TxFrame.mailbox) != HAL_OK) {
      /* 发送心跳失败 这个暂时不讨论 */
    }
  }
}

/* ACK Frame */
void CAN_ACK(uint8_t command_code, uint8_t res, uint32_t command_num) {
  CAN_ACK_Frame.data[0] = command_code;
  CAN_ACK_Frame.data[1] = res;
  CAN_ACK_Frame.data[2] = command_num;
  if (HAL_CAN_AddTxMessage(&hcan, (const CAN_TxHeaderTypeDef *)&CAN_ACK_Frame.CAN_TxHeader, (const uint8_t *)CAN_ACK_Frame.data, (uint32_t *)&CAN_ACK_Frame.mailbox) != HAL_OK) {
    /* 发送应答失败 这个暂时不讨论 */
  }
}

/* Heartbeat Frame */
void CAN_Heart(uint32_t heart_num) {
  CAN_Heartbeat_Frame.data[0] = 0x01;
  CAN_Heartbeat_Frame.data[1] = heart_num;
  if (HAL_CAN_AddTxMessage(&hcan, (const CAN_TxHeaderTypeDef *)&CAN_Heartbeat_Frame.CAN_TxHeader, (const uint8_t *)CAN_Heartbeat_Frame.data, (uint32_t *)&CAN_Heartbeat_Frame.mailbox) != HAL_OK) {
    
  }
  else {
    heartbeat_in_flight = 1;
  }
}

/* Receive */
CAN_RxHeaderTypeDef CAN1_RxHeader1;
void CAN1_RxDATA(uint8_t *RxDATA) {
  if (HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &CAN1_RxHeader1, RxDATA) != HAL_OK) {
    
  }
}

#define RPM_SIZE 6
uint8_t SetRPM_CommandValueData[RPM_SIZE][8];
/* 保存、读取命令数组中数据的函数 */
volatile uint8_t SetRPM_WIndex = 0;
volatile uint8_t SetRPM_RIndex = 0;
uint8_t SaveCommandValue(uint8_t commandcode, uint8_t commandvalue[]) {
  uint8_t i = 0;
  if (commandcode == 0x01) {
    if (!(((SetRPM_WIndex + 1) % RPM_SIZE) == SetRPM_RIndex)) {
      for (i = 0; i < 4; i++) {
        SetRPM_CommandValueData[SetRPM_WIndex][i] = commandvalue[i];
      }
      SetRPM_WIndex = (SetRPM_WIndex + 1) % RPM_SIZE;
      return 0; // 存入成功
    }
  }
  return 1; // 满
}

uint8_t ReadCommandValue(uint8_t commandcode, uint8_t commandvalue[]) {
  uint8_t i = 0;
  if (commandcode == 0x01) {
    if (SetRPM_RIndex != SetRPM_WIndex) {
      for (i = 0; i < 4; i++) {
        commandvalue[i] = SetRPM_CommandValueData[SetRPM_RIndex][i];
      }
      SetRPM_RIndex = (SetRPM_RIndex + 1) % RPM_SIZE;
      return 0; // 读取成功
    }
  }
  return 1; // 空
}

volatile Event_Flat Event_Flats = {0};

/* 
  FIFO0用于接收控制命令(ID: 0x301)
  当接收到来自主机的控制命令后 该中断回调负责
  先将命令接收下来 
  然后进行对命令的关键数据进行保存(保存在对应类型命令数据数组中 各个命令数据数组不混杂在一起 便于管理)
  然后挂起事件待处理标志位(总事件 + 具体分事件 比如接收到主机的设置转速命令 那么就 总事件+1 设置转速事件+1)
  完成后就退出中断 不长时间占用cpu
*/

/* FIFO0邮箱接收到数据 中断回调 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
  if (hcan->Instance == CAN1) {
    CAN_Frame_Rx command_frame;
    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &command_frame.CAN_RxHeader, command_frame.data);
    if (command_frame.CAN_RxHeader.StdId == 0x301) {  // 检查是否是控制命令 多留一步退路 避免以后FIFO0不止用于接收控制命令
      if (command_frame.data[0] == 0x01) {  // 设置转速命令
        if (SaveCommandValue(0x01, command_frame.data)) {
          Event_Flats.Error_Event++;
          return;
        }
        Event_Flats.Total_Event++;
        Event_Flats.RPM_Event++;
      }
    }
  }
}

/*
  CAN_Frame data1 = {0};
    CAN1_RxDATA(data1.data);
    data1.id = CAN1_RxHeader1.StdId;
    data1.dlc = CAN1_RxHeader1.DLC;
    if (CAN1_RxHeader1.IDE == 0) {
      if (data1.data[0] == 0x01) {  // set rpm
        uint8_t command_code = data1.data[0];
        uint8_t rpmlow = data1.data[1];
        uint8_t rpmhigh = data1.data[2];
        uint16_t rpm = (rpmhigh << 8) | rpmlow;
        if (rpm == 1000) {
          HAL_GPIO_TogglePin(LED_TEST_GPIO_Port, LED_TEST_Pin);
        }
        uint32_t command_num = data1.data[3];
        CAN1_ACK(command_code, 0x00, command_num);
      }
    }
*/

/* Txbox0 */
void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan) {
  if (hcan->Instance == CAN1) {
    
  }
}

/* Txbox1 */
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan) {
  if (hcan->Instance == CAN1) {
    
  }
}

/* Txbox2 */
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan) {
  if (hcan->Instance == CAN1) {

  }
}

/* USER CODE END 1 */

