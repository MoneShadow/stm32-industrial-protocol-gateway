#ifndef __APP_H
#define __APP_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

extern QueueHandle_t queue_feedback_rpm;
extern QueueHandle_t queue_node_state;
extern volatile uint8_t tx_in_flight;

void app(void);

#endif