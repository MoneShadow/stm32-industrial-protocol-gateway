# STM32F407 + FreeRTOS 工业协议网关

这是一个基于 STM32F407、FreeRTOS、CAN 和 Modbus RTU 的双向工业协议网关示例。

STM32F103 用作模拟 CAN 下位设备，周期上报心跳和设备状态；STM32F407 维护统一的设备模型，并将这些状态映射为 Modbus 保持寄存器。PC 可以通过 RS485 读取设备状态，也可以写入目标转速，由 F407 转换为 CAN 控制命令发送给 F103。

## 系统架构

```text
PC（Modbus RTU Master）
        │
        │ USB-RS485 / 115200 8N1
        ▼
MAX3485 + STM32F407VGT6
Modbus RTU Slave / Gateway / Device Model / FreeRTOS
        │
        │ CAN 2.0A / 500 kbit/s
        ▼
SN65HVD230 + STM32F103C8T6
模拟 CAN 设备节点（Node ID = 1）
```

主要数据链：

```text
状态读取：
F103 状态帧 → F407 CAN RX → Device Model → Modbus 0x03 → PC

参数控制：
PC Modbus 0x06 → 最新控制命令队列 → CAN 0x301 → F103 执行
                                                ├→ ACK 0x401 → F407 匹配执行结果
                                                └→ 状态帧 0x101 → Device Model → PC 后续读取
```

Modbus `0x06` 的正常回显表示 F407 已接受写入请求并登记控制命令，不代表 F103 已经执行成功。F103 的实际执行结果由 CAN `0x401` ACK 返回，F407 使用命令类型和序号进行匹配，并在调试串口输出成功、失败或超时结果。

## 硬件与接口

### STM32F407 网关

- MCU：STM32F407VGT6，168 MHz，1 MB Flash
- CAN1：PD0（RX）、PD1（TX）
- USART1 调试口：PA9（TX）、PA10（RX），115200 8N1
- USART2 / RS485：PD5（TX）、PD6（RX），115200 8N1
- RS485 方向控制：PD4，低电平接收、高电平发送
- CAN 收发器：SN65HVD230（3.3 V）
- RS485 收发器：MAX3485（3.3 V）

### STM32F103 模拟节点

- MCU：STM32F103C8T6，72 MHz
- CAN1：PA11（RX）、PA12（TX）
- USART1 调试口：PA9（TX）、PA10（RX），115200 8N1
- CAN 收发器：SN65HVD230（3.3 V）

CAN 总线两端需要正确终端匹配，并保证两个节点共地。下载和调试使用外部 ST-Link，F407 与 F103 分别烧录各自的 ELF 文件。

## CAN 应用协议

项目使用标准 11-bit CAN ID，Node ID 固定为 1。

| CAN ID | 方向 | 用途 | DLC |
|---|---|---|---:|
| `0x101` | F103 → F407 | 设备状态 | 8 |
| `0x201` | F103 → F407 | Heartbeat | 2 |
| `0x301` | F407 → F103 | 控制命令 | 8 |
| `0x401` | F103 → F407 | 命令 ACK | 3 |

接收端同时校验标准帧、数据帧、CAN ID 和 DLC；格式不合法的业务帧会被忽略。

### 状态帧 `0x101`

多字节状态字段采用小端序。

| 字节 | 含义 |
|---|---|
| Byte 0–1 | 当前转速 RPM，低字节在前 |
| Byte 2–3 | F103 保存的目标转速，低字节在前 |
| Byte 4 | 母线电压原始值，单位 0.1 V |
| Byte 5 | 温度，当前版本只表示非负值 |
| Byte 6 bit 0–3 | 设备状态 |
| Byte 6 bit 4–7 | 故障码 |
| Byte 7 | 状态帧序号，8-bit 自然回绕 |

F407 通过状态序号跳变统计应用层未连续处理到的状态帧。该统计既可能反映 CAN 传输丢失，也可能反映高负载下状态队列被最新帧覆盖。

### Heartbeat `0x201`

| 字节 | 含义 |
|---|---|
| Byte 0 | Node ID，当前为 1 |
| Byte 1 | 心跳序号低 8 位 |

F103 默认每 500 ms 发送一次心跳。F407 启动时默认节点离线，连续观察到 3 次新心跳后确认上线；最新心跳超过 1500 ms 未更新则确认离线，并清除尚未提交的旧控制命令。

### 控制命令 `0x301`

| 字节 | 含义 |
|---|---|
| Byte 0 | 命令码，`0x01` 表示设置目标转速 |
| Byte 1–2 | 目标转速，低字节在前 |
| Byte 3 | 命令序号 |
| Byte 4–7 | 保留，当前为 0 |

