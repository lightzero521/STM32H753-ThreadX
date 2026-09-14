#include "nx_stm32_phy_driver.h"
#include "nx_stm32_eth_config.h"
#include "tx_api.h"

static int32_t lan8742_io_init(void);
static int32_t lan8742_io_deinit(void);
static int32_t lan8742_io_write_reg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal);
static int32_t lan8742_io_read_reg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal);
static int32_t lan8742_io_get_tick(void);

static lan8742_IOCtx_t LAN8742_IOCtx = {lan8742_io_init, lan8742_io_deinit, lan8742_io_write_reg, lan8742_io_read_reg,
                                        lan8742_io_get_tick};
static lan8742_Object_t LAN8742;

int32_t nx_eth_phy_init(void)
{
    uint32_t val = 0U;
    uint32_t t0;

    LAN8742_RegisterBusIO(&LAN8742, &LAN8742_IOCtx);
    if (LAN8742.IO.Init != 0 && LAN8742.IO.Init() != ETH_PHY_STATUS_OK)
        return ETH_PHY_STATUS_ERROR;

    /* NUCLEO-H753ZI straps LAN8742A at address 0. Skip the 0..31 scan. */
    LAN8742.DevAddr = 0U;
    if (lan8742_io_read_reg(0U, LAN8742_PHYI1R, &val) != ETH_PHY_STATUS_OK)
        return ETH_PHY_STATUS_ERROR;

    if (lan8742_io_write_reg(0U, LAN8742_BCR, LAN8742_BCR_SOFT_RESET) != ETH_PHY_STATUS_OK)
        return ETH_PHY_STATUS_ERROR;

    t0 = (uint32_t)HAL_GetTick();
    for (;;) {
        tx_thread_sleep(10);
        if (lan8742_io_read_reg(0U, LAN8742_BCR, &val) != ETH_PHY_STATUS_OK)
            return ETH_PHY_STATUS_ERROR;
        if ((val & LAN8742_BCR_SOFT_RESET) == 0U)
            break;
        if (((uint32_t)HAL_GetTick() - t0) > 500U)
            return ETH_PHY_STATUS_ERROR;
    }

    if (lan8742_io_write_reg(0U, LAN8742_BCR, LAN8742_BCR_AUTONEGO_EN) != ETH_PHY_STATUS_OK)
        return ETH_PHY_STATUS_ERROR;

    LAN8742.Is_Initialized = 1U;
    return ETH_PHY_STATUS_OK;
}

int32_t nx_eth_phy_set_link_state(int32_t LinkState)
{
    return LAN8742_SetLinkState(&LAN8742, LinkState);
}

int32_t nx_eth_phy_get_link_state(void)
{
    return LAN8742_GetLinkState(&LAN8742);
}

nx_eth_phy_handle_t nx_eth_phy_get_handle(void)
{
    return (nx_eth_phy_handle_t)&LAN8742;
}

int32_t lan8742_io_init(void)
{
    HAL_ETH_SetMDIOClockRange(&eth_handle);
    return ETH_PHY_STATUS_OK;
}

int32_t lan8742_io_deinit(void)
{
    return ETH_PHY_STATUS_OK;
}

int32_t lan8742_io_read_reg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal)
{
    if (HAL_ETH_ReadPHYRegister(&eth_handle, DevAddr, RegAddr, pRegVal) != HAL_OK)
        return ETH_PHY_STATUS_ERROR;
    return ETH_PHY_STATUS_OK;
}

int32_t lan8742_io_write_reg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal)
{
    if (HAL_ETH_WritePHYRegister(&eth_handle, DevAddr, RegAddr, RegVal) != HAL_OK)
        return ETH_PHY_STATUS_ERROR;
    return ETH_PHY_STATUS_OK;
}

int32_t lan8742_io_get_tick(void)
{
    return HAL_GetTick();
}
