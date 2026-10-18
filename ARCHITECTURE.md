# 架构与RTX51 Tiny运行模型

本工程的控制框架是Keil RTX51 Tiny，数据通路由USB DMA缓冲区、应用ISR及DAP任务共同实现。
RTX不是USB协议栈，也不是串口DMA控制器；“由内核调度任务”不等于“所有外设中断由内核代办”。

## 启动与任务生命周期

```text
8051启动代码 / RTX启动流程
       |
Task 0 INIT：CfgFsys、时钟稳定、允许全局中断
       |
       ├─ 创建Task 1 DAP -> DAP_Setup -> 常驻命令处理/事件等待
       ├─ 创建Task 2 USB -> USBDeviceInit -> 删除Task 2
       └─ 创建Task 3 UART -> InitUart0 -> 删除Task 3
       |
INIT删除自身

稳定运行：DAP任务 + 外设ISR + RTX Timer0/等待/空闲机制
```

对应源码：[Start.C](Application/Start.C)、[任务编号](Application/include/Config.H)、
[DAP任务](Application/DAP.C)、[USB初始化任务](Application/Usb.C)、[UART初始化任务](Application/Uart.C)。
任务创建顺序不是固定硬件中断优先级，也不能据此认定初始化任务一直驻留。
当前未创建独立LED闪烁、按键或转义处理常驻任务。

## 调试命令：ISR通知，任务执行

1. USB控制器将EP1数据放入端点XRAM缓冲区。
2. USB ISR检查传输和缓冲区状态，记录实际请求长度，设置就绪状态并发送RTX ISR信号。
3. DAP任务检查请求和可用响应槽，固定本次通道及USB会话epoch，再执行CMSIS-DAP命令。
4. SWD/JTAG由任务调用GPIO、汇编叶或SPI后端；SPI不属于ISR，也不是两个协议同时使用。
5. 回复提交用短USB中断临界区核对epoch及槽状态；若期间发生USB复位，不提交旧会话结果。
6. 主机读完响应后，USB ISR完成IN槽交接；DAP任务无可处理请求时等待`K_SIG`。

信号是一种通知，不是可靠的逐包计数器。多个事件可合并，缓冲区状态与所有权必须独立维护。
不能仅因收到信号就消费某个槽，也不能在USB ISR里执行Flash下载协议循环。

## CDC与SWO：字节实时路径不依赖常驻UART任务

| 方向 | 实际通路 | RTX的作用 |
| --- | --- | --- |
| USB到UART0 | EP2 OUT双缓冲 -> UART0 TX中断逐字节发送 | UART任务仅初始化，不是每字节发送任务 |
| UART0到USB | UART0 RX ISR -> 128 B ring -> USB/Timer1服务 -> 两个64 B IN DMA槽 | 不等DAP任务或一次轮转才接收字节 |
| SWO到主机 | UART1 RX ISR -> 128 B SWO ring -> DAP_SWO_Data命令 | 在DAP任务中读取/封装SWO数据 |

CDC RX ring可用127 B；两枚IN DMA槽不等于USB包长变为128 B。
2 ms聚合由应用Timer1服务实现，不能与RTX Timer0 tick混为一谈。
SWO仅在启用的GPIO全功能Target中存在；JTAG模式停止SWO，切回SWD后重新配置捕获。

## 中断、寄存器组与调度边界

| 所属 | 定时器/中断 | 寄存器组 | 职责 |
| --- | --- | ---: | --- |
| RTX51 Tiny | Timer0 | 2 | tick、定时等待、轮转、任务/栈管理和空闲处理 |
| USB应用层 | USB IRQ | 1 | 枚举、DMA端点状态、缓冲交接、通知DAP任务 |
| CDC UART | UART0 IRQ，高优先级 | 3 | UART0字节接收、TX发送推进 |
| CDC聚包服务 | Timer1 IRQ，低优先级 | 1 | RX ring消费、USB短包聚合/空闲尾包服务 |
| SWO，可选 | UART1 IRQ，低优先级 | 1 | SWO字节接收和溢出记录 |

USB/Timer1/UART1同为低优先级，不能因为使用相同寄存器组就提高其中一个使其互相嵌套。
高优先级UART0能抢占低优先级ISR；硬件优先级不等于RTX任务编号或轮转配额。
任务执行期间也可能被ISR抢占，影响GPIO/SPI相位之间的实际间隙。

[Conf_tny.A51](Project/Conf_tny.A51)保留INT_CLOCK=16000、TIMESHARING=5、
INT_REGBANK=2、RAMTOP=0xFF、FREE_STACK=20及CPU_IDLE_CODE=1。
INT_CLOCK不是毫秒单位；未独立测量时，不将16000或五tick直接写成某个实测时间。
任务上下文、ISR保存和内核栈管理均占用8051内部RAM；内部RAM占用表或FREE_STACK阈值
不能代替完整任务调用、ISR嵌套与RTX切换时的最坏栈峰值证明。

## 三个Target与框架关系

三个Target共用同一个RTX/USB/CDC框架，仅协议后端、能力及引脚组合不同。
默认ssj1为SWD SPI/GPIO JTAG；gsf1为GPIO SWJ/SWO；jsr1为JTAG SPI/GPIO SWD。
它们均需单独实机回归，不能将旧后端的性能结果直接继承为新Target成绩。
构建依赖、接线、容量及待验收范围见[README](README.md)，
计时口径与历史实测见[性能说明](docs/PERFORMANCE.md)。
