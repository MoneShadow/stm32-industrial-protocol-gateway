#include "main.h"
#include "usart.h"
#include "gpio.h"
#include "app.h"

/* RS485_DE_Pin 低电平接收 高电平发送 */
#define RS485_SET_TxMod() HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_SET)
#define RS485_SET_RxMod() HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_RESET)

HAL_StatusTypeDef RS485_SendBlocking(const uint8_t *data, uint16_t length, uint32_t timeout) {
    HAL_StatusTypeDef Status = 0;
    RS485_SET_TxMod();  // 先设置发送模式
    Status = HAL_UART_Transmit(&huart2, data, length, timeout);
    RS485_SET_RxMod();  // 发送完成后恢复接收模式
    return Status;
}

volatile uint16_t old_pos = 0;
uint8_t dma_buffer[128];
HAL_StatusTypeDef RS485_ReceiveBlocking(void) {
    old_pos = 0;
    HAL_StatusTypeDef Status = 0;
    RS485_SET_RxMod();  // 先设置接收模式(默认接收模式 这里做双重保障 下面同理)
    Status = HAL_UARTEx_ReceiveToIdle_DMA(&huart2, dma_buffer, 128);
    RS485_SET_RxMod();  // 发送完成后恢复接收模式
    return Status;
}

uint16_t rs485rx_pos_size[2];
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (huart->Instance == USART2) {
        HAL_UART_RxEventTypeTypeDef event;
        uint16_t datasize = 0;
        event = HAL_UARTEx_GetRxEventType(huart);
        if (event == HAL_UART_RXEVENT_HT) {         // DMA Half Transfer
            
        }
        else if (event == HAL_UART_RXEVENT_TC) {    // DMA Transfer Complete
            
        }
        else if (event == HAL_UART_RXEVENT_IDLE) {  // UART IDLE
            if (Size >= old_pos) {   // 没有发生回绕
                datasize = Size - old_pos;
            }
            else {                  // 发生回绕
                datasize = huart2.RxXferSize - old_pos + Size;
            }
            old_pos = Size;
            rs485rx_pos_size[0] = old_pos;
            rs485rx_pos_size[1] = datasize;
            BaseType_t pxHigherPriority = pdFALSE;
            xQueueSendFromISR(queue_rs485_receive, rs485rx_pos_size, &pxHigherPriority);
            portYIELD_FROM_ISR(pxHigherPriority);
        }
    }
}