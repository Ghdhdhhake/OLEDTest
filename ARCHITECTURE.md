# OLEDTest 项目架构说明（面向嵌入式学习者）

> 芯片：STM32F103C8（Cortex-M3，72MHz）　屏幕：SSD1306 OLED 128×64（I2C 地址 0x78）
> 功能：用 GPIO 模拟 I2C，往屏上循环播放 30 帧猫咪动画。
> 全项目实际用到的外设只有：**GPIOB（PB6/PB7）、SysTick、RCC/FLASH（时钟）**。

---

## ① 系统架构总览（绿=正在使用，灰=编译了但没启用）

```mermaid
flowchart TB
    classDef used fill:#d4edda,stroke:#28a745,stroke-width:2px
    classDef unused fill:#f8f9fa,stroke:#adb5bd,stroke-dasharray:5 3
    classDef hw fill:#fff3cd,stroke:#ffc107,stroke-width:2px

    subgraph BOOT["⚙️ 启动 startup/"]
        S["startup_stm32f10x_md.s<br/>建向量表 → 调 SystemInit → 跳 main"]
    end

    subgraph APP["📱 应用层 user/"]
        MAIN["main.c<br/>程序入口：初始化 + 播放 30 帧猫咪动画"]
        SYS["system_stm32f10x.c<br/>SystemInit：直接写寄存器<br/>HSE + PLL → 72MHz"]
        IT["stm32f10x_it.c<br/>SysTick_Handler 中断<br/>每 1ms 给 ulTicks 加 1"]
    end

    subgraph LIB["📚 功能库 my_lib/"]
        OLED["oled.c/h — SSD1306 驱动<br/>显存 128×8=1024B · 初始化命令序列<br/>画点/线/圆/位图 · 刷屏"]
        SI2C["si2c.c/h — 软件 I2C ✅<br/>用 GPIO 模拟时序（当前实际使用）"]
        DELAY["delay.c/h — 延时<br/>Delay(ms) / GetTick()"]
        FONT["oled_font.h + 字库<br/>字体结构定义 + 字形点阵"]
        I2C["i2c.c/h — 硬件 I2C ⭕<br/>已编译 · main 未调用"]
        USART["usart.c/h — 串口 ⭕<br/>已编译 · main 未调用"]
    end

    subgraph SPL["🧩 官方外设库 std_periph_driver/"]
        STD["GPIO / RCC 模块（被调用）<br/>其余 20 个外设模块：编译但零调用"]
    end

    subgraph HW["🔌 硬件层"]
        MCU["STM32F103C8 · Cortex-M3<br/>Flash 128KB · RAM 20KB"]
        PB["GPIOB 引脚<br/>PB6 = SCL（时钟线）<br/>PB7 = SDA（数据线）<br/>开漏输出 + 板上上拉"]
        OLEDHW["OLED 显示屏<br/>SSD1306 控制器 · I2C 从机 0x78<br/>128×64 像素"]
    end

    S -->|"① SystemInit"| SYS
    S -->|"② 跳转"| MAIN
    SYS -->|"③ 直接操作 RCC/FLASH 寄存器"| MCU
    MAIN -->|"④ 填引脚 PB6/PB7 → My_SI2C_Init"| SI2C
    MAIN -->|"⑤ 注册回调 → OLED_Init"| OLED
    MAIN -->|"⑥ 循环：清屏/画位图/刷屏"| OLED
    MAIN -->|"⑦ 每帧 Delay(10ms)"| DELAY
    OLED -. "⑧ 回调 i2c_write_cb（main 注册）" .-> MAIN
    MAIN -->|"⑨ 回调实现：My_SI2C_SendBytes"| SI2C
    OLED -->|"字形/字库数据"| FONT
    SI2C -->|"⑩ GPIO_Init/WriteBit + RCC 时钟"| STD
    DELAY -->|"RCC_GetClocksFreq"| STD
    DELAY -. "⑪ 依赖 SysTick 中断累计时间" .-> IT
    STD -->|"⑫ 寄存器操作"| MCU
    MCU -->|"⑬ 引脚电平翻转"| PB
    PB -->|"⑭ I2C 总线协议传输"| OLEDHW

    class MAIN,OLED,SI2C,DELAY,IT,SYS,STD used
    class I2C,USART unused
    class MCU,PB,OLEDHW hw
```

## ② 外设资源地图（芯片视角：谁在用、谁闲着）

```mermaid
flowchart LR
    classDef used fill:#d4edda,stroke:#28a745,stroke-width:2px
    classDef unused fill:#f8f9fa,stroke:#adb5bd,stroke-dasharray:5 3

    subgraph CHIP["STM32F103C8 芯片内部资源"]
        direction TB
        SYSTICK["SysTick（内核定时器）✅<br/>1ms 中断 → Delay/GetTick"]
        GPIOB["GPIOB ✅<br/>PB6=SCL · PB7=SDA"]
        RCC["RCC ✅<br/>HSE+PLL → 72MHz · 使能 GPIOB 时钟"]
        FLASH["FLASH 接口 ✅<br/>预取/等待周期（72MHz 必需）"]
        I2CHW["硬件 I2C1/I2C2 ⭕<br/>代码已写，未启用"]
        USARTHW["USART ⭕<br/>代码已写，未启用"]
        OTHERS["ADC/TIM/SPI/DMA/EXTI/CAN… ⭕<br/>编译进工程，零调用"]
    end

    GPIOB -->|"I2C 协议 · 从机地址 0x78"| SSD["OLED 屏（SSD1306）<br/>128×64 · 显存 1KB"]
    SYSTICK -. "1ms 节拍" .-> SSD
    RCC -. "72MHz" .-> SYSTICK

    class SYSTICK,GPIOB,RCC,FLASH used
    class I2CHW,USARTHW,OTHERS unused
```

