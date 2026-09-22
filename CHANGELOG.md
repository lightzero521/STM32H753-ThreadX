# Changelog

## 0.1.3

- Power 模块接入 SHP8808 bring-up：I2C1 PB8/PB9（CN7 I2C_A）探测 0x6C，软主机、ADC 连续采样（不置 ADC_START）、充电默认关
- 串口 1 Hz 打印 VBUS/VBAT/IBUS/IBAT 和 ADC `0x1A`；HTTP `GET/POST /api/shp8808`，调试台默认 SHP8808 页签
- BQ25756 仍可选：总线上有 0x6B 才启用，不再阻塞启动
- `console_print` 增加 `%ld`

## 0.1.2

- 新增 SHP8808 驱动库（`src/modules/power/shp8808/`），I2C 地址 0x6C，风格对齐 BQ25756；Web 页签仍预留
- README 收成一句话；板级 / 构建 / 调试说明改记本文件

## 0.1.1

- SYSCLK 提到 480 MHz（ST-LINK 8 MHz HSE bypass → PLL，VOS0，AXI/AHB 240 MHz）
- I-Cache 开、D-Cache 关，仍不配置 MPU（DMA/ETH 无需 cache 一致性）
- HTTP `:80` BQ25756 电源调试台（充电启停、配置、ADC 曲线、异常锁存；ECharts 走 jsDelivr，电脑需能上网）
- 烧录/调试改为板载 ST-LINK + pyOCD（无 H753 内置项，目标用 `stm32h743xx`）
- Cortex-Debug 配置 `Debug (ST-Link pyOCD)`，`launch.json` 由 CMake 从 `.vscode/launch.json.in` 生成

## 0.1.0

- 按 [GD32F503-ThreadX](https://github.com/lightzero521/GD32F503-ThreadX) 模板搭出 NUCLEO-H753ZI 最小 ThreadX 工程；外设走 STM32 HAL，不移植 lzport
- SYSCLK 200 MHz（HSE bypass 8 MHz），不启用 I/D Cache，不配置 MPU
- ST-LINK VCP USART3 PD8/PD9 460800，上电打印 bring-up / 编译时间
- LD1/LD2/LD3 全部初始化，LD1 绿灯 2 Hz 闪烁
- 移植 `src/modules/power`（BQ25756），I2C1 PB8/PB9（Arduino D15/D14）100 kHz，需上拉
- NetX Duo + LAN8742A RMII，DHCP，失败回退 `192.168.1.80`；静态页在 `web/`
- 激活 mcu-env 后 `mcuenv.py build` / `mcuenv.py flash`，固件 `build/output/bringup_STM32H753.{elf,bin,hex}`
