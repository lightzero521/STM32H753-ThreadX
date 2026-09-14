ENTRY(Reset_Handler)

MEMORY
{
    FLASH  (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
    RAM    (xrw) : ORIGIN = 0x24000000, LENGTH = 512K
    RAM_D2 (xrw) : ORIGIN = 0x30000000, LENGTH = 288K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);
_sp = _estack;
__ram_end__ = _estack;

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text .text.*)
        *(.rodata .rodata.*)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH
    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH
    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH

    _sidata = LOADADDR(.data);
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data .data.*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(8);
        _ebss = .;
        __bss_end__ = .;
        __end__ = .;
        end = .;
        _end = .;
    } > RAM

    .eth_dma (NOLOAD) :
    {
        . = ALIGN(32);
        *(.RxDecripSection)
        *(.TxDecripSection)
        *(.RxArraySection)
        . = ALIGN(32);
    } > RAM_D2

    /DISCARD/ : { *(.note.GNU-stack) }
}

PROVIDE(__gVectors = g_pfnVectors);
ASSERT(_ebss <= _estack, "STM32H753 AXI SRAM overflow")
