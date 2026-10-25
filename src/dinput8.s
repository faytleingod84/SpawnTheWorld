.text
.extern ProxyResolve
.globl ProxyDispatch
.seh_proc ProxyDispatch
ProxyDispatch:
    subq $0xa8, %rsp
    .seh_stackalloc 0xa8
    .seh_endprologue
    movq %rcx, 0x20(%rsp)
    movq %rdx, 0x28(%rsp)
    movq %r8, 0x30(%rsp)
    movq %r9, 0x38(%rsp)
    movdqu %xmm0, 0x40(%rsp)
    movdqu %xmm1, 0x50(%rsp)
    movdqu %xmm2, 0x60(%rsp)
    movdqu %xmm3, 0x70(%rsp)
    movl %r10d, %ecx
    callq ProxyResolve
    movq %rax, %r11
    movq 0x20(%rsp), %rcx
    movq 0x28(%rsp), %rdx
    movq 0x30(%rsp), %r8
    movq 0x38(%rsp), %r9
    movdqu 0x40(%rsp), %xmm0
    movdqu 0x50(%rsp), %xmm1
    movdqu 0x60(%rsp), %xmm2
    movdqu 0x70(%rsp), %xmm3
    addq $0xa8, %rsp
    testq %r11, %r11
    jz ProxyMissingExport
    jmpq *%r11
ProxyMissingExport:
    xorl %eax, %eax
    retq
.seh_endproc

.globl DirectInput8Create
DirectInput8Create:
    movl $0, %r10d
    jmp ProxyDispatch

.globl DllCanUnloadNow
DllCanUnloadNow:
    movl $1, %r10d
    jmp ProxyDispatch

.globl DllGetClassObject
DllGetClassObject:
    movl $2, %r10d
    jmp ProxyDispatch

.globl DllRegisterServer
DllRegisterServer:
    movl $3, %r10d
    jmp ProxyDispatch

.globl DllUnregisterServer
DllUnregisterServer:
    movl $4, %r10d
    jmp ProxyDispatch

.globl GetdfDIJoystick
GetdfDIJoystick:
    movl $5, %r10d
    jmp ProxyDispatch
