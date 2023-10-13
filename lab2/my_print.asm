global sprint           ; 对外的函数标签


section .data
red db 1bh, "[31m", 0
white db 1bh, "[0m", 0

section .text
; 求字符串长度
strlen:
    push ebx
    mov ebx, eax
.next:
    cmp BYTE[ebx], 0
    jz .finish
    inc ebx
    jmp .next
.finish:
    sub ebx, eax
    mov eax, ebx
    pop ebx
    ret

; sprint(const char* p, int color)
sprint:
    mov eax, [esp+8]    ; 取出 color
    cmp eax, 1          ; 1 代表红色，0 代表白色
    jz .setRed

.setWhite:              ; 设置当前颜色为白色
    mov eax, white
    call strlen
    mov edx, eax
    mov ecx, white
    mov ebx, 1      
    mov eax, 4
    int 80h
    jmp .print

.setRed:                ; 设置当前颜色为红色
    mov eax, red
    call strlen
    mov edx, eax
    mov ecx, red
    mov ebx, 1
    mov eax, 4
    int 80h

.print:                 ; 打印字符串
    mov eax, [esp+4]    ; 取出 p
    mov ecx, eax        ; ecx:字符串首地址
    call strlen
    mov edx, eax        ; edx:字符串长度
    mov ebx, 1          ; ebx:fd=1，对应stdout
    mov eax, 4          ; eax:系统调用号=4，对应sys_write
    int 80h

    ret


    


