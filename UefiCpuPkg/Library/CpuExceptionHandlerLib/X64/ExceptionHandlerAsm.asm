;------------------------------------------------------------------------------ ;
; Copyright (c) 2012, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
; Module Name:
;
;   ExceptionHandlerAsm.Asm
;
; Abstract:
;
;   x64 CPU Exception Handler
;
; Notes:
;
;------------------------------------------------------------------------------

;
; CommonExceptionHandler()
;
CommonExceptionHandler             PROTO   C

EXTRN mErrorCodeFlag:DWORD ; Error code flags for exceptions

data SEGMENT

CommonEntryAddr                dq      CommonInterruptEntry;

.code

Exception0Handle:
    push    0
    jmp     qword ptr [CommonEntryAddr]
Exception1Handle:
    push    1
    jmp     qword ptr [CommonEntryAddr]
Exception2Handle:
    push    2
    jmp     qword ptr [CommonEntryAddr]
Exception3Handle:
    push    3
    jmp     qword ptr [CommonEntryAddr]
Exception4Handle:
    push    4
    jmp     qword ptr [CommonEntryAddr]
Exception5Handle:
    push    5
    jmp     qword ptr [CommonEntryAddr]
Exception6Handle:
    push    6
    jmp     qword ptr [CommonEntryAddr]
Exception7Handle:
    push    7
    jmp     qword ptr [CommonEntryAddr]
Exception8Handle:
    push    8
    jmp     qword ptr [CommonEntryAddr]
Exception9Handle:
    push    9
    jmp     qword ptr [CommonEntryAddr]
Exception10Handle:
    push    10
    jmp     qword ptr [CommonEntryAddr]
Exception11Handle:
    push    11
    jmp     qword ptr [CommonEntryAddr]
Exception12Handle:
    push    12
    jmp     qword ptr [CommonEntryAddr]
Exception13Handle:
    push    13
    jmp     qword ptr [CommonEntryAddr]
Exception14Handle:
    push    14
    jmp     qword ptr [CommonEntryAddr]
Exception15Handle:
    push    15
    jmp     qword ptr [CommonEntryAddr]
Exception16Handle:
    push    16
    jmp     qword ptr [CommonEntryAddr]
Exception17Handle:
    push    17
    jmp     qword ptr [CommonEntryAddr]
Exception18Handle:
    push    18
    jmp     qword ptr [CommonEntryAddr]
Exception19Handle:
    push    19
    jmp     qword ptr [CommonEntryAddr]
Exception20Handle:
    push    20
    jmp     qword ptr [CommonEntryAddr]
Exception21Handle:
    push    21
    jmp     qword ptr [CommonEntryAddr]
Exception22Handle:
    push    22
    jmp     qword ptr [CommonEntryAddr]
Exception23Handle:
    push    23
    jmp     qword ptr [CommonEntryAddr]
Exception24Handle:
    push    24
    jmp     qword ptr [CommonEntryAddr]
Exception25Handle:
    push    25
    jmp     qword ptr [CommonEntryAddr]
Exception26Handle:
    push    26
    jmp     qword ptr [CommonEntryAddr]
Exception27Handle:
    push    27
    jmp     qword ptr [CommonEntryAddr]
Exception28Handle:
    push    28
    jmp     qword ptr [CommonEntryAddr]
Exception29Handle:
    push    29
    jmp     qword ptr [CommonEntryAddr]
Exception30Handle:
    push    30
    jmp     qword ptr [CommonEntryAddr]
Exception31Handle:
    push    31
    jmp     qword ptr [CommonEntryAddr]

;CommonInterruptEntrypoint:
;---------------------------------------;
; _CommonEntry                  ;
;----------------------------------------------------------------------------;
; The follow algorithm is used for the common interrupt routine.
; Entry from each interrupt with a push eax and eax=interrupt number

;---------------------------------------;
; CommonInterruptEntry                  ;
;---------------------------------------;
; The follow algorithm is used for the common interrupt routine.

CommonInterruptEntry PROC PUBLIC  
    cli
    ;
    ; All interrupt handlers are invoked through interrupt gates, so
    ; IF flag automatically cleared at the entry point
    ;
    ;
    ; Calculate vector number
    ;
    xchg    rcx, [rsp] ; get the return address of call, actually, it is the address of vector number.
    cmp     ecx, 32         ; Intel reserved vector for exceptions?
    jae     NoErrorCode
    bt      mErrorCodeFlag, ecx
    jc      @F

NoErrorCode:

    ;
    ; Push a dummy error code on the stack
    ; to maintain coherent stack map
    ;
    push    [rsp]
    mov     qword ptr [rsp + 8], 0
@@:       
    push    rbp
    mov     rbp, rsp

    ;
    ; Stack:
    ; +---------------------+ <-- 16-byte aligned ensured by processor
    ; +    Old SS           +
    ; +---------------------+
    ; +    Old RSP          +
    ; +---------------------+
    ; +    RFlags           +
    ; +---------------------+
    ; +    CS               +
    ; +---------------------+
    ; +    RIP              +
    ; +---------------------+
    ; +    Error Code       +
    ; +---------------------+
    ; + RCX / Vector Number +
    ; +---------------------+
    ; +    RBP              +
    ; +---------------------+ <-- RBP, 16-byte aligned
    ;


    ;
    ; Since here the stack pointer is 16-byte aligned, so
    ; EFI_FX_SAVE_STATE_X64 of EFI_SYSTEM_CONTEXT_x64
    ; is 16-byte aligned
    ;

