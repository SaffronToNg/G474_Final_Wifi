# G474 Final Wifi 总仓库

这是一个面向 **STM32G474CBTx** 的总仓库，里面包含 3 个相关子工程：

- [G474_Final1-main](G474_Final1-main/)
- [G474_Final2-main](G474_Final2-main/)
- [G474_Final3-main](G474_Final3-main/)

这 3 个目录都不是“随便打开一个就能在任意开发板上跑”的通用示例，而是带有明显**板级依赖**的电源控制工程。它们都基于 **STM32CubeMX + Keil MDK-ARM**，并且涉及：

- Buck / Boost 模式
- 状态机（`WAIT -> RISE -> RUN`）
- ADC + DMA 采样链
- HRTIM 驱动与控制链
- TFT 显示
- USART3 串口交互
- 校准 / 偏置 / 显示换算

---

## 仓库结构

| 子工程 | 主要入口 | MCU / 工具链 | 当前说明 |
| --- | --- | --- | --- |
| [G474_Final1-main](G474_Final1-main/) | `MDK-ARM/G474_NewStart.uvprojx` / `G474_NewStart.ioc` | STM32G474CBTx / CubeMX + Keil | 可作为总仓库的代表性入口工程阅读 |
| [G474_Final2-main](G474_Final2-main/) | `MDK-ARM/G474_NewStart.uvprojx` / `G474_NewStart.ioc` | STM32G474CBTx / CubeMX + Keil | 含现有分析文档最多，适合深入理解显示/偏置/迁移背景 |
| [G474_Final3-main](G474_Final3-main/) | `MDK-ARM/G474_NewStart.uvprojx` / `G474_NewStart.ioc` | STM32G474CBTx / CubeMX + Keil | 与前两者共享相同工程骨架，适合对比不同阶段代码 |

> 说明：当前仓库已经确认三者共享相似工程骨架，但它们的精确版本角色（例如“最终版 / 实验版 / 中间版”）并未在代码中自带正式说明，因此本文档不会臆造差异定义。若你是维护者，建议后续补充每个子工程的明确定位。

---

## 快速开始

### 1. 准备工具
请先准备：

- **Keil MDK-ARM**（用于编译 / 下载）
- **STM32CubeMX**（用于查看或再生成 `ioc` 配置）
- 对应的 **STM32G4 Device Pack**

### 2. 选择一个子工程
首次上手建议从 [G474_Final1-main](G474_Final1-main/) 或 [G474_Final2-main](G474_Final2-main/) 开始。

每个子工程的两个关键入口分别是：

- `MDK-ARM/G474_NewStart.uvprojx`：Keil 工程入口
- `G474_NewStart.ioc`：CubeMX 配置入口

### 3. 在 Keil 中打开工程
例如打开：

- [G474_Final1-main/MDK-ARM/G474_NewStart.uvprojx](G474_Final1-main/MDK-ARM/G474_NewStart.uvprojx)

若你需要检查引脚、时钟、ADC/HRTIM/USART 配置，则打开：

- [G474_Final1-main/G474_NewStart.ioc](G474_Final1-main/G474_NewStart.ioc)

### 4. 编译与下载前先确认硬件条件
该仓库不是通用裸机示例。下载前请确认你手上的硬件至少满足：

- MCU：**STM32G474CBTx**
- 外部时钟：工程当前使用 **HSE 外部晶振**
- 串口：**USART3**
- 按键：存在 **KEY2** 参与输出使能
- 显示：带 **TFT / LCD** 驱动链
- 功率级 / 采样电路：ADC / HRTIM / Buck / Boost 相关板级硬件已就绪

---

## 已知运行机制速览

根据当前代码，系统具备以下已知行为：

- 存在 **Buck / Boost** 两种模式
- 状态机会经历 `WAIT -> RISE -> RUN`
- **KEY2** 用于触发输出使能
- 系统会周期性通过 **USART3** 发送状态帧
- 系统支持通过串口发送固定格式 **`V####`** 命令设置 `VTAR`
- TFT 上的 `vin / vout / ii / io / vtar` 是**最终显示链结果**，不是原始 ADC 量

---

## 串口交互速览

### 串口端口
代表性工程（如 `G474_Final1-main`）中，USART3 配置为：

- 波特率：**115200**
- 数据位：**8**
- 停止位：**1**
- 校验：**None**
- 引脚：
  - `PB10 -> USART3_TX`
  - `PB11 -> USART3_RX`