目标转速合法范围为 0–3000 RPM。待发送队列长度为 1，新的设置值会覆盖尚未提交的旧值，因此该命令采用“最终目标值”语义，而不是“每次写入都必须执行”语义。

### 命令 ACK `0x401`

| 字节 | 含义 |
|---|---|
| Byte 0 | 原命令码 |
| Byte 1 | 执行结果：`0x00` 成功，`0x01` 失败 |
| Byte 2 | 原命令序号 |

F407 最多等待 1000 ms，并只接受命令码和序号均匹配的 ACK。ACK 超时不等于命令一定没有执行：控制帧可能已经执行而 ACK 丢失，因此超时的准确含义是“执行结果未知”。

## Modbus RTU

- 角色：STM32F407 为 Slave，PC 为 Master
- Slave Address：1
- 串口：115200、8 数据位、无校验、1 停止位
- CRC：CRC-16/Modbus，报文中低字节先发送
- 功能码：`0x03` Read Holding Registers、`0x06` Write Single Register

### 保持寄存器表

表中的 PDU 地址是 Modbus 报文实际携带的零基地址。

| 逻辑编号 | PDU 地址 | 访问 | 含义 |
|---|---:|---|---|
| 40001 | 0 | 只读 | 当前转速 RPM |
| 40002 | 1 | 读写 | PC 请求的目标转速，0–3000 RPM |
| 40003 | 2 | 只读 | 母线电压 V |
| 40004 | 3 | 只读 | 温度 °C |
| 40005 | 4 | 只读 | 设备状态 |
| 40006 | 5 | 只读 | 故障码 |
| 40007 | 6 | 只读 | 在线状态：0 在线，1 离线 |
| 40008 | 7 | 只读 | 状态帧丢失计数 |

CRC 错误以及非本机地址请求会被静默丢弃。当前实现支持以下异常响应：

| 异常码 | 含义 |
|---:|---|
| `0x01` | 不支持的功能码 |
| `0x02` | 非法寄存器地址 |
| `0x03` | 非法数量或写入值 |
| `0x04` | 本项目用于表示下游 CAN 节点离线 |

## FreeRTOS 设计

中断只负责取帧、进行基本格式检查并通知任务；协议解析、状态更新、日志和 Modbus 应答均在任务上下文完成。

| 任务 | 优先级 | 栈深度（word） | 职责 |
|---|---:|---:|---|
| `Can_Tx_Command` | 3 | 384 | 发送最新控制命令，匹配 ACK，处理确认超时和挂起邮箱 |
| `Modbus_RTU_Task` | 2 | 384 | 接收、校验和解析 Modbus 请求，生成响应 |
| `f103_state_monitor` | 1 | 256 | Heartbeat 超时、离线和连续心跳上线判断 |
| `f103_various_states_update` | 1 | 128 | 解析状态帧并原子更新 Device Model |
| `print_f103node_state` | 1 | 1536 | 通过 USART1 输出 Device Model 快照 |
| `Stack_Monitor` | 1 | 128 | 周期采集各业务任务的最小剩余栈空间 |

主要同步机制：

- 长度为 1 的控制队列保存最新目标值。
- 长度为 1 的状态队列使用 `xQueueOverwriteFromISR()` 保留最新状态。
- ACK 队列保存独立命令结果，队列满时累计诊断计数。
- 二进制信号量用于通知控制命令和状态更新。
- USART1 Mutex 防止多个任务的日志相互穿插，不再通过暂停调度保护打印。
- Device Model 写入和快照复制使用短临界区，避免读取更新到一半的数据。

工程启用了 FreeRTOS 内存分配失败检测和栈溢出检测。一次完整业务测试中的最小剩余栈空间如下，单位为 word（STM32F407 上为 4 字节）：

| 任务 | 最小剩余 |
|---|---:|
| CAN 命令发送 | 212 |
| 节点监控 | 126 |
| 状态更新 | 75 |
| 状态打印 | 1378 |
| Modbus RTU | 241 |

## 异常处理与恢复

### CAN 控制发送

- CAN 暂时忙时，发送任务每 10 ms 重新检查，不提前取走待发送命令。
- `HAL_CAN_AddTxMessage()` 立即失败时，等待 100 ms 后最多重试一次。
- 控制帧提交成功后，F407 等待匹配的应用 ACK。
- ACK 超时且对应邮箱仍挂起时，F407 请求取消该邮箱。
- 发送完成、中止和发送错误路径都会恢复 `tx_in_flight` 状态。
- 节点离线时清空尚未提交的控制命令，重连后不会自动执行旧目标。

### RS485

