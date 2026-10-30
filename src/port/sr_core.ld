/* Second Reality: sdk/ld/core_ram_emu.ld plus the pack references
 * (sr_init_array) and the parts' memory (build/gen/sr_overlays*.ld). */

INCLUDE gnw_ram_emu.ld

MEMORY
{
  /* (xrw) matches reality: the image executes from writable AXI SRAM. */
  RAM_EMU (xrw) : ORIGIN = __RAM_EMU_START__, LENGTH = __RAM_EMU_LENGTH__
}

PHDRS
{
  text PT_LOAD FLAGS(5); /* PF_R | PF_X */
  data PT_LOAD FLAGS(6); /* PF_R | PF_W */
}

SECTIONS
{
  /* ------------------------------------------------------------------ */
  /* Segment 0 payload (file-backed): entry + text/rodata + init_array  */
  /* ------------------------------------------------------------------ */
  .core_entry __RAM_EMU_START__ :
  {
    /* Offset 0: Thumb trampoline — firmware jumps here blindly. */
    KEEP(*(.core_entry))

    /* ABI bake-ins for pack_core.py (KEEP: otherwise --gc-sections drops). */
    KEEP(*(.gw_core_bridge_probe))

    . = ALIGN(4);
    *(.text*)
    *(.rodata*)
    *(.glue_7)
    *(.glue_7t)

    /* Nested in .core_entry so packaging needs no third --only-section. */
    . = ALIGN(4);
    __CORE_INIT_ARRAY_START__ = .;
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    __CORE_INIT_ARRAY_END__ = .;

    /* build/gen/sr_flash_refs.cpp: bound by sr_bind_data() once the pack is open */
    . = ALIGN(4);
    __start_sr_init_array = .;
    KEEP(*(sr_init_array))
    __stop_sr_init_array = .;
  } >RAM_EMU :text

  /* ------------------------------------------------------------------ */
  /* Initialized data (file-backed). May be empty — do not ALIGN before. */
  /* ------------------------------------------------------------------ */
  .data :
  {
    INCLUDE sr_overlays_data.ld
    *(.data*)
  } >RAM_EMU :data

  /* End of bytes stored in the CORE .bin for segment 0. */
  __CORE_CODE_END__ = .;

  /* ------------------------------------------------------------------ */
  /* BSS (NOT in the .bin). Firmware zeroes [__CORE_CODE_END__, here).  */
  /* ------------------------------------------------------------------ */
  /* the parts' variables first (an input section goes to the first
     output section that matches it) */
  INCLUDE sr_overlays.ld

  .bss (NOLOAD) :
  {
    . = ALIGN(8);
    *(.bss*)
    *(COMMON)
    . = ALIGN(8);
  } >RAM_EMU :data

  __CORE_BSS_END__ = .;

  ASSERT(__CORE_BSS_END__ <= __RAM_EMU_END__,
         "Error: core code+data+bss overflows RAM_EMU")

  /* Empty unwind table bounds (Stella / -lstdc++ may still reference). */
  __exidx_start = .;
  __exidx_end = .;

  /DISCARD/ :
  {
    *(.fini_array*)
    *(.preinit_array*)
    *(.ARM.exidx*)
    *(.ARM.extab*)
    *(.comment)
    *(.ARM.attributes)
  }
}
