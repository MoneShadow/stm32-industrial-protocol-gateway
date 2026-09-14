#include "main.h"
#include "app.h"
#include "can.h"
#include "usart.h"
#include "rs485.h"
#include "modbus_rtu.h"
#include <string.h>
#include <stdio.h>

QueueHandle_t queue_feedback_rpm;
QueueHandle_t queue_ctrl_rpm_command;
QueueHandle_t queue_node_state;
QueueHandle_t queue_rs485_receive;
SemaphoreHandle_t semphr_commandupdate;
SemaphoreHandle_t semphr_f103nodestateupdate;
SemaphoreHandle_t semphrmutex_uart1;

volatile Device_Model device_model = {0};

void Task1(void *pvParameters) {
    while (1) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_2);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* 更新接收来的从机状态 */
void f103_various_states_update(void *pvParameters) {
    while (1) {
        CAN_Frame_Rx rxdata;
        if (xQueueReceive(queue_node_state, &rxdata, portMAX_DELAY) == pdPASS) {
            taskENTER_CRITICAL();
            device_model.ID = rxdata.CAN_RxHeader.StdId;
            device_model.Current_RPM = (rxdata.data[1] << 8) | rxdata.data[0];
            device_model.Target_RPM  = (rxdata.data[3] << 8) | rxdata.data[2];
            device_model.Bus_Voltage  = rxdata.data[4] / 10;
            device_model.Temperature  = rxdata.data[5];
            device_model.State  = rxdata.data[6] & 0xF;
            device_model.Fault_Code  = rxdata.data[6] >> 4;

            /* 判断是否丢失/重复状态帧判断 */
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
            taskEXIT_CRITICAL();
            xSemaphoreGive(semphr_f103nodestateupdate);
        }
    }
}

/* 打印状态 */
void print_f103node_state(void *pvParameters) {
    while (1) {
        xSemaphoreTake(semphr_f103nodestateupdate, portMAX_DELAY);
        Device_Model snapshot = {0};
        taskENTER_CRITICAL();
        snapshot = device_model;
        taskEXIT_CRITICAL();
        xSemaphoreTake(semphrmutex_uart1, portMAX_DELAY);
        u1_prinf("ID: %x\r\n", snapshot.ID);
        u1_prinf("Current RPM: %u\r\n", snapshot.Current_RPM);
        u1_prinf("Target RPM: %u\r\n", snapshot.Target_RPM);
        u1_prinf("Bus Voltage: %u V\r\n", snapshot.Bus_Voltage);
        u1_prinf("Temperature: %u C\r\n", snapshot.Temperature);
        u1_prinf("State: %u\r\n", snapshot.State);
        u1_prinf("Fault Code: %u\r\n", snapshot.Fault_Code);
        u1_prinf("State Num: %u\r\n", snapshot.State_Num_Current);
        u1_prinf("State Count: %lu\r\n", snapshot.State_Count);
        u1_prinf("Lost Count: %u\r\n", snapshot.Lost_Count);
        u1_prinf("Online State: %u\r\n", snapshot.Online);
        xSemaphoreGive(semphrmutex_uart1);
    }
}

