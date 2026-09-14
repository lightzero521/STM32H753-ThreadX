#include "tx_api.h"

#include "board.h"

int main(void)
{
    (void)HAL_Init();
    (void)board_clock_init();
    tx_kernel_enter();
    return 0;
}
