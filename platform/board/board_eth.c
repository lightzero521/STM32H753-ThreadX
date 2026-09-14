#include "board.h"

#include "board_config.h"
#include "tx_api.h"

ETH_HandleTypeDef heth;

ETH_DMADescTypeDef dma_rx_desc[ETH_RX_DESC_CNT] __attribute__((section(".RxDecripSection"), aligned(32)));
ETH_DMADescTypeDef dma_tx_desc[ETH_TX_DESC_CNT] __attribute__((section(".TxDecripSection"), aligned(32)));

static uint8_t mac_addr[6] = {0x02U, 0x80U, 0xE1U, 0x00U, 0x00U, 0x01U};

void HAL_ETH_MspInit(ETH_HandleTypeDef *eth)
{
    GPIO_InitTypeDef gpio = {0};

    if (eth->Instance != ETH)
        return;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_ETH1MAC_CLK_ENABLE();
    __HAL_RCC_ETH1TX_CLK_ENABLE();
    __HAL_RCC_ETH1RX_CLK_ENABLE();
    __HAL_RCC_ETH1MAC_FORCE_RESET();
    __HAL_RCC_ETH1MAC_RELEASE_RESET();

    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Alternate = GPIO_AF11_ETH;

    gpio.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &gpio);
    gpio.Pin = GPIO_PIN_13;
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOC, &gpio);
    gpio.Pin = GPIO_PIN_11 | GPIO_PIN_13;
    HAL_GPIO_Init(GPIOG, &gpio);

    HAL_NVIC_SetPriority(ETH_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(ETH_IRQn);
    /* RMII REF_CLK from LAN8742 must be live before HAL_ETH_Init writes DMAMR.SWR. */
    tx_thread_sleep(50);
}

void MX_ETH_Init(void)
{
    heth.Instance = ETH;
    heth.Init.MACAddr = mac_addr;
    heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
    heth.Init.TxDesc = dma_tx_desc;
    heth.Init.RxDesc = dma_rx_desc;
    heth.Init.RxBuffLen = 1536;
    (void)HAL_ETH_Init(&heth);
}

void ETH_IRQHandler(void)
{
    HAL_ETH_IRQHandler(&heth);
}
