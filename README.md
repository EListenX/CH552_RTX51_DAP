# CH552_RTX51_DAP

基于 CH552、Keil C51 和 RTX51 Tiny 的 CMSIS-DAP 调试器固件。
通过 USB 全速复合设备提供 CMSIS-DAP v2 WinUSB Bulk 与 CDC 虚拟串口，
提供纯 GPIO SWJ、SWD SPI 加速和 JTAG SPI 加速三种独立构建配置。

这是从开发工作区整理出的独立源码工程，不包含原 Gitea 历史。
**当前三个 Target 均已完整编译、ISP 检查及异地目录重建通过；
这些新功能组合尚未完成对应实机验收，历史性能数据不能代替它们的测速结果。**

## 架构

```text
主机：Keil / OpenOCD / CMSIS-DAP客户端 / 串口工具
                  |
        CH552 USB全速复合设备
         |                      |
   EP1 WinUSB Bulk DAP       EP2 CDC数据
         |                      |
   USB ISR收包/端点交接     USB OUT -> TX ping-pong -> UART0 TX
         |                  UART0 RX ISR -> RX ring -> USB IN DMA槽
   RTX51 DAP独立任务
         |
   DAP.C：命令解析、包边界、重试、AP posted-read收尾
         |
   SW_DP.C / JTAG_DP.C：SWD状态机与JTAG TAP/扫描
         |
   SWD_Byte.C汇编叶 / DAP_SPI.C / IO_Config.H板级引脚
```

- `Start.C`：24 MHz 时钟、外设与 RTX51 任务初始化。
- `Usb.C`：枚举、端点及缓冲区所有权、任务通知；不在 USB ISR 中执行 SWD/JTAG 长传输。
- `DAP.C`：CMSIS-DAP 协议分发、SWD/JTAG 模式及传输封装。
- `SW_DP.C`、`JTAG_DP.C`：GPIO/硬件后端共享上层命令处理；同一时刻仅选择一个调试端口。
- `DAP_SPI.C`：SWD 写数据相位的两线 SPI，或 JTAG 恒定 TMS 数据段的三线 SPI。
  ACK、换向和 TAP 状态切换等相位仍由 GPIO/汇编完成，不是所有相位都由 SPI 接管。
- `SWD_Byte.C`：C51 导出板级符号后交由 A51 组装，提供字节/奇偶校验等紧凑底层。
- `Uart.C`：CDC 使用 UART0；USB→UART TX 保留 ping-pong，UART RX 使用128 B ring
  （可用127 B），由 USB/Timer1 消费并填入原有两个64 B IN DMA槽。
  64 B积压唤醒服务、短包2 ms聚合、空闲满包尾部一次ZLP；USB包长仍64 B。
- `SWO.C`：UART1 alternate RX=P3.4 接收 NRZ SWO，128 B ring，经 DAP_SWO_Data 读取。
  全功能 GPIO 配置进入 JTAG 时停止 SWO，避免目标 PB3 的 TDO/SWO 复用冲突。
- `CH55x_Math.A51`：共用紧凑时钟/波特率运算，不因体积优化裁掉任意波特率计算。

芯片资源为256 B内部RAM、1 KiB XRAM、16 KiB物理Flash；
用户程序仅允许0x0000..0x37FF，上方ISP BootLoader区不得覆盖。
USB/UART/RTX的寄存器组、中断嵌套及任务栈受8051约束，RAM统计不等于实测栈余量。

## 性能速览

以下是**历史构建**在 STM32F407VET6、512 KiB 全量镜像上的实测，
不是本仓库三个新 Target 的性能承诺。KiB/s = 1024 B/s。

| 历史配置 | 协议 | Keil完整下载 KiB/s | 512 KiB耗时 |
| --- | --- | ---: | ---: |
| gso1，纯GPIO SWD＋SWO | SWD | 17.08 | 29.98 s |
| gsjh，纯GPIO SWJ维护版 | SWD | 16.21 | 31.59 s |
| gsjh，纯GPIO SWJ维护版 | JTAG | 13.38 | 38.28 s |
| jcd1，JTAG SPI＋GPIO SWD | JTAG | 16.03 | 31.93 s |
| SWD SPI＋SWO维护版 | SWD | 19.16 | 26.72 s |

Keil完整下载为进程墙钟，包含启动、擦除、编程、校验及运行，不等于纯编程速度。
同一 SWD SPI维护版另用OpenOCD三轮测得：纯编程平均6.143 s、**83.35 KiB/s**，
擦除＋编程**35.41 KiB/s**。同片HS DAP参考适配修正版的历史纯编程为87.52 KiB/s；
两者为顺序批次，并非随机交替试验，不能承诺固定差距。
请求时钟/SPI分频不等于实际有效协议吞吐，不能由4 MHz直接推出Flash下载速度。

当前默认ssj1、纯IO全功能gsf1和JTAG SPI双复位jsr1均**待对应实机测速**。
详细计时边界、批次数和来源见[性能说明](docs/PERFORMANCE.md)。

