PUBLIC  NLoadImageDetour
PUBLIC  NLoadFileDetour
PUBLIC  AssemblyNativeLoadFromBufferDetour
PUBLIC  ClrJitCompileMethodDetour

EXTRN   ClrNative_NLoadImage_Inspect:PROC
EXTRN   ClrNative_NLoadFile_Inspect:PROC
EXTRN   ClrNative_LoadFromBuffer_Inspect:PROC
EXTRN   ClrJit_CompileMethod_Inspect:PROC

EXTRN   OrgNLoadImage:QWORD
EXTRN   OrgNLoadFile:QWORD
EXTRN   OrgAssemblyNativeLoadFromBuffer:QWORD
EXTRN   OrgClrJitCompileMethod:QWORD

DETOUR_FRAME_SIZE EQU 128h

OFF_RCX   EQU 40h
OFF_RDX   EQU 48h
OFF_R8    EQU 50h
OFF_R9    EQU 58h
OFF_RAX   EQU 60h
OFF_R10   EQU 68h
OFF_R11   EQU 70h
OFF_XMM0  EQU 78h
OFF_XMM1  EQU 88h
OFF_XMM2  EQU 98h
OFF_XMM3  EQU 0A8h
OFF_XMM4  EQU 0B8h
OFF_XMM5  EQU 0C8h
OFF_RBX   EQU 0D8h
OFF_RBP   EQU 0E0h
OFF_RDI   EQU 0E8h
OFF_RSI   EQU 0F0h
OFF_R12   EQU 0F8h
OFF_R13   EQU 100h
OFF_R14   EQU 108h
OFF_R15   EQU 110h

STK_ARG5  EQU 20h

SAVE_DETOUR_CONTEXT MACRO
    mov     QWORD PTR [rsp+OFF_RCX], rcx
    mov     QWORD PTR [rsp+OFF_RDX], rdx
    mov     QWORD PTR [rsp+OFF_R8], r8
    mov     QWORD PTR [rsp+OFF_R9], r9
    mov     QWORD PTR [rsp+OFF_RAX], rax
    mov     QWORD PTR [rsp+OFF_R10], r10
    mov     QWORD PTR [rsp+OFF_R11], r11
    
    movups  XMMWORD PTR [rsp+OFF_XMM0], xmm0
    movups  XMMWORD PTR [rsp+OFF_XMM1], xmm1
    movups  XMMWORD PTR [rsp+OFF_XMM2], xmm2
    movups  XMMWORD PTR [rsp+OFF_XMM3], xmm3
    movups  XMMWORD PTR [rsp+OFF_XMM4], xmm4
    movups  XMMWORD PTR [rsp+OFF_XMM5], xmm5

    mov     QWORD PTR [rsp+OFF_RBX], rbx
    mov     QWORD PTR [rsp+OFF_RBP], rbp
    mov     QWORD PTR [rsp+OFF_RDI], rdi
    mov     QWORD PTR [rsp+OFF_RSI], rsi
    mov     QWORD PTR [rsp+OFF_R12], r12
    mov     QWORD PTR [rsp+OFF_R13], r13
    mov     QWORD PTR [rsp+OFF_R14], r14
    mov     QWORD PTR [rsp+OFF_R15], r15
ENDM

RESTORE_DETOUR_CONTEXT MACRO
    mov     rcx, QWORD PTR [rsp+OFF_RCX]
    mov     rdx, QWORD PTR [rsp+OFF_RDX]
    mov     r8,  QWORD PTR [rsp+OFF_R8]
    mov     r9,  QWORD PTR [rsp+OFF_R9]
    mov     rax, QWORD PTR [rsp+OFF_RAX]
    mov     r10, QWORD PTR [rsp+OFF_R10]
    mov     r11, QWORD PTR [rsp+OFF_R11]

    movups  xmm0, XMMWORD PTR [rsp+OFF_XMM0]
    movups  xmm1, XMMWORD PTR [rsp+OFF_XMM1]
    movups  xmm2, XMMWORD PTR [rsp+OFF_XMM2]
    movups  xmm3, XMMWORD PTR [rsp+OFF_XMM3]
    movups  xmm4, XMMWORD PTR [rsp+OFF_XMM4]
    movups  xmm5, XMMWORD PTR [rsp+OFF_XMM5]
    mov     rbx, QWORD PTR [rsp+OFF_RBX]
    mov     rbp, QWORD PTR [rsp+OFF_RBP]
    mov     rdi, QWORD PTR [rsp+OFF_RDI]
    mov     rsi, QWORD PTR [rsp+OFF_RSI]
    mov     r12, QWORD PTR [rsp+OFF_R12]
    mov     r13, QWORD PTR [rsp+OFF_R13]
    mov     r14, QWORD PTR [rsp+OFF_R14]
    mov     r15, QWORD PTR [rsp+OFF_R15]

ENDM

CALL_INSPECT_4 MACRO InspectFn
    mov     rcx, QWORD PTR [rsp+OFF_RCX]
    mov     rdx, QWORD PTR [rsp+OFF_RDX]
    mov     r8,  QWORD PTR [rsp+OFF_R8]
    mov     r9,  QWORD PTR [rsp+OFF_R9]
    call    InspectFn
ENDM

DETOUR_BODY MACRO InspectFn, OrgPtr
    sub     rsp, DETOUR_FRAME_SIZE


    .allocstack DETOUR_FRAME_SIZE
    .endprolog

    SAVE_DETOUR_CONTEXT
    CALL_INSPECT_4 InspectFn
    RESTORE_DETOUR_CONTEXT
    add     rsp, DETOUR_FRAME_SIZE
    jmp     QWORD PTR [OrgPtr]


ENDM

.CODE

NLoadImageDetour PROC FRAME

    DETOUR_BODY ClrNative_NLoadImage_Inspect, OrgNLoadImage
NLoadImageDetour ENDP

NLoadFileDetour PROC FRAME

    DETOUR_BODY ClrNative_NLoadFile_Inspect, OrgNLoadFile
NLoadFileDetour ENDP

AssemblyNativeLoadFromBufferDetour PROC FRAME

    sub     rsp, DETOUR_FRAME_SIZE
    .allocstack DETOUR_FRAME_SIZE
    .endprolog

    SAVE_DETOUR_CONTEXT
    mov     rax, QWORD PTR [rsp+DETOUR_FRAME_SIZE+28h]
    mov     QWORD PTR [rsp+STK_ARG5], rax

    CALL_INSPECT_4 ClrNative_LoadFromBuffer_Inspect

    RESTORE_DETOUR_CONTEXT
    add     rsp, DETOUR_FRAME_SIZE
    jmp     QWORD PTR [OrgAssemblyNativeLoadFromBuffer]

AssemblyNativeLoadFromBufferDetour ENDP

ClrJitCompileMethodDetour PROC FRAME

    DETOUR_BODY ClrJit_CompileMethod_Inspect, OrgClrJitCompileMethod
ClrJitCompileMethodDetour ENDP

END
