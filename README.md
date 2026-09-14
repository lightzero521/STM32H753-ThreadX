面向 NUCLEO-H753ZI 的最小 ThreadX 工程，使用 mcu-env 构建。对照 [GD32F503-ThreadX](https://github.com/lightzero521/GD32F503-ThreadX)，外设走 STM32 HAL，不移植 lzport。

## 板级

- MCU：STM32H753ZIT6，SYSCLK **480 MHz**（ST-LINK 8 MHz HSE bypass → PLL，AXI/AHB 240 MHz，VOS0）
- **I-Cache 开、D-Cache 关**，不配置 MPU（DMA/ETH 无需维护 cache 一致性）
- 控制台：USART3（ST-LINK VCP，PD8/PD9），**460800** 8N1
- LED：LD1 PB0 绿 2 Hz；LD2 PE1 黄；LD3 PB14 红
- 电源模块：I2C1 PB8/PB9（Arduino D15/D14），BQ25756 飞线，需上拉
- 网口：LAN8742A RMII + NetX Duo，DHCP，失败则回退 `192.168.1.80`

## 构建

先激活 mcu-env，再编译：

```powershell
. D:\mcu-env\export.ps1
mcuenv.py build
```

固件在 `build/output/bringup_STM32H753.{elf,bin,hex}`。

## 烧录 / 调试

板载 **ST-LINK**，工程默认 **pyOCD**（本机 mcu-env 未配置 OpenOCD）。pyOCD 无 H753 内置目标，烧录/仿真用兼容目标 `stm32h743xx`。

```powershell
mcuenv.py flash
```

VS Code / Cursor 用 Cortex-Debug 配置 `Debug (ST-Link pyOCD)`（`launch.json` 由 CMake 从 `.vscode/launch.json.in` 生成）。

## Web

板子拿到 IP 后打开 `http://<ip>/`。BQ25756 调试台（充电启停、配置、ADC 曲线、状态/故障锁存）。ECharts 从 jsDelivr 加载，电脑需能上网。SHP8808 驱动库已落地（`src/modules/power/shp8808/`），风格对齐 BQ25756，调试台页签仍预留。
