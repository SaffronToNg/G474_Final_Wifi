# G474_Final1-main

这是 `D:\FInal_Wifi76` 总仓库中的一个 STM32G474 子工程。

- 工程入口（Keil）：[`MDK-ARM/G474_NewStart.uvprojx`](MDK-ARM/G474_NewStart.uvprojx)
- 配置入口（CubeMX）：[`G474_NewStart.ioc`](G474_NewStart.ioc)

如果你是第一次接触整个仓库，建议先回到顶层阅读：[../README.md](../README.md)。

---

## 这个子工程在总仓库中的定位

当前已知它与 `G474_Final2-main`、`G474_Final3-main` 共享相同的总体工程骨架，包括：

- STM32G474CBTx
- CubeMX + Keil 工程组织
- ADC / DMA / HRTIM / TIM2 / TIM3 / USART3
- Buck / Boost 相关状态机与显示链

但仓库中没有自带对 `Final1 / Final2 / Final3` 三者角色的正式说明，因此：

> 当前不建议主观把它定义成“最终版”或“实验版”。若你只是想选一个代表性入口开始阅读，这个工程可以作为合适起点。

---

## 工具链与入口

### Keil 工程入口
- [`MDK-ARM/G474_NewStart.uvprojx`](MDK-ARM/G474_NewStart.uvprojx)

该文件表明工程主要面向 **Keil MDK-ARM**。

### CubeMX 配置入口
- [`G474_NewStart.ioc`](G474_NewStart.ioc)

如果你要查看引脚、时钟、ADC/HRTIM/USART 等外设配置，请用 **STM32CubeMX** 打开该文件。

### 已知 MCU / Pack 信息
从 `uvprojx` 和 `ioc` 可确认：

- MCU：`STM32G474CBTx`
- 核心：`Cortex-M4`
- 工具链：`STM32CubeMX + Keil MDK-ARM`

---

## 硬件前提

这个工程带有明显板级依赖，不建议理解成“通用裸机模板”。下载前请确认硬件至少满足：

- MCU：**STM32G474CBTx**
- 外部时钟：工程当前使用 **HSE 外部晶振**
- 显示：带 **TFT / LCD** 驱动链
- 输入：存在 **KEY2** 按键
- 串口：**USART3**
- 功率级相关：Buck / Boost 功率板、采样链、ADC 输入与 HRTIM 输出通路已接好

### 代表性板级线索
当前代码中可直接看到：

- `KEY2`：`PB14`
- `USART3_TX`：`PB10`
- `USART3_RX`：`PB11`
- TFT / LCD 软件驱动：见 [`Core/Inc/LCDDriver.h`](Core/Inc/LCDDriver.h)

---

## 上手步骤

1. 用 Keil 打开 [`MDK-ARM/G474_NewStart.uvprojx`](MDK-ARM/G474_NewStart.uvprojx)
2. 检查 STM32G4 Device Pack 是否安装齐全
3. 编译工程
4. 下载到目标板
5. 连接串口（USART3）
6. 上电后观察 TFT、LED、状态机行为
7. 使用 `KEY2` 触发输出使能
8. 必要时通过串口发送 `V####` 测试 VTAR 设置

---

## 运行逻辑简述

当前工程包含以下已知运行机制：

- 系统存在 **Buck / Boost** 两种模式
- 状态机会经历：`WAIT -> RISE -> RUN`
- `KEY2` 参与输出使能
- `TIM2 / TIM3` 驱动采样处理、状态机与周期行为
- `USART3` 会发送状态帧，也会接收命令调整 VTAR
- `TFT` 会显示 `vin / vout / ii / io / vtar` 等最终显示值

如需进一步阅读：

- [`Core/Src/main.c`](Core/Src/main.c)
- [`Core/Src/minimal_state_machine.c`](Core/Src/minimal_state_machine.c)
- [`Core/Src/tft_debug_view.c`](Core/Src/tft_debug_view.c)

---

## 串口协议简述

### 串口参数
当前 `USART3` 配置为：

- 波特率：`115200`
- 数据位：`8`
- 停止位：`1`
- 校验：`None`
- 引脚：`PB10 TX` / `PB11 RX`

参考文件：
- [`Core/Src/usart.c`](Core/Src/usart.c)

### 周期发送状态帧
系统会通过 USART3 周期发送一帧状态数据：

- 帧头：`0xAA`
- 字段：`mode / vin / vout / ii / io / vtar`
- 每个字段为 16-bit
- 末尾有 XOR 校验

### 接收命令
支持固定格式：

```text
V####
```

例如：

```text
V2500
```

它会把上位机发来的目标显示值转换到内部 `vref_target`。

> 注意：这些值是“最终显示链”的数值，不应直接假定为未处理的真实物理量。

---

## 重要提醒：显示值不等于原始物理量

这个工程里，TFT 和串口上报的 `vin / vout / ii / io / vtar` 可能经过：

- 采样换算
- 滤波
- 偏置修正
- 增益补偿
- 显示链重映射

所以：

> **不要直接把屏幕值或串口值当作示波器 / 万用表上的原始实测量。**

如果你正在做校准、偏置或显示判定相关工作，建议先看下方参考文档。

---

## 本目录参考文档

当前目录中已经存在以下文档：

- [F334_CURRENT_LOGIC.html](F334_CURRENT_LOGIC.html)
- [INPUT_SOURCE_CC_ANALYSIS.html](INPUT_SOURCE_CC_ANALYSIS.html)
- [OFFSET_EXPLANATION.html](OFFSET_EXPLANATION.html)
- [TFT_SAMPLING_DISPLAY_JUDGMENT.html](TFT_SAMPLING_DISPLAY_JUDGMENT.html)
- [STM32F334_TO_G474_MIGRATION_PLAN.md](STM32F334_TO_G474_MIGRATION_PLAN.md)

建议阅读顺序：

1. 先看本 README
2. 若想理解显示值语义，优先看 [TFT_SAMPLING_DISPLAY_JUDGMENT.html](TFT_SAMPLING_DISPLAY_JUDGMENT.html)
3. 若想理解偏置与误差来源，看 [OFFSET_EXPLANATION.html](OFFSET_EXPLANATION.html)
4. 若想理解历史背景，看 [STM32F334_TO_G474_MIGRATION_PLAN.md](STM32F334_TO_G474_MIGRATION_PLAN.md)

---

## 给开源使用者的最后提醒

你当前看到的是一个**板级工程**，不是一个“开箱即用”的通用电源 demo。真正最容易踩坑的地方不是代码本身，而是：

- 拿错子工程
- 没有匹配硬件
- 把显示值误当成真实物理量
- 不理解 WAIT / RISE / RUN 状态就开始怀疑程序没跑

如果你想稳定上手，先把“工程入口、硬件依赖、显示语义”这三件事看清楚，比立刻改代码更重要。