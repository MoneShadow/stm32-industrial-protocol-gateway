#ifndef __APP_H
#define __APP_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Device Model */
typedef struct {
    uint16_t ID;

    uint16_t Current_RPM;
    uint16_t Target_RPM;
    uint8_t Bus_Voltage;
    uint8_t Temperature;
    uint8_t State;
    uint8_t Fault_Code;

    uint32_t Last_Heart_Time;

    uint8_t State_Num_Last;
    uint8_t State_Num_Current;
    uint8_t State_Num_Valid;
    uint32_t State_Count;
    uint16_t Lost_Count;

    uint8_t Online; // 0x00 Online 0x01 Offline
} Device_Model;

extern QueueHandle_t queue_feedback_rpm;
extern QueueHandle_t queue_node_state;
extern QueueHandle_t queue_rs485_receive;
extern volatile uint8_t tx_in_flight;
extern volatile Device_Model device_model;

void app(void);

#endif