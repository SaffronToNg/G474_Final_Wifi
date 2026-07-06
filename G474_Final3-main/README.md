# G474_Final3-main

这是 `D:\FInal_Wifi76` 总仓库中的一个 STM32G474 子工程。

- 工程入口（Keil）：[`MDK-ARM/G474_NewStart.uvprojx`](MDK-ARM/G474_NewStart.uvprojx)
- 配置入口（CubeMX）：[`G474_NewStart.ioc`](G474_NewStart.ioc)

首次进入总仓库时，建议先阅读：[../README.md](../README.md)。

---

## 这个子工程在总仓库中的定位

当前已知它与 `Final1 / Final2` 共享相同工程骨架，并保留了 Buck / Boost、状态机、TFT、串口与校准链相关内容。

如果你的使用目标是：

- 对比多个阶段代码
- 查看另一套参数 / 显示策略 / 调整结果
- 在不离开总仓库的前提下比较相近工程版本

那么这个目录是有价值的。

但当前仓库并没有给出“Final3 官方身份说明”，所以文档不会臆造它一定是“最终版”或“最新版”。

---

## 工具链与入口

### Keil 工程入口
- [`MDK-ARM/G474_NewStart.uvprojx`](MDK-ARM/G474_NewStart.uvprojx)

### CubeMX 配置入口
- [`G474_NewStart.ioc`](G474_NewStart.ioc)

### 已知平台信息
从 `uvprojx` / `ioc` 可以确认：

- MCU：`STM32G474CBTx`
- 工具链：`STM32CubeMX + Keil MDK-ARM`
- 工程依赖外设：ADC、DMA、HRTIM、TIM2、TIM3、USART3 等

---

## 硬件前提

请把这个工程视作**板级专用工程**而不是通用示例。使用前至少确认：

- MCU 为 STM32G474CBTx
- 板上存在 HSE 外部晶振
- 板上存在 Buck / Boost 功率级与对应采样链
- 有 TFT / LCD 显示链
- 有 KEY2 按键
- USART3 可以接入串口工具

如果你没有对应硬件，建议把它作为“代码结构和实现参考”来阅读，而不是直接烧录目标板。

---

## 上手步骤

1. 用 Keil 打开 [`MDK-ARM/G474_NewStart.uvprojx`](MDK-ARM/G474_NewStart.uvprojx)
2. 检查工程所需 Pack 与编译器环境
3. 如需查看引脚和时钟，用 CubeMX 打开 [`G474_NewStart.ioc`](G474_NewStart.ioc)
4. 编译下载到匹配板卡
5. 上电后观察 TFT、LED、状态机状态变化
6. 使用 `KEY2` 触发输出使能
7. 通过串口观察状态帧、发送 `V####` 命令测试交互

---

## 运行逻辑简述

当前可从代码确认的关键行为包括：

- 系统有 Buck / Boost 两种模式
- 状态机经历 `WAIT -> RISE -> RUN`
- `KEY2` 参与输出使能
- USART3 周期发送状态帧
- USART3 接收 `V####` 设置 VTAR
- TFT 显示 `vin / vout / ii / io / vtar`

建议优先查看：

- [`Core/Src/main.c`](Core/Src/main.c)
- [`Core/Src/minimal_state_machine.c`](Core/Src/minimal_state_machine.c)
- [`Core/Src/tft_debug_view.c`](Core/Src/tft_debug_view.c)

---

## 串口协议简述

### 串口参数
代表性工程中，USART3 使用：

- 波特率：`115200`
- 数据位：`8`
- 停止位：`1`
- 校验：`None`
- 引脚：`PB10 TX` / `PB11 RX`

### 状态帧
系统周期发送：

- `0xAA` 帧头
- `mode / vin / vout / ii / io / vtar` 六个 16-bit 字段
- XOR 校验

### 设置命令
支持：

```text
V####
```

例如：

```text
V2500
```

用于根据目标显示值设置 `vref_target`。

---

## 重要提醒：显示值与物理量之间存在“显示链”

和总仓库另外两个工程一样，这里的 TFT / 串口数值也不能默认等同于真实物理测量值。它们可能经过：

- ADC 换算
- 滤波
- 偏置修正
- 增益补偿
- 显示判定逻辑

所以：

> **如果你要做示波器 / 万用表对照，请先理解显示链，而不是直接拿屏幕值判断物理真实性。**

---

## 本目录参考文档

当前目录已有：

- [F334_CURRENT_LOGIC.html](F334_CURRENT_LOGIC.html)
- [INPUT_SOURCE_CC_ANALYSIS.html](INPUT_SOURCE_CC_ANALYSIS.html)
- [OFFSET_EXPLANATION.html](OFFSET_EXPLANATION.html)
- [TFT_SAMPLING_DISPLAY_JUDGMENT.html](TFT_SAMPLING_DISPLAY_JUDGMENT.html)
- [STM32F334_TO_G474_MIGRATION_PLAN.md](STM32F334_TO_G474_MIGRATION_PLAN.md)

这些文档可以帮助你理解：

- 电流逻辑来源
- 输入源 / 控制背景
- 偏置与显示语义
- F334 → G474 的迁移背景

---

## 推荐使用方式

如果你只是想快速跑工程：

1. 看本 README
2. 打开 `uvprojx`
3. 编译下载
4. 观察 TFT / 串口 / KEY2 行为

如果你想深入分析当前工程为何这样显示 / 这样控制：

1. 看本 README
2. 看 [`Core/Src/main.c`](Core/Src/main.c)
3. 看 [`Core/Src/minimal_state_machine.c`](Core/Src/minimal_state_machine.c)
4. 看 [TFT_SAMPLING_DISPLAY_JUDGMENT.html](TFT_SAMPLING_DISPLAY_JUDGMENT.html)
5. 看 [OFFSET_EXPLANATION.html](OFFSET_EXPLANATION.html)

---

## 给开源使用者的最后建议

这个目录对外部使用者最大的难点，不是“能不能打开工程”，而是：

- 你是否清楚自己现在打开的是哪个版本
- 你是否有匹配的硬件
- 你是否知道屏幕值不一定就是物理值

如果这三件事没有先想清楚，你后面越往下改，越容易把“代码问题”和“工程使用边界问题”混在一起。