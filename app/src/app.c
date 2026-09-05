#include "main.h"
#include "app.h"
#include "can.h"
#include "usart.h"

QueueHandle_t queue1;

void Task1(void *pvParameters) {
    while (1) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_2);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void CAN_RxDataTransmit(void *pvParameters) {
    while (1) {
        CAN_Frame RxData;
        uint8_t uartFrame[2 + 1 + 8];

        if (xQueueReceive(queue1, &RxData, portMAX_DELAY) == pdPASS) {
            /* UART format: ID high byte, ID low byte, DLC, DATA[8]. */
            uartFrame[0] = (uint8_t)(RxData.id >> 8);
            uartFrame[1] = (uint8_t)(RxData.id & 0xFFU);
            uartFrame[2] = RxData.dlc;

            for (uint8_t i = 0; i < sizeof(RxData.data); i++) {
                uartFrame[3 + i] = RxData.data[i];
            }

            HAL_UART_Transmit(&huart1, uartFrame, RxData.dlc + 3, 1000);
        }
    }
}

void app(void) {
    /* Create A Queue for the CAN1Rx to use */
    queue1 = xQueueCreate(8, sizeof(CAN_Frame));

    uint8_t TxData[8] = {1, 2, 3, 4, 5 ,6 ,7, 8};
    CAN1_TxDATA(TxData, sizeof(TxData));

    /* Creare Tasks */
    xTaskCreate(Task1, "Task1", 128, NULL, 1, NULL);
    xTaskCreate(CAN_RxDataTransmit, "CAN_RxDataTransmit", 128, NULL, 1, NULL);

    /* Start the Schedular */
    vTaskStartScheduler();
}
