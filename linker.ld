/*
 * Linker script for the Krytonium x86_64 kernel loaded by Limine.
 *
 * The kernel is placed in the higher half of virtual memory at the standard
 * Limine virtual base (-2 GiB).  Limine maps it there automatically.
 *
 * Build flags must include:
 *   -mcmodel=kernel  -mno-red-zone  -mno-mmx  -mno-sse  -mno-sse2
 */

ENTRY(_start)

SECTIONS
{
    /* Limine loads the kernel at the virtual base it negotiates, which for
     * a plain higher-half setup is 0xFFFFFFFF80000000 + phys_offset.
     * We link at 0xFFFFFFFF80200000 (2 MiB past the base) so there is room
     * for the Limine structures below us. */
    . = 0xFFFFFFFF80200000;

    .text : ALIGN(4096)
    {
        *(.text._start)
        *(.text*)
    }

    .rodata : ALIGN(4096)
    {
        *(.rodata*)
    }

    /* Limine request structures go in a dedicated section that the bootloader
     * scans for its magic IDs. */
    .requests : ALIGN(8)
    {
        __requests_start = .;
        *(.requests)
        __requests_end = .;
    }

    .data : ALIGN(4096)
    {
        *(.data*)
    }

    .bss : ALIGN(4096)
    {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }
}
