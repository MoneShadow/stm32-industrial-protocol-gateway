#ifndef __APP_H
#define __APP_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

extern QueueHandle_t queue1;

void app(void);

#endif