/* 从机在线监控 */
volatile uint8_t F103_Status = 1;
volatile uint8_t F103_online_Status_count = 0;
void f103_state_monitor(void *pvParameters) {
    uint32_t last_hearttime = 0, current_hearttime = 0;
    while (1) {
        current_hearttime = HeartTime;
        if (((HAL_GetTick() - current_hearttime) >= 1500)) {
            F103_online_Status_count = 0;
            if (!F103_Status) {
                F103_Status = 1;
                device_model.Online = 0x01;
                device_model.State_Num_Valid = 0; // 掉线后重新计算状态帧序号
                xSemaphoreTake(semphrmutex_uart1, portMAX_DELAY);
                u1_prinf("Offline\r\n");
                xSemaphoreGive(semphrmutex_uart1);
            }
        }
        else if (((HAL_GetTick() - current_hearttime) < 1500) && F103_Status && current_hearttime != last_hearttime) {
            if (last_hearttime != 0 && (current_hearttime - last_hearttime) >= 1500U) {
                F103_online_Status_count = 0;
            }
            if (F103_online_Status_count < 3) {
                F103_online_Status_count++;
            }
            if (F103_online_Status_count >= 3) {
                device_model.Online = 0x00;
                F103_online_Status_count = 0;
                F103_Status = 0;
                xSemaphoreTake(semphrmutex_uart1, portMAX_DELAY);
                u1_prinf("Online\r\n");
                xSemaphoreGive(semphrmutex_uart1);
            }
        }
        last_hearttime = current_hearttime;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* 发送命令 */
volatile uint8_t tx_in_flight = 0;
void Can_Tx_Command(void *pvParameters) {
    while (1) {
        xSemaphoreTake(semphr_commandupdate, portMAX_DELAY);
        /* 当前不具备提交下一条控制帧的条件 */
        while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0 || tx_in_flight != 0) {
            if (F103_Status) {  // 如果从机离线 退出CAN忙碌检查 因为即使检查到CAN空闲 也无法发送控制命令
                break;
            }
            /* 延时后重新检查 */
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (F103_Status) {  // 如果从机离线 清空控制命令队列 防止从机一上线就执行旧命令
            xQueueReset(queue_ctrl_rpm_command);
            continue;
        }
        if ((HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) != 0) && !tx_in_flight && !F103_Status) {
            CAN_Frame_Tx command = {0};
            if (xQueueReceive(queue_ctrl_rpm_command, &command, 0) != pdPASS) {
                continue;   // 这里改为不阻塞取命令 如果没有命令则跳出当次循环 重新等待更新信号
            }
            if (command.CAN_TxHeader.StdId == 0x301) {   // 控制命令
                if (command.data[0] == 0x01) {  // 设置转速
                    tx_in_flight = 1;
                    if (HAL_CAN_AddTxMessage(&hcan1, &command.CAN_TxHeader, command.data, &command.mailbox) != HAL_OK) {
                        tx_in_flight = 0;
                        vTaskDelay(pdMS_TO_TICKS(100)); // 第一次发送失败 等待100ms后重试
                        if (!F103_Status) {
                            tx_in_flight = 1;
                            if (HAL_CAN_AddTxMessage(&hcan1, &command.CAN_TxHeader, command.data, &command.mailbox) != HAL_OK) {
                                tx_in_flight = 0;   // 第二次发送失败 丢弃命令
                                continue;           // 命令发送失败 没有必要再等待ACK了 命令都没有发到从机 从机怎么可能产生反馈应答
                            }
                        }
                        else {
                            continue; // 第二次发送等待期间从机掉线 直接退出 不应进入ACK超时判断 命令都没有发到从机 从机怎么可能产生反馈应答
                        }
                    }
                    /* 无论是第一次命令发送成功还是第二次命令发送成功 都会来到这个进行ACK等待超时判断 */
                    CAN_Frame_Rx rxdata;
                    TickType_t start = xTaskGetTickCount();
                    TickType_t budget = pdMS_TO_TICKS(1000);
                    uint8_t TimeOut = 0;
                    while (1) {
                        TickType_t elapsed = xTaskGetTickCount() - start;
                        if (elapsed >= budget) {
                            /* ACK等待超时 */
                            TimeOut = 1;
                            break;
                        }
                        else {
                            if (xQueueReceive(queue_feedback_rpm, &rxdata, budget - elapsed) != pdPASS) {
                                /* ACK等待超时 */
                                TimeOut = 1;
                                break;
                            }
                        }
                        /* 不是当前命令的合法 ACK，继续等 */
                        if (rxdata.CAN_RxHeader.StdId != 0x401 || rxdata.CAN_RxHeader.DLC != 3 || rxdata.data[0] != command.data[0] || rxdata.data[2] != command.data[3]) {
                            continue;
                        }
                        /* 协议只定义了 0：成功、1：失败 其他的ACK值是异常值 */
                        if (rxdata.data[1] != 0 && rxdata.data[1] != 1) {
                            continue;
                        }
                        xSemaphoreTake(semphrmutex_uart1, portMAX_DELAY);
                        u1_prinf("CommandNum: %u, RuquestRPM: %u, Outcome: %s\r\n",
                            rxdata.data[2],
                            (unsigned int)(command.data[1] | ((uint16_t)command.data[2] << 8)),
                            rxdata.data[1] == 0? "PASS" : "FAIL");
                        xSemaphoreGive(semphrmutex_uart1);
                        break;
                    }
                    if (TimeOut) {
                        /* ACK等待超时 停止等待并打印等待超时 */
                        TimeOut = 0;
                        if (HAL_CAN_IsTxMessagePending(&hcan1, command.mailbox)) {
                            if (HAL_CAN_AbortTxRequest(&hcan1, command.mailbox) != HAL_OK) {
                                /* 记录错误 */
                                xSemaphoreTake(semphrmutex_uart1, portMAX_DELAY);
                                u1_prinf("CommandNum: %u Cancel Fail\r\n", command.data[3]);
                                xSemaphoreGive(semphrmutex_uart1);
                            }
                        }
                        xSemaphoreTake(semphrmutex_uart1, portMAX_DELAY);
                        u1_prinf("CommandNum: %u TimeOut\r\n", command.data[3]);
                        xSemaphoreGive(semphrmutex_uart1);
                    }
                }
            }
        }
    }
}

/* Modbus解析 应答 */
void Modbus_RTU_Task(void *pvParameters) {
    uint16_t pos_size[2];
    uint8_t rx_buffer[128];
    uint8_t tx_buffer[128];
    Modbus_03_Request request03 = {0};
    Modbus_06_Request request06 = {0};
    RS485_ReceiveBlocking();
    while (1) {
        if (xQueueReceive(queue_rs485_receive, pos_size, portMAX_DELAY) != pdPASS) {
            continue;
        }
        /* 先停止DMA，保证复制期间dma_buffer不再变化 */
        if (HAL_UART_AbortReceive(&huart2) != HAL_OK) {
            /* 记录接收中止失败 本轮不能贸然发送 */
            RS485_ReceiveBlocking();
            continue;
        }
        uint16_t rx_length = pos_size[1];
        for (uint16_t i = 0; i < rx_length; i++) {
            rx_buffer[i] = dma_buffer[((pos_size[0] + 128U - rx_length) + i) % 128U];
        }
        uint16_t tx_length = 0;
        Modbus_ParseResult state = Modbus_CheckRequest(rx_buffer, rx_length);
        if (state == MODBUS_REQUEST_OK) {
            if (rx_buffer[1] == 0x03) {
                state = Modbus_Parse03Request(rx_buffer, &request03);
            }
            else if (rx_buffer[1] == 0x06) {
                state = Modbus_Parse06Request(rx_buffer, &request06);
            }
            else {
                tx_length = 0;
            }

            if (state != MODBUS_03REQUEST_OK && state != MODBUS_06REQUEST_OK) {
                tx_length = Modbus_ErrorCode_Generate(rx_buffer,rx_length, tx_buffer, state);
            }
            else {
                switch (state) {
                case MODBUS_03REQUEST_OK:
                    tx_length = Modbus_Handle03(request03, tx_buffer, sizeof(tx_buffer));
                    break;
                case MODBUS_06REQUEST_OK:
                    tx_length = Modbus_Handle06(request06, tx_buffer, sizeof(tx_buffer));
                    break;
                default:
                    tx_length = 0;
                    break;
                }
            }
        }
        else {
            tx_length = Modbus_ErrorCode_Generate(rx_buffer, rx_length,tx_buffer, state);
        }
        /* tx_length为0表示静默丢弃或暂时没有响应 */
        if (tx_length > 0U) {
            if (RS485_SendBlocking(tx_buffer, tx_length, HAL_MAX_DELAY) != HAL_OK) {
                /* 记录发送错误 */
            }
        }
        /* 无论是否产生响应，最后都回到接收状态 */
        if (RS485_ReceiveBlocking() != HAL_OK) {
            /* 记录重新启动接收失败 */
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
    semphrmutex_uart1 = xSemaphoreCreateMutex();

    if (HAL_CAN_Start(&hcan1) != HAL_OK) {
        Error_Handler();
    }

    device_model.Online = 0x01; // 上电先默认从机离线

    /* Creare Tasks */
    xTaskCreate(Can_Tx_Command,      "Can_Tx_Command",      128 * 3, NULL, 3, NULL);

    xTaskCreate(f103_state_monitor,         "f103_state_monitor",         256,      NULL, 1, NULL);
    xTaskCreate(f103_various_states_update, "f103_various_states_update", 128,      NULL, 1, NULL);
    xTaskCreate(print_f103node_state,       "print_f103node_state",       128 * 12, NULL, 1, NULL);

    xTaskCreate(Modbus_RTU_Task, "Modbus_RTU_Task", 128 * 3, NULL, 2, NULL);

    /* Start the Schedular */
    vTaskStartScheduler();
}
