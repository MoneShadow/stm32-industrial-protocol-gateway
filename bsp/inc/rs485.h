#ifndef __RS485_H
#define __RS485_H

extern uint8_t dma_buffer[];

HAL_StatusTypeDef RS485_SendBlocking(const uint8_t *data, uint16_t length, uint32_t timeout);
HAL_StatusTypeDef RS485_ReceiveBlocking(void);

#endif