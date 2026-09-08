/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.h
  * @brief   This file contains all the function prototypes for
  *          the can.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CAN_H__
#define __CAN_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern CAN_HandleTypeDef hcan;

/* USER CODE BEGIN Private defines */

/* 普通数据帧格式 */
typedef struct {
  uint16_t id;
  uint8_t  dlc;
  uint8_t  data[8];
} CAN_Frame;

/* 发送命令帧格式 */
typedef struct {
  CAN_TxHeaderTypeDef CAN_TxHeader;
  uint8_t TxHeader_flat;
  uint32_t mailbox;
  uint8_t data[8];
} CAN_Frame_Tx;

/* 接收命令帧格式 */
typedef struct {
  CAN_RxHeaderTypeDef CAN_RxHeader;
  uint8_t RxHeader_flat;
  uint8_t data[8];
} CAN_Frame_Rx;

/* 控制事件标志位 */
typedef struct {
  uint16_t Total_Event;
  uint16_t Error_Event;
  uint8_t RPM_Event;
  /* 保留 */
} Event_Flat;

extern volatile Event_Flat Event_Flats;
extern volatile CAN_Frame_Tx CAN_Heartbeat_Frame;
extern volatile CAN_Frame_Tx CAN_ACK_Frame;

/* USER CODE END Private defines */

void MX_CAN_Init(void);

/* USER CODE BEGIN Prototypes */

void CAN1_FilterBank_Init(void);
void CAN_Frame_Register(void);
void CAN1_RxDATA(uint8_t *RxDATA);
void CAN1_TxDATA(CAN_Frame_Tx TxFrame);
void CAN_ACK(uint8_t command_code, uint8_t res, uint32_t command_num);
void CAN_Heart(uint32_t heart_num);
uint8_t SaveCommandValue(uint8_t commandcode, uint8_t commandvalue[]);
uint8_t ReadCommandValue(uint8_t commandcode, uint8_t commandvalue[]);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __CAN_H__ */

