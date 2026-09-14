#ifndef __MODBUS_RTU_H
#define __MODBUS_RTU_H

/* 转速最大最小值 */
#define RPM_MIN 0
#define RPM_MAX 3000

typedef enum {
    MODBUS_REQUEST_OK               = 0,    // 请求基础格式正确
    MODBUS_REQUEST_TOO_SHORT        = 1,    // 长度不足
    MODBUS_REQUEST_CRC_ERROR        = 2,    // CRC错误
    MODBUS_REQUEST_NOT_FOR_US       = 3,    // 不是发给本机
    MODBUS_REQUEST_UNSUPPORTED_FUNC = 4,    // 不支持的功能码           0x01
    MODBUS_REQUEST_LENGTH_ERROR     = 5,    // 功能码对应的长度错误     0x03

    MODBUS_REQUEST_ILLEGAL_DATA_ADDRESS = 6,    // 请求地址错误             0x02
    MODBUS_REQUEST_ILLEGAL_DATA_VALUE = 7,      // 请求/写入 数据 数量/值 错误         0x03

    MODBUS_03REQUEST_OK               = 9,      // 请求基础格式正确
    MODBUS_06REQUEST_OK               = 10,     // 请求基础格式正确

    SLAVE_NOTONLINE                   = 11,     // 从机离线           0x04
} Modbus_ParseResult;

typedef enum {
    ILLEGAL_FUNCTION        = 0x01, // 非法功能码
    ILLEGAL_DATA_ADDRESS    = 0x02, // 非法地址
    ILLEGAL_DATA_VALUE      = 0x03, // 非法数据
    ILLEGAL_SLAVE_STATE     = 0x04, // 非法从机状态
} Modbus_Unusualcode;

typedef struct {
    uint16_t start_address;
    uint16_t quantity;
} Modbus_03_Request;

typedef struct {
    uint16_t register_address;
    uint16_t register_value;
} Modbus_06_Request;

uint16_t Modbus_CRC16(const uint8_t *data, uint16_t length);
Modbus_ParseResult Modbus_CheckRequest(const uint8_t *frame, uint16_t frame_length);
Modbus_ParseResult Modbus_Parse03Request(const uint8_t *frame, Modbus_03_Request *request);
Modbus_ParseResult Modbus_Parse06Request(const uint8_t *frame, Modbus_06_Request *request);
uint16_t Modbus_Handle03(Modbus_03_Request request, uint8_t *response_frame, uint16_t response_capacity);
uint16_t Modbus_Handle06(Modbus_06_Request request, uint8_t *response_frame, uint16_t response_capacity);
uint16_t Modbus_ErrorCode_Generate(const uint8_t *request_frame, uint16_t request_length, uint8_t *response_frame, Modbus_ParseResult state);

#endif