; nasm -felf32 div.asm
; ld -m elf_i386 div.o
; ./a.out
SECTION .data
message1 db "Please input two numbers:", 0h
message2 db "Error! The divisor is zero!", 0h
message3 db "The results of quotient and remainder are:", 0h

SECTION .bss
input_string resb 255
first_string resb 127   ; 余数最终在被除数空间
second_string resb 127
quo_result resb 127     ; 商
first_len resb 1
second_len resb 1

SECTION .text
global _start

; 得到字符串长度
; 传入参数为字符串首地址，保存在 eax 中；返回值为字符串长度，保存在 eax 中
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

; 输出一个字符串
; 通过80h中断：eax 设为 4(write)；ebx 设为1(stdout)；ecx 为输入区地址；edx 为输出长度
; 传入参数为输入区地址，在eax
puts:
    push edx
    push ecx
    push ebx
    push eax

    mov ecx, eax
    call strlen
    mov edx, eax
    mov ebx, 1
    mov eax, 4
    int 80h

    pop eax
    pop ebx
    pop ecx
    pop edx
    ret

; 读入一个字符串
; 通过80h中断：eax 设为 3(read)；ebx 设为0(stdin)；ecx 为输入区地址；edx 为输入区大小
; 传入参数为输入区地址，在eax；输入区大小，在ebx
getline:
    push edx
    push ecx
    push ebx
    push eax

    mov edx, ebx
    mov ecx, eax
    mov ebx, 0
    mov eax, 3
    int 80h

    pop eax
    pop ebx
    pop ecx
    pop edx

    ret

; 打印换行
endl:
    push eax
    mov eax, 0Ah
    call putchar
    pop eax
    ret

; 输出一个字符
; 参数保存在 eax 中
putchar:
    push edx
    push ecx
    push ebx
    push eax

    mov eax, 4
    mov ebx, 1
    mov ecx, esp
    mov edx, 1
    int 80h

    pop eax
    pop ebx
    pop ecx
    pop edx

    ret

; 将一个字符串通过一个空格分割，得到两个字符串
; 传入为要分割字符串首地址，在 ecx 中；分割后字符串首地址，在 eax 中
; 注意：ecx 被更改
parse_input:
    push eax
.loop:
    cmp BYTE[ecx], 32   ; 32 为空格的 ASCII 码
    jz .rett
    cmp BYTE[ecx], 10   ; 10(0Ah) 为换行的 ASCII 码
    jz .rett
    mov dl, BYTE[ecx]
    mov BYTE[eax], dl
    inc eax
    inc ecx
    jmp .loop

.rett:
    inc ecx
    pop eax
    ret

; 执行减法
; 参数保存在 esi 和 edi 中，分别为被减数和减数字符串首地址
sub_digit:
    push esi
    push edi
    push eax
    push ebx

    mov edi, second_string
    mov al, BYTE[second_len]
    and eax, 00ffh
    add esi, eax
    add edi, eax

    .subLoop:
        cmp al, 0
        jz .sub_ret
        dec al
        dec esi
        dec edi
        mov bl, [edi]
        add BYTE[esi], 48
        sub BYTE[esi], bl
        cmp BYTE[esi], 48
        jnl .subLoop
        add BYTE[esi], 10
        mov ebx, esi
        dec ebx
        sub BYTE[ebx], 1
        jmp .subLoop

.sub_ret:
    pop ebx
    pop eax
    pop edi
    pop esi
    ret

; 输出没有前置 '0' 的字符串，如果为多个 '0'，则仅输出一个 '0'
; 传入参数在 eax 中，为字符串首地址
printf:
    push eax
    push ebx

.printfLoop:
    mov bl, BYTE[eax]
    cmp bl, 48
    jnz .printf_res
    inc eax
    mov bh, BYTE[eax]
    cmp bh, 0
    jnz .printfLoop
    mov eax, 48
    call putchar
    jmp .printf_ret

.printf_res:
    call puts

.printf_ret:
    pop ebx
    pop eax
    ret


_start:
    mov eax, message1
    call puts
    call endl

    mov eax, input_string
    mov ebx, 255
    call getline

    mov ecx, input_string
    mov eax, first_string
    mov BYTE[eax], 48
    inc eax
    call parse_input
    mov eax, second_string          ; 得到被除数和除数并放在 first_string 和 second_string 中
    call parse_input                ; 为了方便书写后面代码，在被除数前加了一个字符 0

.getLength:
    mov eax, first_string
    call strlen
    dec al
    mov BYTE[first_len], al         ; 得到被除数长度，注意减去加上的字符 0
    mov eax, second_string
    call strlen
    mov BYTE[second_len], al        ; 得到除数长度
    mov ebx, second_string          
.loop:                              ; 判断除数是否为 0
    cmp al, 0
    jz .isZero
    cmp BYTE[ebx], 48
    jnz .division
    inc ebx
    dec al
    jmp .loop

.division:                          ; 除法过程
    mov esi, first_string           ; esi 指向被除数字符串
    mov edi, second_string          ; edi 指向除数字符串
    mov ecx, quo_result             ; ecx 指向商字符串
    dec ecx                         ; 方便 .outLoop 循环体代码书写
    mov al, BYTE[first_len]
    sub al, BYTE[second_len]
    inc al
    cmp al, 0
    jle .isShort                    ; 被除数小于除数，跳转
    
    .outLoop:                       ; 外层循环执行 al 次，al 最初为商的长度
        cmp al, 0                   ; 每循环一次，esi 前移一次，ecx 前移一次
        jz .print
        dec al
        inc esi
        inc ecx
        add BYTE[ecx], 48           ; 设置初始值 48，即 '0' 的ASCII值

    .isEnough:                      ; 判断是否够减
        mov edi, second_string
        dec edi
        mov ebx, esi
        dec ebx
        cmp BYTE[ebx], 48
        jg .callSub                 ; 如果当前 esi 指向的前一位大于 0，则一定够减
        mov ah, BYTE[second_len]    ; 内层循环执行 ah 次，ah 最初为除数长度
        .inLoop:
            cmp ah, 0
            jz .callSub             ; 相等也是够减
            dec ah
            inc ebx
            inc edi
            mov dl, BYTE[ebx]
            cmp BYTE[edi], dl
            jg .outLoop             ; 不够减就跳到外层循环
            jl .callSub             ; 够减则执行减法
            jmp .inLoop        

    .callSub:                       ; 调用一次减法，并把当前商的位置加 1
        call sub_digit
        add BYTE[ecx], 1
        jmp .isEnough               ; 继续判断是否够减

.isShort:                           ; 被除数小于除数，把商置 0
    inc ecx
    add BYTE[ecx], 48

.print:                             ; 打印结果
    mov eax, message3
    call puts
    call endl
    mov eax, quo_result
    call printf                     ; 输出商
    mov eax, 32
    call putchar                    ; 输出空格
    mov eax, first_string
    call printf                     ; 输出余数
    call endl
    jmp .finish

.isZero:                            ; 除数为 0，输出错误信息
    mov eax, message2
    call puts
    call endl
.finish:                            ; 退出程序
    mov eax, 1
    xor ebx, ebx
    int 80h
    