# Changelog

## 0.1.1

- SYSCLK 提到 480 MHz（VOS0，AXI/AHB 240 MHz）
- I-Cache 开、D-Cache 关，仍不配置 MPU
- HTTP `:80` BQ25756 电源调试台（ECharts CDN，异常锁存）
- 烧录/调试改为板载 ST-LINK + pyOCD（`stm32h743xx`）

## 0.1.0

- 按 GD32F503-ThreadX 模板搭出 NUCLEO-H753ZI 最小 ThreadX 工程
- SYSCLK 200 MHz（HSE bypass 8 MHz），不启用 I/D Cache，不配置 MPU
- ST-LINK VCP USART3 PD8/PD9 460800，上电打印 bring-up / 编译时间
- LD1/LD2/LD3 全部初始化，LD1 绿灯 2 Hz 闪烁
- 移植 `src/modules/power`（BQ25756），I2C1 PB8/PB9 100 kHz，HAL I2C
- NetX Duo + LAN8742 RMII HTTP 服务，静态页在 `web/`