打开 `Project/CH552_RTX51_DAP.uvproj`。第一个 Target 为主版本开发基线。
仅包含实际编译源码、依赖头文件、启动/RTX 配置和 ISP 检查工具。
源码保持原 UTF-8/CP936 字节及版权注释；不包含历史实验工程、资源仓库、
个人 uvopt/uvgui、Objects/Listings、旧 HEX 或测试采集记录。

## 构建

需要已授权的 Keil C51/A51/LX51、RTX51 Tiny 和 Python 3（在 PATH 中）。
原验证工具版本为 C51 9.60.7.0、A51 8.2.7.0；RTX 头文件与运行库由 Keil 提供，
不在本包复制工具链二进制。工程为 LARGE 模型，24 MHz，IRAM 256 B、XRAM 1 KiB，
IROM 0x0000..0x37FF；不可关闭 AfterMake 的 ISP 边界检查以绕过超限。
直接选择 Target 并 Rebuild，HEX 输出为：
`Project/Objects/<Target>/CH552_RTX51_DAP.hex`。
SWD_Byte.C 已保留 Generate/Assemble Assembly 设置，不要改成单纯 C 编译。
所有工程引用均为包内相对路径，工程可整体移动。

| Target | 身份 | 功能 | ROM 基线 |
| --- | --- | --- | ---: |
| SWD_SPI_SWJ（默认） | ssj1 | SWD SPI + GPIO JTAG + CDC + nTRST/nRESET；无 SWO | 13651 B |
| GPIO_SWJ_FULL | gsf1 | GPIO SWD/JTAG + SWO + CDC + nTRST/nRESET | 14305 B |
| JTAG_SPI_SWJ | jsr1 | JTAG SPI + GPIO SWD + CDC + nTRST/nRESET；无 SWO | 13349 B |

这些新组合均待对应的实机验收，不能将构建通过解释为已硬件验证。
gsf1 ROM 仅余24 B，XRAM 使用1015 B；实际任务/嵌套中断栈峰值仍待确认。
编译宏固定在各 Target 的 C51 Define 中，切换配置请选 Target，
不要只改 Board_Config.H 而忽略 Target 宏。资源/SHA 基线见 BUILD_BASELINE.json。

## CH552 到 STM32F407 接线

断电改线；两板共地。目标须有合适供电，信号脚不得接5 V。

| 信号 | ssj1 | gsf1 | jsr1 | F407 |
| --- | --- | --- | --- | --- |
| SWCLK/TCK | P1.7 | P1.7 | P1.7 | PA14 |
| SWDIO/TMS | P1.6 | P1.6 | P3.3 | PA13 |
| TDI | P1.5 | P1.5 | P1.5 | PA15 |
| TDO | P3.3 | P3.3 | P1.6 | PB3 |
| nTRST | P3.2 | P3.2 | P3.2 | PB4 |
| nRESET | P3.4 | P1.4 | P3.4 | NRST |
| SWO | 禁用 | P3.4 | 禁用 | PB3 |
| GND | GND | GND | GND | GND |

gsf1 的 PB3 在 JTAG TDO/SWD SWO 间复用；可分别接入两个探针输入，
但不能将两种协议并行使用。进入 JTAG 会停止 SWO，返回 SWD 后需重新配置捕获。
ssj1/jsr1 的 P3.4 是复位输出，不得保留连接 PB3 的旧 SWO 线。
CDC：P3.1 TX -> 对端 RX，P3.0 RX <- 对端 TX，GND 共地，勿将 VCC 相连。
纯 SWD 可不接 TDI/TDO/nTRST；对应功能的实测需补齐接线。

## 验证范围与已知边界

- 三个 Target 在独立、含空格路径中完整重建均为0错误0警告，HEX SHA与
  [构建基线](BUILD_BASELINE.json)一致；`hardware_verified=false`明确保留。
- 新ssj1/gsf1/jsr1待实机验证：SWD/JTAG切换、复位、调试/Flash、CDC并发和真实栈峰值。
- 历史测试目标主要为F407VET6；不能推断全部STM32、所有板型或CH554已经适配。
- 历史维护版有限CDC 115200/500000回归通过，不等于所有新组合、1.5 M RX或长期并发可靠性通过。
  曾有jcd1高速CDC ring丢弃记录，仍需后续跟踪，不以单批成功掩盖历史失败。
- 未实现或未宣告的能力包括Atomic/Queue Commands、Manchester SWO和独立SWO USB流端点。
- gsf1用户Flash地址余量仅24 B、XRAM未占用仅9 B；后续改动必须重新完整构建和检查。

## 代码来源与许可

保留原文件的CMSIS-DAP、WCH及Keil/RTX版权与许可标记，未将所有文件统一改授新许可证。
参见[第三方来源说明](THIRD_PARTY_NOTICES.md)；构建依赖的商业工具链需自行合法安装。