### 周期状态帧
代码显示系统会周期发送一帧状态数据：

- 帧头：`0xAA`
- 后续字段：`mode / vin / vout / ii / io / vtar`
- 每个字段为 **16-bit** 值
- 末尾为 **异或校验**

### VTAR 设置命令
串口接收使用固定格式：

```text
V####
```

例如：

```text
V2500
```

其作用是把上位机给定的目标显示值反推到 `vref_target`，再应用到系统中。

> 注意：字段单位、缩放、显示换算并不是“天然物理量直读”。如果你要把串口值与万用表/示波器直接对照，请先阅读下方“重要提醒”。

---

## 重要提醒（强烈建议先看）

### 1. 这不是通用开发板示例
即使 MCU 型号相同，也**不保证**能直接跑在其他 STM32G474 板子上。当前工程显著依赖：

- HSE
- 板载功率级
- 采样链
- TFT / LCD
- KEY2
- 特定 GPIO 连接

### 2. 三个工程不能默认完全等价
它们共享相同骨架，但不应假设三者内容、参数、校准链完全一致。请在进入某个子工程前先看该目录下的 README。

### 3. TFT / 串口显示值不一定等于真实物理测量值
当前工程中，`vin / vout / ii / io / vtar` 可能经过：

- ADC 原始采样
- 滤波
- 偏置修正
- 显示换算
- 量纲缩放
- 经验补偿或判定逻辑

所以：

> **不要默认 TFT 或串口上报值就是万用表 / 示波器看到的瞬时真实物理量。**

如果你需要理解这件事，请优先阅读下面的现有分析文档。

---

## 延伸阅读 / 现有分析文档

以下文档当前位于 [G474_Final2-main](G474_Final2-main/) 中，适合作为背景材料阅读：

- [F334_CURRENT_LOGIC.html](G474_Final2-main/F334_CURRENT_LOGIC.html)  
  说明 F334 电流逻辑背景与来源。
- [INPUT_SOURCE_CC_ANALYSIS.html](G474_Final2-main/INPUT_SOURCE_CC_ANALYSIS.html)  
  说明输入源 / 控制行为相关分析。
- [OFFSET_EXPLANATION.html](G474_Final2-main/OFFSET_EXPLANATION.html)  
  说明偏置、零点和测量误差来源。
- [TFT_SAMPLING_DISPLAY_JUDGMENT.html](G474_Final2-main/TFT_SAMPLING_DISPLAY_JUDGMENT.html)  
  说明 TFT 显示值和采样 / 显示判定链之间的关系。
- [STM32F334_TO_G474_MIGRATION_PLAN.md](G474_Final2-main/STM32F334_TO_G474_MIGRATION_PLAN.md)  
  说明项目从 F334 到 G474 的迁移背景。

---

## 建议的阅读顺序

如果你是第一次接触这个仓库，建议按下面顺序：

1. 先看本页（总仓库 README）
2. 再进入某个子工程目录看该工程的 README
3. 用 Keil 打开 `uvprojx`
4. 必要时用 CubeMX 打开 `ioc`
5. 如果对显示值 / 偏置 / 迁移背景有疑问，再阅读上面的分析文档

---

## 当前已知代码参考入口

以下文件有助于快速理解系统：

- [G474_Final1-main/Core/Src/main.c](G474_Final1-main/Core/Src/main.c)
- [G474_Final1-main/Core/Src/minimal_state_machine.c](G474_Final1-main/Core/Src/minimal_state_machine.c)
- [G474_Final1-main/Core/Src/tft_debug_view.c](G474_Final1-main/Core/Src/tft_debug_view.c)
- [G474_Final1-main/Core/Src/usart.c](G474_Final1-main/Core/Src/usart.c)
- [G474_Final1-main/G474_NewStart.ioc](G474_Final1-main/G474_NewStart.ioc)

---

## 后续建议（给维护者）

如果你准备长期把这个仓库作为公开入口维护，下一步最值得补充的不是更多技术细节，而是：

1. 明确 `Final1 / Final2 / Final3` 三者的官方定位
2. 补一张硬件连接或板卡功能框图
3. 单独整理一页串口协议说明
4. 补一张状态机流程图

这些内容会比继续堆零散注释更能帮助外部使用者上手。