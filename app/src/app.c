#include "main.h"
#include "app.h"
#include "can.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

QueueHandle_t queue_feedback_rpm;
QueueHandle_t queue_ctrl_rpm_command;
QueueHandle_t queue_node_state;
SemaphoreHandle_t semphr_commandupdate;

void Task1(void *pvParameters) {
    while (1) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_2);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* 打印接收来的从机状态 */
void f103_various_states_transmit(void *pvParameters) {
    while (1) {
        CAN_Frame_Rx rxdata;
        uint8_t uartFrame[2 + 1 + 8];
        if (xQueueReceive(queue_node_state, &rxdata, portMAX_DELAY) == pdPASS) {
            /* UART format: ID high byte, ID low byte, DLC, DATA[8]. */
            uartFrame[0] = (uint8_t)(rxdata.CAN_RxHeader.StdId >> 8);
            uartFrame[1] = (uint8_t)(rxdata.CAN_RxHeader.StdId & 0xFFU);
            uartFrame[2] = rxdata.CAN_RxHeader.DLC;
            for (uint8_t i = 0; i < sizeof(rxdata.data); i++) {
                uartFrame[3 + i] = rxdata.data[i];
            }
            vTaskSuspendAll();
            HAL_UART_Transmit(&huart1, uartFrame, rxdata.CAN_RxHeader.DLC + 3, 1000);
            xTaskResumeAll();
        }
    }
}

/* 打印接收来的从机应答 */
void f103_feedback_transmit(void *pvParameters) {
    while (1) {
        CAN_Frame_Rx rxdata;
        uint8_t uartFrame[2 + 1 + 8];
        if (xQueueReceive(queue_feedback_rpm, &rxdata, portMAX_DELAY) == pdPASS) {
            /* UART format: ID high byte, ID low byte, DLC, DATA[8]. */
            uartFrame[0] = (uint8_t)(rxdata.CAN_RxHeader.StdId >> 8);
            uartFrame[1] = (uint8_t)(rxdata.CAN_RxHeader.StdId & 0xFFU);
            uartFrame[2] = rxdata.CAN_RxHeader.DLC;
            for (uint8_t i = 0; i < sizeof(rxdata.data); i++) {
                uartFrame[3 + i] = rxdata.data[i];
            }
            vTaskSuspendAll();
            HAL_UART_Transmit(&huart1, uartFrame, rxdata.CAN_RxHeader.DLC + 3, 1000);
            xTaskResumeAll();
        }
    }
}

/* 从机在线监控 */
volatile uint8_t F103_Status = 0;
volatile uint8_t F103_online_Status_count = 0;
void f103_state_transmit(void *pvParameters) {
    while (1) {
        if (((HAL_GetTick() - HeartTime) >= 1500) && !F103_Status) {
            F103_Status = 1;
            char Buffer[128];
            sprintf(Buffer,"F103_Offline");
            vTaskSuspendAll();
            HAL_UART_Transmit(&huart1, (uint8_t *)Buffer, strlen(Buffer), 1000);
            xTaskResumeAll();
        }
        else if (((HAL_GetTick() - HeartTime) < 1500) && F103_Status) {
            if (F103_online_Status_count < 3) {
                F103_online_Status_count++;
            }
            else if (F103_online_Status_count >= 3) {
                char Buffer[128];
                sprintf(Buffer,"F103_Online");
                vTaskSuspendAll();
                HAL_UART_Transmit(&huart1, (uint8_t *)Buffer, strlen(Buffer), 1000);
                xTaskResumeAll();
                F103_online_Status_count = 0;
                F103_Status = 0;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

/* 更新命令 保持控制命令一直处于最新状态 这个更新命令的任务优先级建议高于发送命令任务 避免出现读取到正在改写的数据 */
uint32_t commandnum = 0;
void Ctrl_Command_Update(void *pvParameters) {
    while (1) {
        CAN_Frame_Tx new_command = {0};
        new_command = register_rpm_command(1000, commandnum++);
        xQueueOverwrite(queue_ctrl_rpm_command, &new_command);
        xSemaphoreGive(semphr_commandupdate);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* 发送命令 */
volatile uint8_t tx_in_flight = 0;
void Can_Tx_Command(void *pvParameters) {
    while (1) {
        xSemaphoreTake(semphr_commandupdate, portMAX_DELAY);
        if ((HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) != 0) && !tx_in_flight && !F103_Status) {
            CAN_Frame_Tx command = {0};
            xQueueReceive(queue_ctrl_rpm_command, &command, portMAX_DELAY);
            if (command.CAN_TxHeader.StdId == 0x301) {   // 控制命令
                if (command.data[0] == 0x01) {  // 设置转速
                    if (HAL_CAN_AddTxMessage(&hcan1, &command.CAN_TxHeader, command.data, &command.mailbox) != HAL_OK) {
                        vTaskDelay(pdMS_TO_TICKS(100)); // 第一次发送失败 等待100ms后重试
                        if (!F103_Status) {
                            if (HAL_CAN_AddTxMessage(&hcan1, &command.CAN_TxHeader, command.data, &command.mailbox) != HAL_OK) {
                                tx_in_flight = 0;   // 第二次发送失败 丢弃命令
                            }
                            else {
                                tx_in_flight = 1;
                            }
                        }
                    }
                    else {
                        tx_in_flight = 1;
                        /* 需要检测仲裁失败 无ACK应答 BUS——OFF等情况 暂时先搁置 */
                    }
                }
            }
        }
    }
}

void app(void) {
    /* Create A Queue for the CAN1Rx to use */
    queue_feedback_rpm = xQueueCreate(8, sizeof(CAN_Frame_Rx));
    queue_ctrl_rpm_command = xQueueCreate(1, sizeof(CAN_Frame_Tx));
    queue_node_state = xQueueCreate(1, sizeof(CAN_Frame_Rx));
    semphr_commandupdate = xSemaphoreCreateBinary();

    if (HAL_CAN_Start(&hcan1) != HAL_OK) {
        Error_Handler();
    }

    /* Creare Tasks */
    xTaskCreate(Ctrl_Command_Update, "Ctrl_Command_Update", 128, NULL, 2, NULL);
    xTaskCreate(Can_Tx_Command,      "Can_Tx_Command",      128, NULL, 3, NULL);
    xTaskCreate(f103_feedback_transmit, "f103_feedback_transmit", 128, NULL, 1, NULL);
    xTaskCreate(f103_state_transmit,    "f103_state_transmit",    128, NULL, 1, NULL);
    xTaskCreate(f103_various_states_transmit,    "f103_various_states_transmit",    128, NULL, 1, NULL);

    /* Start the Schedular */
    vTaskStartScheduler();
}
