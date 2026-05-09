; Krytonium x86_64 kernel entry point for Limine bootloader.
; Limine enters the kernel in 64-bit long mode with:
;   - Interrupts disabled (IF=0)
;   - A valid stack already provided
;   - Paging enabled, kernel mapped at virtual base
; We simply set up our own stack and call the C++ kernel_main.

bits 64
global _start
extern kernel_main

section .text

_start:
    cli                         ; ensure interrupts are off

    ; Install our own stack (Limine's stack is small)
    lea rsp, [rel stack_top]

    ; Zero the frame pointer to stop stack-unwinders from walking past us
    xor rbp, rbp

    ; BSS is already zeroed by Limine; call C++ entry
    call kernel_main

.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 65536          ; 64 KiB kernel stack
stack_top:

; Mark the stack as non-executable (suppresses the linker warning)
section .note.GNU-stack noalloc noexec nowrite progbits
