/* $NetBSD: stand.ldscript.be,v 1.1.4.1 2000/09/03 22:42:37 soren Exp $ */

/*  ldscript for NetBSD/mipsbe stand-alone programs */
OUTPUT_FORMAT("elf32-bigmips", "elf32-bigmips",
	      "elf32-littlemips")
OUTPUT_ARCH(mips)
ENTRY(_start)
SECTIONS
{
  /*  Read-only sections, merged into text segment.  Assumes the
      stand Makefile sets the start address via -Ttext.  */
  .text      :
  {
    _ftext = . ;
    *(.text)
    *(.gnu.warning)
  } =0
  _etext = .;
  PROVIDE (etext = .);
  .rodata    : { *(.rodata)  }
  .data    :
  {
    _fdata = . ;
    *(.data)
    CONSTRUCTORS
  }
  _gp = ALIGN(16);
  .lit8 : { *(.lit8) }
  .lit4 : { *(.lit4) }
  .sdata     : { *(.sdata) }
  _edata  =  .;
  PROVIDE (edata = .);
  __bss_start = .;
  _fbss = .;
  .sbss      : { *(.sbss) *(.scommon) }
  .bss       :
  {
   *(.bss)
   *(COMMON)
  }
  _end = . ;
  PROVIDE (end = .);
}
