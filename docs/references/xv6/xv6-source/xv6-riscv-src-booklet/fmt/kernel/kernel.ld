8450 OUTPUT_ARCH( "riscv" )
8451 ENTRY( _entry )
8452 
8453 SECTIONS
8454 {
8455   /*
8456    * ensure that entry.S / _entry is at 0x80000000,
8457    * where qemu's -kernel jumps.
8458    */
8459   . = 0x80000000;
8460 
8461   .text : {
8462     kernel/entry.o(_entry)
8463     *(.text .text.*)
8464     . = ALIGN(0x1000);
8465     _trampoline = .;
8466     *(trampsec)
8467     . = ALIGN(0x1000);
8468     ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page");
8469     PROVIDE(etext = .);
8470   }
8471 
8472   .rodata : {
8473     . = ALIGN(16);
8474     *(.srodata .srodata.*) /* do not need to distinguish this from .rodata */
8475     . = ALIGN(16);
8476     *(.rodata .rodata.*)
8477   }
8478 
8479   .data : {
8480     . = ALIGN(16);
8481     *(.sdata .sdata.*) /* do not need to distinguish this from .data */
8482     . = ALIGN(16);
8483     *(.data .data.*)
8484   }
8485 
8486   .bss : {
8487     . = ALIGN(16);
8488     *(.sbss .sbss.*) /* do not need to distinguish this from .bss */
8489     . = ALIGN(16);
8490     *(.bss .bss.*)
8491   }
8492 
8493   PROVIDE(end = .);
8494 }
8495 
8496 
8497 
8498 
8499 
