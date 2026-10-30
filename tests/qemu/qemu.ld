/* QEMU mps2-an500: code in SSRAM1 (0x00000000, 4 MB), data / heap in PSRAM
 * (0x60000000, 16 MB), stack at the top of SSRAM2/3 (0x20000000, 4 MB). */
MEMORY
{
  CODE (rx)  : ORIGIN = 0x00000000, LENGTH = 4M
  STACK (rw) : ORIGIN = 0x20000000, LENGTH = 4M
  RAM (rw)   : ORIGIN = 0x60000000, LENGTH = 16M
}
__stack_top = ORIGIN(STACK) + LENGTH(STACK);
ENTRY(Reset_Handler)
SECTIONS
{
  .vectors : { KEEP(*(.vectors)) } > CODE
  .text : { *(.text*) *(.rodata*) KEEP(*(.init)) KEEP(*(.fini)) } > CODE
  .ARM.exidx : { *(.ARM.exidx*) } > CODE
  .init_array : { __init_array_start = .; KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)) __init_array_end = .; } > CODE
  .preinit_array : { __preinit_array_start = .; KEEP(*(.preinit_array*)) __preinit_array_end = .; } > CODE
  .fini_array : { __fini_array_start = .; KEEP(*(.fini_array*)) __fini_array_end = .; } > CODE
  sr_init_array : { __start_sr_init_array = .; KEEP(*(sr_init_array)) __stop_sr_init_array = .; } > CODE
  .data : { __data_start__ = .; *(.data*) __data_end__ = .; } > RAM
  .bss (NOLOAD) : { __bss_start__ = .; *(.bss*) *(COMMON) __bss_end__ = .; } > RAM
  end = .;
  __end__ = .;
  __HeapLimit = ORIGIN(RAM) + LENGTH(RAM);
}
