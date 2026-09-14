#include "board.h"

#include "board_config.h"
#include "tx_api.h"

UART_HandleTypeDef huart3;
I2C_HandleTypeDef hi2c1;

static uint8_t tx_tick_ready;

void board_tick_use_threadx(void)
{
    tx_tick_ready = 1U;
}

uint32_t HAL_GetTick(void)
{
    if (tx_tick_ready != 0U)
        return (uint32_t)tx_time_get();
    return uwTick;
}

int board_clock_init(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* VOS0 overdrive lives in SYSCFG->PWRCR (ODEN). */
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
    }

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_BYPASS;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 1;
    osc.PLL.PLLN = 120; /* 8 MHz * 120 / 2 = 480 MHz SYSCLK */
    osc.PLL.PLLP = 2;
    osc.PLL.PLLQ = 4;
    osc.PLL.PLLR = 2;
    osc.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
    osc.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    osc.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        return -1;

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                    RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.SYSCLKDivider = RCC_SYSCLK_DIV1;
    clk.AHBCLKDivider = RCC_HCLK_DIV2; /* AXI/AHB 240 MHz (H753 max) */
    clk.APB3CLKDivider = RCC_APB3_DIV2;
    clk.APB1CLKDivider = RCC_APB1_DIV2;
    clk.APB2CLKDivider = RCC_APB2_DIV2;
    clk.APB4CLKDivider = RCC_APB4_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK)
        return -1;

    __HAL_RCC_CSI_ENABLE();
    HAL_EnableCompensationCell();

    __HAL_RCC_D2SRAM1_CLK_ENABLE();
    __HAL_RCC_D2SRAM2_CLK_ENABLE();
    __HAL_RCC_D2SRAM3_CLK_ENABLE();

    /*
     * I-Cache on, D-Cache off: fetch from flash is fast; DMA/ETH/I2C buffers
     * stay coherent without MPU regions. Do not call SCB_EnableDCache().
     */
    SCB_InvalidateICache();
    SCB_EnableICache();
    return 0;
}

int board_console_open(bool dma)
{
    GPIO_InitTypeDef gpio = {0};

    (void)dma;
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &gpio);

    huart3.Instance = USART3;
    huart3.Init.BaudRate = BOARD_CONSOLE_BAUD;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    return HAL_UART_Init(&huart3) == HAL_OK ? 0 : -1;
}

int board_console_write(const uint8_t *data, uint32_t size)
{
    if (data == 0 && size != 0U)
        return -1;
    if (size == 0U)
        return 0;
    return HAL_UART_Transmit(&huart3, (uint8_t *)data, (uint16_t)size, 100U) == HAL_OK ? (int)size : -1;
}

int board_led_open(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_14;
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_1;
    HAL_GPIO_Init(GPIOE, &gpio);

    board_led_off(1);
    board_led_off(2);
    board_led_off(3);
    return 0;
}

void board_led_on(unsigned id)
{
    if (id == 1U)
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
    else if (id == 2U)
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
    else if (id == 3U)
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
}

void board_led_off(unsigned id)
{
    if (id == 1U)
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    else if (id == 2U)
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET);
    else if (id == 3U)
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
}

void board_led1_toggle(void)
{
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
}

int board_i2c_open(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_AF_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &gpio);

    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x307075B1U; /* ~100 kHz at 120 MHz PCLK1 */
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
        return -1;
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
        return -1;
    return HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) == HAL_OK ? 0 : -1;
}

int board_i2c_write(uint8_t addr7, const uint8_t *data, uint32_t size, uint32_t timeout_ms)
{
    if (data == 0 && size != 0U)
        return -1;
    return HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(addr7 << 1), (uint8_t *)data, (uint16_t)size, timeout_ms) ==
                   HAL_OK
               ? 0
               : -1;
}

int board_i2c_write_read(uint8_t addr7, const uint8_t *wdata, uint32_t wsize, uint8_t *rdata, uint32_t rsize,
                         uint32_t timeout_ms)
{
    if (wdata == 0 || rdata == 0 || wsize != 1U)
        return -1;
    return HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(addr7 << 1), wdata[0], I2C_MEMADD_SIZE_8BIT, rdata, (uint16_t)rsize,
                            timeout_ms) == HAL_OK
               ? 0
               : -1;
}

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
}