- USART2 RX 使用循环 DMA + UART IDLE 事件。
- 收到一轮数据后，Modbus 任务停止 RX DMA，并复制稳定快照。
- 发送前将 MAX3485 DE 置高，阻塞发送完成后置低并重新启动接收。
- USART2 的收发只由 Modbus 任务控制，避免多个任务竞争 RS485 方向。

## 已完成测试

- F407 CAN 内部回环收发。
- F103 与 F407 双节点 CAN 正常通信。
- F103 上报心跳、状态，F407 更新 Device Model。
- Modbus `0x03` 完整读取和部分读取。
- Modbus `0x06` 合法写入及 CAN 控制闭环。
- CRC 错误、非本机地址、非法功能码、非法寄存器地址和非法数值。
- 停止 Heartbeat 后离线，恢复后连续 3 次心跳上线。
- 停止应用 ACK 后的确认超时。
- CAN 断线时挂起邮箱取消、发送错误回调和重连恢复。
- CAN 忙碌时保留最新目标，离线时清除旧命令。
- 高频状态帧下的队列覆盖和 Sequence 丢失统计。
- 10 ms 连续 Modbus 写入压力下 ACK 队列未发生溢出。
- USART1 日志与 Modbus 请求并发运行。
- FreeRTOS 任务栈余量检查。

## 构建

依赖：

- CMake 3.22 或更高版本
- Ninja
- GNU Arm Embedded Toolchain（`arm-none-eabi-gcc`）

构建 F407 网关：

```bash
cmake --preset Debug
cmake --build --preset Debug
```

输出文件：`build/Debug/stm32-industrial-protocol-gateway.elf`

构建 F103 模拟节点：

```bash
cd f103_node
cmake --preset Debug
cmake --build --preset Debug
```

输出文件：`f103_node/build/Debug/f103_node.elf`

也可以将 `Debug` 替换为 `Release`。使用 ST-Link 和 STM32CubeProgrammer、OpenOCD 或兼容调试环境，将两个 ELF 分别烧录到对应 MCU。

## 目录结构

```text
Core/                 STM32F407 启动、HAL 外设和中断代码
app/                  Device Model、FreeRTOS 任务和网关业务
bsp/                  RS485 板级驱动
protocol/             Modbus RTU 校验、解析和响应
FreeRTOS/              FreeRTOS Kernel
Drivers/               STM32F4 HAL 与 CMSIS
f103_node/             STM32F103 模拟 CAN 节点独立工程
cmake/                 F407 工具链与 CubeMX CMake 配置
```

## 已知限制

- 当前只支持一个 CAN 节点和 Modbus Slave Address 1。
- Modbus 仅实现 `0x03` 和 `0x06`。
- USART2 当前使用 UART IDLE 作为帧边界，没有使用独立定时器严格实现 Modbus RTU 3.5 字符时间。
- Modbus `0x06` 先返回请求回显，F103 的异步执行结果仅通过 USART1 日志诊断，尚未映射为 Modbus 状态寄存器。
- 尚未实现完整的 CAN Bus-Off 状态机、所有队列/外设错误统计和独立看门狗恢复策略。
- F103 是软件模拟节点：当前转速、温度和电压主要使用固定测试值，没有连接真实电机。
- USART1 日志仍采用轮询发送；状态打印任务的栈空间为调试阶段的保守配置。
- 部分 FreeRTOS 对象创建返回值尚未统一检查。

## 关键调试记录

- FreeRTOS 队列传递 CAN 帧时，队列元素大小必须与完整帧结构一致，不能只按单字节创建。
- CAN 发送邮箱编号由 HAL 返回，不能假设固定使用邮箱 0、1 或 2。
- CAN 硬件发送完成与 F103 应用层执行 ACK 是两个不同阶段，必须分别跟踪。
- 请求取消 CAN 邮箱后，HAL 可能进入 Abort Callback，也可能因 `TERR` 进入 Error Callback；两个路径都需要恢复软件发送状态。
- `tx_in_flight` 必须在提交 CAN 帧前置位，提交失败再回滚，避免完成中断先清零后被任务重新置位。
- F103 心跳和状态帧需要分别记录各自使用的发送邮箱，避免完成状态相互干扰。
- USART1 多任务日志应使用 Mutex，而不是暂停整个调度器。
- Modbus 报文中的 16-bit 字段为高字节在前，CRC 为低字节在前；串口工具的十六进制显示方式不影响线上实际字节。
- PC 请求目标值与 F103 状态帧上报的实际目标值具有不同语义，应在 Device Model 中分别保存。

## 第三方组件

项目包含 STMicroelectronics HAL、CMSIS 和 FreeRTOS Kernel。ST 提供的组件许可证文件保留在对应 `Drivers` 目录中；使用或再分发时请遵循各组件附带的许可证条款。
