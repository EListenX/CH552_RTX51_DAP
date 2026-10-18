# 性能记录及计时边界

整理日期：2026-10-19。速度单位KiB/s=1024 B/s。
当前源码快照的三个Target为ssj1、gsf1、jsr1：均已重建验证，**尚无各自实机测速结果**。
下表仅说明本项目历史优化水平，不将旧固件的结果冒充为新功能组合成绩。

## 本机Keil：完整下载墙钟

目标STM32F407VET6，512 KiB全量镜像，包含启动、擦除、编程、校验及运行。
每项为单轮有限回归，不是多轮统计或同一时刻的随机配对实验。
完整SHA读回与小测试程序恢复通过；恢复的是测试程序，不是用户原Flash完整备份。

| 固件 | 协议 | 下载秒 | 下载KiB/s | 独立读回KiB/s |
| --- | --- | ---: | ---: | ---: |
| gso1 纯GPIO SWD/SWO | SWD | 29.9759 | 17.0804 | 55.3938 |
| gsjh 纯GPIO SWJ维护版 | SWD | 31.5911 | 16.2071 | 46.5515 |
| gsjh 纯GPIO SWJ维护版 | JTAG | 38.2750 | 13.3769 | 29.1943 |
| jcd1 JTAG SPI/GPIO SWD | SWD | 31.2235 | 16.3979 | 45.2062 |
| jcd1 JTAG SPI/GPIO SWD | JTAG | 31.9347 | 16.0327 | 41.5050 |
| SWD SPI/SWO CDC维护版 | SWD | 26.7162 | 19.1644 | 57.0253 |

## 本机OpenOCD：纯编程与擦除＋编程

同F407、同512 KiB镜像；OpenOCD 0.12.0-7，主机请求6 MHz。
纯GPIO的请求频率不是实测线频，SPI也只接管部分相位。
每项三轮，纯编程与擦除＋编程吞吐均按镜像大小除以对应平均耗时计算。

| 历史固件 | 纯编程KiB/s | 擦除＋编程KiB/s |
| --- | ---: | ---: |
| 本项目twg2，GPIO汇编基线 | 53.08 | 28.51 |
| 本项目tat2，读写ACK分入口 | 83.18 | 35.17 |
| 本项目SWD SPI/SWO CDC维护版 | 83.35 | 35.41 |
| HS DAP GPIO汇编24MHz本机适配修正版 | 87.52 | 36.04 |

本机参考适配版来自CH55x_HS_DAP-Link-v2，同CH552/F407与负载，但修改了引脚、
DIR假设和若干兼容性问题；不是作者原始发布HEX。顺序批次不等于所有环境固定收益。
同环境回烧旧tat2后为83.09 KiB/s，对比维护版83.35 KiB/s基本持平。
不能拿Keil的19.16与OpenOCD纯编程83.35相减，认定固件速度下降。
维护版和参考适配版各保留三条既有ITM_TCR_BUSY_BIT告警；烧录、校验与完整读回通过。

## 竞品公开样本：仅作参考

第一手来源：[Ruan Xingzhi，DIY：自制DAPLink（2024-08-10）](https://www.ruanx.net/diy-daplink/)。
以下为作者对H743、约1 MiB镜像的OpenOCD擦除＋下载日志口径；不是本机同台测试。

| 样本 | 擦除＋下载约KiB/s |
| --- | ---: |
| CH55x_HS_DAP-Link-v2 / CH552，16MHz公开样本 | 35.17 |
| 国产DAPLink C6T6A | 34.46 |
| 国产DAPLink C8T6 | 34.48 |
| J-Link edu mini | 61.03 |
| 树莓派debugprobe | 66.14 |
| PWLINK2 Lite | 66.16 |
| STLINK-V3SET | 66.49 |

F407与H743的擦除/写入算法、主机与工具配置不同，不能据此给探针作严格排名，
更不能宣称本项目比F103探针更快。尚无已核对的F103 DAPLink JTAG同口径数据。

## 证据追溯

数值来源为原开发工作区中对应固件的benchmark.json，未将包含设备序列号、
绝对路径、抓包或历史日志的大量开发资料复制到这个干净仓库。
原记录名称：gso1-swd-benchmark、gsjh-swd-benchmark、gsjh-maintained-jtag-benchmark、
jcd1-swd-benchmark、jcd1-jtag-benchmark、swc-maintained-swd-benchmark、
twg2-swd-openocd-3、tat2-swd-openocd-3、swc-maintained-swd-openocd-3、
hs-dap-p16-24m-compat-openocd-3、tat2-ab-20261019-openocd-3。
这些是记录名称，不是本仓库中不存在文件的链接。
本快照源码的构建身份、占用和预期HEX SHA见根目录BUILD_BASELINE.json。
