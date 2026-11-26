MEMORY { ROM (rx) : ORIGIN = 0x00100000, LENGTH = 0x80000  RAM (rw) : ORIGIN = 0x00400000, LENGTH = 0x80000 }
SECTIONS {
  .text : { *(.text*) *(.rodata*) *(.srodata*) } > ROM
  .data : { *(.data*) *(.sdata*) } > RAM
  .bss : { *(.bss*) *(.sbss*) *(COMMON) } > RAM
  /DISCARD/ : { *(.comment) *(.note*) *(.riscv.attributes) }
}
