ENTRY(test_main)
MEMORY {
  ROM (rx) : ORIGIN = 0x10000, LENGTH = 128K
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}
SECTIONS {
  .text : { *(.text*) *(.rodata*) } > ROM
  .data : { *(.data*) } > RAM
  .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) }
}
