# G474_Final2-main

这是 `D:\FInal_Wifi76` 总仓库中的一个 STM32G474 子工程。

- 工程入口（Keil）：[`MDK-ARM/G474_NewStart.uvprojx`](MDK-ARM/G474_NewStart.uvprojx)
- 配置入口（CubeMX）：[`G474_NewStart.ioc`](G474_NewStart.ioc)

如果你是第一次接触整个仓库，建议先看顶层入口：[../README.md](../README.md)。

---

## 这个子工程在总仓库中的定位

相较于另外两个子工程，`G474_Final2-main` 当前最明确的特点是：

> **它附带了最多的现有分析型文档，适合作为理解“显示值语义、偏置、迁移背景、电流逻辑来源”的入口工程。**

也就是说，如果你的目标不是“直接编译下载”，而是先弄清楚这套工程为什么会这样显示、这样标定、这样迁移，那么这个目录是最值得优先阅读的一个。

---

## 工具链与入口

### Keil 工程入口
- [`MDK-ARM/G474_NewStart.uvprojx`](MDK-ARM/G474_NewStart.uvprojx)

### CubeMX 配置入口
- [`G474_NewStart.ioc`](G474_NewStart.ioc)

### 已知平台信息
根据 `uvprojx` / `ioc` 可确认：

- MCU：`STM32G474CBTx`
- 工程类型：`STM32CubeMX + Keil MDK-ARM`
- 涉及外设：ADC、DMA、HRTIM、TIM2、TIM3、USART3 等

---

## 硬件前提

该工程并不是“任意 STM32G474 开发板都能直接运行”的通用示例。请默认它依赖：

- 特定的 STM32G474 功率板
- 外部 HSE
- Buck / Boost 功率级
- 采样链
- TFT / LCD
- KEY2 按键
- USART3 串口通道

如果你当前只是从开源仓库下载代码、但没有原板卡，请优先把它当作“参考工程 + 文档工程”阅读，而不是直接烧录目标。

---

## 上手步骤

1. 用 Keil 打开 [`MDK-ARM/G474_NewStart.uvprojx`](MDK-ARM/G474_NewStart.uvprojx)
2. 检查 STM32G4 Pack 是否齐全
3. 如需核对外设配置，用 CubeMX 打开 [`G474_NewStart.ioc`](G474_NewStart.ioc)
4. 编译并下载到匹配板卡
5. 上电后观察 TFT、状态机行为和串口输出
6. 使用 `KEY2` 触发输出使能
7. 若需调试目标显示值，通过串口发送 `V####`

---

## 运行逻辑简述

当前工程包含以下已知结构：

- Buck / Boost 两种模式
- `WAIT -> RISE -> RUN` 状态机流程
- `KEY2` 触发输出使能
- USART3 周期发送状态帧
- USART3 接收 `V####` 命令设置 VTAR
- TFT 显示 `vin / vout / ii / io / vtar`

若你要理解这些行为来自哪里，建议重点阅读：

- [`Core/Src/main.c`](Core/Src/main.c)
- [`Core/Src/minimal_state_machine.c`](Core/Src/minimal_state_machine.c)
- [`Core/Src/tft_debug_view.c`](Core/Src/tft_debug_view.c)

---

## 串口协议简述

### 状态帧
代码显示 USART3 会周期发送：

- 帧头：`0xAA`
- 字段：`mode / vin / vout / ii / io / vtar`
- 每个字段 16-bit
- 末尾 XOR 校验

### 设置命令
接收格式为：

```text
V####
```

例如：

```text
V2500
```

用于把上位机指定的目标显示值应用到内部 `vref_target`。

> 注意：字段单位、缩放、显示逻辑并不保证是“真实物理量直读”，而是工程内部最终显示链结果。

---

## 重要提醒：这个目录最值得看的不是“结果”，而是“解释”

你如果只把这个工程当作“又一个能编的 G474 工程”，会浪费它最有价值的部分。它最有价值的是：

- 为什么当前显示值与实测值可能不一致
- 为什么要引入偏置 / 补偿 / 判定链
- 旧 F334 逻辑是如何影响当前 G474 工程的
- 为什么某些显示量不能直接等价为真实电流/电压

也就是说：

> `G474_Final2-main` 不只是一个工程目录，更像是这个仓库现阶段的“解释层”。

---

## 本目录现有文档（强烈建议阅读）

### 显示 / 采样语义
- [TFT_SAMPLING_DISPLAY_JUDGMENT.html](TFT_SAMPLING_DISPLAY_JUDGMENT.html)
- [OFFSET_EXPLANATION.html](OFFSET_EXPLANATION.html)

### 电流逻辑来源
- [F334_CURRENT_LOGIC.html](F334_CURRENT_LOGIC.html)

### 输入源 / 控制行为分析
- [INPUT_SOURCE_CC_ANALYSIS.html](INPUT_SOURCE_CC_ANALYSIS.html)

### 迁移背景
- [STM32F334_TO_G474_MIGRATION_PLAN.md](STM32F334_TO_G474_MIGRATION_PLAN.md)

---

## 推荐阅读顺序

如果你想真正理解这个工程，建议按下面顺序：

1. 先看本 README
2. 再看 [TFT_SAMPLING_DISPLAY_JUDGMENT.html](TFT_SAMPLING_DISPLAY_JUDGMENT.html)
3. 再看 [OFFSET_EXPLANATION.html](OFFSET_EXPLANATION.html)
4. 如果你想追根溯源，再看 [F334_CURRENT_LOGIC.html](F334_CURRENT_LOGIC.html)
5. 最后看 [STM32F334_TO_G474_MIGRATION_PLAN.md](STM32F334_TO_G474_MIGRATION_PLAN.md)

---

## 给开源使用者的关键提醒

你现在最容易犯的错误不是“代码没看懂”，而是：

1. 把 TFT / 串口显示值直接当成真实物理量
2. 不理解偏置、滤波、换算、判定链就开始标定
3. 只盯着某一个数值，而忽略这个目录里其实已经有现成解释文档

所以如果你是第一次进入这个工程，最值得做的不是先改系数，而是先搞清楚：

> **当前屏幕上那个数字，到底表示的是什么。**

这件事弄清楚之后，你再用这个工程做校准、分析或迁移，效率会高很多。