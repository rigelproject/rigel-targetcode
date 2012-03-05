SECTIONS
{
  . = 0x0000;
  .text : {
     *(.text)
     . = . + 0x8;
  } = 0x0000
  .gnu.linkonce.t.__rigel_premain : {
     *(.gnu.linkonce.t.__rigel_premain)
     . = . + 0x8;
  } = 0x0000
  .libpar ALIGN(32): { *(.libpar) } 
  .rigellocks ALIGN(32): { *(.rigellocks) } 
  .data ALIGN(32): { *(.data) }
  .bss ALIGN(32): { *(.bss) }
  . = ALIGN(32);
  _end = .;
}

