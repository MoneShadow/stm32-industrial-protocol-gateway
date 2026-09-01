#include "main.h"
#include "app.h"
#include "FreeRTOS.h"
#include "task.h"

void Task1(void *pvParameters) {
    while (1) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_2);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app(void) {
    xTaskCreate(Task1, "Task1", 128, NULL, 1, NULL);

    /* 启动调度器 */
    vTaskStartScheduler();
}