org 0100h

    mov	ax, cs
	mov	ds, ax
	mov	es, ax
    mov bp, LoaderMessage
    mov cx, 13
    mov	ax, 01301h
    mov	bx, 004fh		; 页号为0(BH = 0) 红底白字(BL = 4fh)
    mov dx, 0810h
    int 10h
    jmp $                               ; 到此停住

    LoaderMessage db "Hello Loader!"