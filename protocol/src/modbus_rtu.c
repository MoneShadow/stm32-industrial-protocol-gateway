#include "main.h"
#include "modbus_rtu.h"
#include "app.h"

uint16_t Modbus_CRC16(const uint8_t *data, uint16_t length) {
    /* CRC16/Modbus 的初始值固定为 0xFFFF。 */
    uint16_t crc = 0xFFFFU;

    /* 将报文中参与校验的字节依次加入 CRC，不包含报文末尾原有的 CRC 字节。 */
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        /* CRC16/Modbus 按最低位优先的方式处理当前字节的 8 个数据位。 */
        for (uint8_t bit = 0; bit < 8U; bit++) {
            if ((crc & 0x0001U) != 0U) {
                crc = (crc >> 1U) ^ 0xA001U;
            }
            else {
                crc >>= 1U;
            }
        }
    }
    /* 组装 Modbus RTU 报文时，应先发送返回值的低字节，再发送高字节。 */
    return crc;
}

Modbus_ParseResult Modbus_CheckRequest(const uint8_t *frame, uint16_t frame_length) {
    if (frame_length < 4) {
        return MODBUS_REQUEST_TOO_SHORT;        // 报文长度不足
    }
    uint16_t received_crc = frame[frame_length - 1] << 8 | frame[frame_length - 2];
    uint16_t calculated_crc = Modbus_CRC16(frame, frame_length - 2);
    if (received_crc != calculated_crc) {
        return MODBUS_REQUEST_CRC_ERROR;        // CRC校验错误
    }
    if (frame[0] != 0x01) {
        return MODBUS_REQUEST_NOT_FOR_US;       // 不是发给本机
    }
    if (frame[1] != 0x03 && frame[1] != 0x06) {
        return MODBUS_REQUEST_UNSUPPORTED_FUNC; // 不支持的功能码
    }
    if (frame_length != 8) {
        return MODBUS_REQUEST_LENGTH_ERROR;     // 功能码对应的长度错误
    }
    return MODBUS_REQUEST_OK;                   // 请求格式正确
}

Modbus_ParseResult Modbus_Parse03Request(const uint8_t *frame) {
    uint16_t start_address = frame[2] << 8 | frame[3];
    uint16_t quantity = frame[4] << 8 | frame[5];
    if (quantity == 0 || quantity > 125) {                      // 请求寄存器数量超过modbus协议或为0
        return MODBUS_REQUEST_ILLEGAL_DATA_VALUE;
    }
    if (start_address >= 8 || quantity > 8 - start_address) {   // 起始地址超过了设备中最后一位寄存器地址 或 数量超过了从起始地址开始到最后一位寄存器的数量
        return MODBUS_REQUEST_ILLEGAL_DATA_ADDRESS;
    }
    return MODBUS_03REQUEST_OK;
}

uint16_t Modbus_Handle03(const uint8_t *request_frame, uint16_t request_length, uint8_t *response_frame, uint16_t response_capacity) {
    /* 进入这个函数，默认本次modbus帧数据正常 */
    uint16_t start_address = request_frame[2] << 8 | request_frame[3];
    uint16_t quantity = request_frame[4] << 8 | request_frame[5];
    /* 一次性取得Device Model快照 */
    Device_Model snapshot;
    taskENTER_CRITICAL();
    snapshot = device_model;
    taskEXIT_CRITICAL();
    /* 将Device Model转换为连续的Modbus寄存器表 数组下标就是Modbus报文中的寄存器地址 */
    uint16_t holding_registers[8] = {
        snapshot.Current_RPM,    // 地址0：40001
        snapshot.Target_RPM,     // 地址1：40002
        snapshot.Bus_Voltage,    // 地址2：40003
        snapshot.Temperature,    // 地址3：40004
        snapshot.State,          // 地址4：40005
        snapshot.Fault_Code,     // 地址5：40006
        snapshot.Online,         // 地址6：40007
        snapshot.Lost_Count      // 地址7：40008
    };
    uint16_t required_length = 5 + quantity * 2;
    if (response_capacity < required_length) {
        return 0;
    }
    uint16_t index = 0;
    /* Modbus响应头 */
    response_frame[index++] = 0x01;  // 从机地址
    response_frame[index++] = 0x03;  // 功能码
    response_frame[index++] = (uint8_t)(quantity * 2);  // 数据字节数 一个保持寄存器是uint16类型 也就是占用两个字节
    /* 按起始地址和数量读取寄存器 */
    for (uint16_t i = 0; i < quantity; i++) {
        uint16_t register_address = start_address + i;
        uint16_t value = holding_registers[register_address];
        /* Modbus寄存器数据：高字节在前 */
        response_frame[index++] = (uint8_t)((value >> 8) & 0xFF);
        response_frame[index++] = (uint8_t)(value & 0xFF);
    }
    /* 计算CRC */
    uint16_t crc = Modbus_CRC16(response_frame, index);
    /* CRC：低字节在前 */
    response_frame[index++] = (uint8_t)(crc & 0xFF);
    response_frame[index++] = (uint8_t)((crc >> 8) & 0xFF);
    return index;
}

uint16_t Modbus_ErrorCode_Generate(const uint8_t *request_frame, uint16_t request_length, uint8_t *response_frame, Modbus_ParseResult state) {
    uint16_t index = 0;
    if (request_length > 1) {
        if (request_frame[1] == 0x03) {
        index = 0;
        response_frame[index++] = request_frame[0];
        response_frame[index++] = request_frame[1] | 0x80;
        switch (state) {
            case MODBUS_REQUEST_LENGTH_ERROR:
                response_frame[index++] = ILLEGAL_DATA_VALUE;
                break;
            case MODBUS_REQUEST_ILLEGAL_DATA_ADDRESS:
                response_frame[index++] = ILLEGAL_DATA_ADDRESS;
                break;
            case MODBUS_REQUEST_ILLEGAL_DATA_VALUE:
                response_frame[index++] = ILLEGAL_DATA_VALUE;
                break;
            default:
                index = 0;
                break;
        }
        }
        else if (request_frame[1] == 0x06) {
            index = 0;
            /* 稍后处理 */
        }
        else if (state == MODBUS_REQUEST_UNSUPPORTED_FUNC){
            index = 0;
            response_frame[index++] = request_frame[0];
            response_frame[index++] = request_frame[1] | 0x80;
            response_frame[index++] = ILLEGAL_FUNCTION;
        }
        else {
            index = 0;
        }
        if (index) {
            uint16_t crc = Modbus_CRC16(response_frame, index);
            response_frame[index++] = (uint8_t)(crc & 0xFF);
            response_frame[index++] = (uint8_t)((crc >> 8) & 0xFF);
        }
    }
    return index;
}