## ③ 一帧动画的数据流（从内存到屏幕）

```mermaid
sequenceDiagram
    autonumber
    participant M as main.c 主循环
    participant O as oled.c 驱动
    participant CB as 回调 i2c_write_bytes<br/>(main.c:1094 注册)
    participant SI as si2c.c 软件 I2C
    participant GP as GPIOB 引脚
    participant S as SSD1306 屏

    Note over M: while(1)：外层先 OLED_Clear 一次<br/>（OLED_Clear 只 memset 显存，不走 I2C）
    loop 播放 30 帧动画 (i=0..29)
        M->>O: OLED_Clear() — 纯内存操作，清 1024B 显存
        M->>O: OLED_DrawBitmap(cat[i], 64×64) — 纯内存操作，画入显存
        M->>O: OLED_SendBuffer() 请求刷屏
        Note over O: 刷屏 = 4 次独立 I2C 写事务：
        O->>CB: 事务① 写命令 {0x00,0x20,0x00} 设横向寻址 (3B)
        O->>CB: 事务② 写命令 {0x00,0x21,0x00,0x7F} 设列范围 (4B)
        O->>CB: 事务③ 写命令 {0x00,0x22,0x00,0x07} 设页范围 (4B)
        O->>CB: 事务④ 写数据 {0x40 + 1024B 显存} (1025B)
        CB->>SI: My_SI2C_SendBytes(&si2c, 0x78, 数据, 长度)<br/>（以上每个事务各触发一次，共 4 次）
        loop 每个事务内（共 4 次）
            SI->>GP: 起始位：SCL 高时拉低 SDA
            SI->>GP: 发送地址字节 0x78（8 bit 逐位翻转 SCL/SDA）
            S-->>SI: ACK：从机拉低 SDA 应答
            SI->>GP: 发送数据字节（每字节 8 bit）
            S-->>SI: 每字节后 ACK
            SI->>GP: 停止位：SCL 高时释放 SDA
        end
        SI-->>CB: 返回 0（成功）/ -1 寻址失败 / -2 数据被拒收
        CB-->>O: 透传结果
        O-->>M: 任一事务失败则返回 -1（main 忽略返回值）
        M->>M: Delay(10ms)：忙等 while(ulTicks < 目标)<br/>SysTick 中断每 1ms 给 ulTicks 加 1
    end
    Note over M: 换下一帧 cat[i+1]，循环 30 帧 → 猫咪动起来
```

## 📋 外设速查表

| 功能 | 使用的外设/资源 | 引脚/参数 | 代码位置 |
|------|----------------|-----------|----------|
| OLED 通信 | GPIOB（软件模拟 I2C） | **PB6=SCL、PB7=SDA**，开漏输出 | `main.c:1086`、`si2c.c` |
| OLED 屏幕 | SSD1306（I2C 从机） | 地址 **0x78**，128×64，显存 1024B | `oled.h:16`、`oled.c` |
| 延时/帧率 | SysTick（内核定时器） | 1ms 中断 → `Delay(10ms)` | `delay.c`、`stm32f10x_it.c:163` |
| 系统时钟 | RCC + HSE + PLL | 72MHz（直接写寄存器） | `system_stm32f10x.c` |
| 备用：串口 | USART（未启用） | — | `usart.c`（main 未调用） |
| 备用：硬件 I2C | I2C1/I2C2（未启用） | — | `i2c.c`（main 未调用） |

## 🎓 值得学习的 5 个嵌入式知识点

1. **软件 I2C vs 硬件 I2C**：`si2c.c` 用 GPIO 逐位翻转模拟 I2C 时序（起始位/地址/ACK/停止位），对照 `i2c.c` 的外设版，理解"协议"与"实现方式"的关系。
2. **SysTick 与裸机计时**：`delay.c` 配置 SysTick 每 1ms 中断，`SysTick_Handler` 累加 `ulTicks` —— RTOS 时间片的基础，也是非阻塞延时/超时的入门写法。
3. **回调解耦**：`OLED_InitTypeDef.i2c_write_cb` 函数指针让 OLED 驱动不依赖具体 I2C 实现，换硬件 I2C 只改 main.c 一行 —— 驱动分层的标准范例。
4. **SSD1306 显存模型**：128 列 × 8 页（每页 8 像素高）= 1024B，画点公式 `pBuffer[x + (y/8)*128] |= 1<<(y%8)`（`oled.c:1215`）。
5. **时钟树与外设使能**：`SystemInit` 直接写 RCC 寄存器把系统时钟提到 72MHz；每个外设使用前必须 `RCC_APB2PeriphClockCmd` 开时钟（`si2c.c:31-65`）——新手"程序没反应"一半是漏了这一步。
