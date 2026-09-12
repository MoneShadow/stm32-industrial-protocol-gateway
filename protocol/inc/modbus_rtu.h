#ifndef __MODBUS_RTU_H
#define __MODBUS_RTU_H

typedef enum {
    MODBUS_REQUEST_OK               = 0,    // 请求基础格式正确
    MODBUS_REQUEST_TOO_SHORT        = 1,    // 长度不足
    MODBUS_REQUEST_CRC_ERROR        = 2,    // CRC错误
    MODBUS_REQUEST_NOT_FOR_US       = 3,    // 不是发给本机
    MODBUS_REQUEST_UNSUPPORTED_FUNC = 4,    // 不支持的功能码           0x01
    MODBUS_REQUEST_LENGTH_ERROR     = 5,    // 功能码对应的长度错误     0x03

    MODBUS_REQUEST_ILLEGAL_DATA_ADDRESS = 6,    // 请求地址错误             0x02
    MODBUS_REQUEST_ILLEGAL_DATA_VALUE = 7,      // 请求数据数量错误         0x03

    MODBUS_03REQUEST_OK               = 9,      // 请求基础格式正确
    MODBUS_06REQUEST_OK               = 10,     // 请求基础格式正确
} Modbus_ParseResult;

typedef enum {
    ILLEGAL_FUNCTION = 0x01,
    ILLEGAL_DATA_ADDRESS = 0x02,
    ILLEGAL_DATA_VALUE = 0x03,
} Modbus_Unusualcode;

uint16_t Modbus_CRC16(const uint8_t *data, uint16_t length);
Modbus_ParseResult Modbus_CheckRequest(const uint8_t *frame, uint16_t frame_length);
Modbus_ParseResult Modbus_Parse03Request( const uint8_t *frame);
uint16_t Modbus_Handle03(const uint8_t *request_frame, uint16_t request_length, uint8_t *response_frame, uint16_t response_capacity);
uint16_t Modbus_ErrorCode_Generate(const uint8_t *request_frame, uint16_t request_length, uint8_t *response_frame, Modbus_ParseResult state);

#endif