OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
    stack PT_GNU_STACK FLAGS(6);
}

SECTIONS
{
    . = 0x0000000040000000;

    .text : ALIGN(4K)
    {
        *(.text*)
    } :text

    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    } :text

    .eh_frame : ALIGN(4K)
    {
        *(.eh_frame*)
    } :text

    .data : ALIGN(4K)
    {
        *(.data*)
    } :data

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    } :data

    /* Stack - grows down, 64KB */
    . = ALIGN(16);
    __stack_top = .;
    . += 65536;

    /* Heap - grows up from here */
    __heap_start = .;
}