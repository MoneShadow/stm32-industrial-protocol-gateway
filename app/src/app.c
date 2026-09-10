#include "main.h"
#include "app.h"
#include "can.h"
#include "usart.h"
#include "rs485.h"
#include <string.h>
#include <stdio.h>

QueueHandle_t queue_feedback_rpm;
QueueHandle_t queue_ctrl_rpm_command;
QueueHandle_t queue_node_state;
QueueHandle_t queue_rs485_receive;
SemaphoreHandle_t semphr_commandupdate;
SemaphoreHandle_t semphr_f103nodestateupdate;

volatile Device_Model device_model = {0};

void Task1(void *pvParameters) {
    while (1) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_2);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void RS485_TestTask(void *pvParameters) {
    while (1) {
        uint8_t data[5] = {0x00, 0x01, 0x02, 0x03, 0x04};
        vTaskSuspendAll();
        RS485_SendBlocking(data, 5, HAL_MAX_DELAY);
        xTaskResumeAll();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void RS485_TestTask2(void *pvParameters) {
    while (1) {
        uint16_t pos_size[2];
        uint8_t buffer[128];
        xQueueReceive(queue_rs485_receive, pos_size, portMAX_DELAY);
        for (uint8_t i = 0; i < pos_size[1]; i++) {
            buffer[i] = dma_buffer[((pos_size[0] + 128U - pos_size[1]) + i) % 128];
        }
        vTaskSuspendAll();
        for (uint16_t i = 0; i < pos_size[1]; i++) {
            u1_prinf("%02X ", buffer[i]);
        }
        u1_prinf("\r\n");
        xTaskResumeAll();
    }
}

void RS485_TestTask3(void *pvParameters) {
    while (1) {
        uint16_t pos_size[2];
        uint8_t buffer[128];
        xQueueReceive(queue_rs485_receive, pos_size, portMAX_DELAY);
        for (uint8_t i = 0; i < pos_size[1]; i++) {
            buffer[i] = dma_buffer[((pos_size[0] + 128U - pos_size[1]) + i) % 128];
        }
        HAL_UART_AbortReceive(&huart2);
        vTaskSuspendAll();
        RS485_SendBlocking(buffer, pos_size[1], HAL_MAX_DELAY);
        xTaskResumeAll();
        RS485_ReceiveBlocking();
    }
}

/* 更新接收来的从机状态 */
void f103_various_states_update(void *pvParameters) {
    while (1) {
        CAN_Frame_Rx rxdata;
        if (xQueueReceive(queue_node_state, &rxdata, portMAX_DELAY) == pdPASS) {
            device_model.ID = rxdata.CAN_RxHeader.StdId;
            device_model.Current_RPM = (rxdata.data[1] << 8) | rxdata.data[0];
            device_model.Target_RPM  = (rxdata.data[3] << 8) | rxdata.data[2];
            device_model.Bus_Voltage  = rxdata.data[4] / 10;
            device_model.Temperature  = rxdata.data[5];
            device_model.State  = rxdata.data[6] & 0xF;
            device_model.Fault_Code  = rxdata.data[6] >> 4;

            /* 36~55 是判断是否丢失/重复状态帧判断 */
            uint8_t current_num = rxdata.data[7];
            device_model.State_Num_Current = current_num;
            device_model.State_Count++;
            if (device_model.State_Num_Valid == 0) {    // 这一块在初始化的时候是0 表示当前只有一帧有效状态帧 无法作为判断依据 后续只在f103掉线后重新置0
                device_model.State_Num_Last = current_num;
                device_model.State_Num_Valid = 1;
            }
            else {
                uint8_t delta = (uint8_t)(current_num - device_model.State_Num_Last);
                if (delta == 0) {
                    // 重复帧 保留 暂时不做逻辑处理
                }
                else {
                    if (delta > 1) {    // 出现丢失状态帧的情况
                        device_model.Lost_Count += (uint16_t)(delta - 1);   // 丢失数量等于序号变化量-1
                    }
                    device_model.State_Num_Last = current_num;              // 更新上次的状态帧序号
                }
            }

            xSemaphoreGive(semphr_f103nodestateupdate);
        }
    }
}

/* 打印状态 */
void print_f103node_state(void *pvParameters) {
    while (1) {
        xSemaphoreTake(semphr_f103nodestateupdate, portMAX_DELAY);
        vTaskSuspendAll();
        u1_prinf("ID: %x\r\n", device_model.ID);
        u1_prinf("Current RPM: %u\r\n", device_model.Current_RPM);
        u1_prinf("Target RPM: %u\r\n", device_model.Target_RPM);
        u1_prinf("Bus Voltage: %u V\r\n", device_model.Bus_Voltage);
        u1_prinf("Temperature: %u C\r\n", device_model.Temperature);
        u1_prinf("State: %u\r\n", device_model.State);
        u1_prinf("Fault Code: %u\r\n", device_model.Fault_Code);
        u1_prinf("State Num: %u\r\n", device_model.State_Num_Current);
        u1_prinf("State Count: %lu\r\n", device_model.State_Count);
        u1_prinf("Lost Count: %u\r\n", device_model.Lost_Count);
        u1_prinf("Online State: %u\r\n", device_model.Online);
        xTaskResumeAll();
    }
}

/* 打印接收来的从机应答 */
void f103_feedback_transmit(void *pvParameters) {
    while (1) {
        CAN_Frame_Rx rxdata;
        if (xQueueReceive(queue_feedback_rpm, &rxdata, portMAX_DELAY) == pdPASS) {
            vTaskSuspendAll();
            u1_prinf("ID: %x\r\n", rxdata.CAN_RxHeader.StdId);
            u1_prinf("DLC: %u\r\n", rxdata.CAN_RxHeader.DLC);
            u1_prinf("commandcode: %u\r\n", rxdata.data[0]);
            u1_prinf("ack state: %u\r\n", rxdata.data[1]);
            u1_prinf("ack num: %u\r\n", rxdata.data[2]);
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
            vTaskSuspendAll();
            u1_prinf("Offline\r\n");
            xTaskResumeAll();
            F103_Status = 1;
            device_model.Online = 0x01;
            device_model.State_Num_Valid = 0; // 掉线后重新计算状态帧序号
        }
        else if (((HAL_GetTick() - HeartTime) < 1500) && F103_Status) {
            if (F103_online_Status_count < 3) {
                F103_online_Status_count++;
            }
            else if (F103_online_Status_count >= 3) {
                vTaskSuspendAll();
                u1_prinf("Online\r\n");
                xTaskResumeAll();
                device_model.Online = 0x00;
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
    queue_rs485_receive = xQueueCreate(8, sizeof(uint16_t) * 2);
    semphr_commandupdate = xSemaphoreCreateBinary();
    semphr_f103nodestateupdate = xSemaphoreCreateBinary();

    if (HAL_CAN_Start(&hcan1) != HAL_OK) {
        Error_Handler();
    }
    RS485_ReceiveBlocking();

    /* Creare Tasks */
    xTaskCreate(Ctrl_Command_Update, "Ctrl_Command_Update", 128, NULL, 2, NULL);
    xTaskCreate(Can_Tx_Command,      "Can_Tx_Command",      128, NULL, 3, NULL);
    xTaskCreate(f103_feedback_transmit, "f103_feedback_transmit", 768, NULL, 1, NULL);
    xTaskCreate(f103_state_transmit,    "f103_state_transmit",    256, NULL, 1, NULL);
    xTaskCreate(f103_various_states_update, "f103_various_states_update", 128, NULL, 1, NULL);
    xTaskCreate(print_f103node_state, "print_f103node_state", 128 * 12, NULL, 1, NULL);
    
    // xTaskCreate(RS485_TestTask, "RS485_TestTask", 128, NULL, 4, NULL);
    // xTaskCreate(RS485_TestTask2, "RS485_TestTask2", 128 * 4, NULL, 4, NULL);
    xTaskCreate(RS485_TestTask3, "RS485_TestTask3", 128 * 4, NULL, 4, NULL);

    /* Start the Schedular */
    vTaskStartScheduler();
}