;; UINT64  Rdi, Rsi, Rbp, Rsp, Rbx, Rdx, Rcx, Rax;
;; UINT64  R8, R9, R10, R11, R12, R13, R14, R15;
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rax
    push qword ptr [rbp + 8]   ; RCX
    push rdx
    push rbx
    push qword ptr [rbp + 48]  ; RSP
    push qword ptr [rbp]       ; RBP
    push rsi
    push rdi

;; UINT64  Gs, Fs, Es, Ds, Cs, Ss;  insure high 16 bits of each is zero
    movzx   rax, word ptr [rbp + 56]
    push    rax                      ; for ss
    movzx   rax, word ptr [rbp + 32]
    push    rax                      ; for cs
    mov     rax, ds
    push    rax
    mov     rax, es
    push    rax
    mov     rax, fs
    push    rax
    mov     rax, gs
    push    rax

    mov     [rbp + 8], rcx               ; save vector number

;; UINT64  Rip;
    push    qword ptr [rbp + 24]

;; UINT64  Gdtr[2], Idtr[2];
    xor     rax, rax
    push    rax
    push    rax
    sidt    [rsp]
    xchg    rax, [rsp + 2]
    xchg    rax, [rsp]
    xchg    rax, [rsp + 8]

    xor     rax, rax
    push    rax
    push    rax
    sgdt    [rsp]
    xchg    rax, [rsp + 2]
    xchg    rax, [rsp]
    xchg    rax, [rsp + 8]

;; UINT64  Ldtr, Tr;
    xor     rax, rax
    str     ax
    push    rax
    sldt    ax
    push    rax

;; UINT64  RFlags;
    push    qword ptr [rbp + 40]

;; UINT64  Cr0, Cr1, Cr2, Cr3, Cr4, Cr8;
    mov     rax, cr8
    push    rax
    mov     rax, cr4
    or      rax, 208h
    mov     cr4, rax
    push    rax
    mov     rax, cr3
    push    rax
    mov     rax, cr2
    push    rax
    xor     rax, rax
    push    rax
    mov     rax, cr0
    push    rax

;; UINT64  Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
    mov     rax, dr7
    push    rax
    mov     rax, dr6
    push    rax
    mov     rax, dr3
    push    rax
    mov     rax, dr2
    push    rax
    mov     rax, dr1
    push    rax
    mov     rax, dr0
    push    rax

;; FX_SAVE_STATE_X64 FxSaveState;
    sub rsp, 512
    mov rdi, rsp
    db 0fh, 0aeh, 07h ;fxsave [rdi]

;; UEFI calling convention for x64 requires that Direction flag in EFLAGs is clear
    cld

;; UINT32  ExceptionData;
    push    qword ptr [rbp + 16]

;; Prepare parameter and call
    mov     rcx, [rbp + 8]
    mov     rdx, rsp
    ;
    ; Per X64 calling convention, allocate maximum parameter stack space
    ; and make sure RSP is 16-byte aligned
    ;
    sub     rsp, 4 * 8 + 8
    mov     rax, CommonExceptionHandler
    call    rax
    add     rsp, 4 * 8 + 8

    cli
;; UINT64  ExceptionData;
    add     rsp, 8

;; FX_SAVE_STATE_X64 FxSaveState;

    mov rsi, rsp
    db 0fh, 0aeh, 0Eh ; fxrstor [rsi]
    add rsp, 512

;; UINT64  Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
;; Skip restoration of DRx registers to support in-circuit emualators
;; or debuggers set breakpoint in interrupt/exception context
    add     rsp, 8 * 6

;; UINT64  Cr0, Cr1, Cr2, Cr3, Cr4, Cr8;
    pop     rax
    mov     cr0, rax
    add     rsp, 8   ; not for Cr1
    pop     rax
    mov     cr2, rax
    pop     rax
    mov     cr3, rax
    pop     rax
    mov     cr4, rax
    pop     rax
    mov     cr8, rax

;; UINT64  RFlags;
    pop     qword ptr [rbp + 40]

;; UINT64  Ldtr, Tr;
;; UINT64  Gdtr[2], Idtr[2];
;; Best not let anyone mess with these particular registers...
    add     rsp, 48

;; UINT64  Rip;
    pop     qword ptr [rbp + 24]

;; UINT64  Gs, Fs, Es, Ds, Cs, Ss;
    pop     rax
    ; mov     gs, rax ; not for gs
    pop     rax
    ; mov     fs, rax ; not for fs
    ; (X64 will not use fs and gs, so we do not restore it)
    pop     rax
    mov     es, rax
    pop     rax
    mov     ds, rax
    pop     qword ptr [rbp + 32]  ; for cs
    pop     qword ptr [rbp + 56]  ; for ss

;; UINT64  Rdi, Rsi, Rbp, Rsp, Rbx, Rdx, Rcx, Rax;
;; UINT64  R8, R9, R10, R11, R12, R13, R14, R15;
    pop     rdi
    pop     rsi
    add     rsp, 8               ; not for rbp
    pop     qword ptr [rbp + 48] ; for rsp
    pop     rbx
    pop     rdx
    pop     rcx
    pop     rax
    pop     r8
    pop     r9
    pop     r10
    pop     r11
    pop     r12
    pop     r13
    pop     r14
    pop     r15

    mov     rsp, rbp
    pop     rbp
    add     rsp, 16
    iretq

CommonInterruptEntry ENDP

;-------------------------------------------------------------------------------------
;  GetTemplateAddressMap (&AddressMap);
;-------------------------------------------------------------------------------------
; comments here for definition of address map
GetTemplateAddressMap   PROC
        mov         rax, offset Exception0Handle
        mov         qword ptr [rcx], rax
        mov         qword ptr [rcx+8h], Exception1Handle - Exception0Handle
        ret
GetTemplateAddressMap   ENDP

END
