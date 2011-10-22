        section .bss
i:      resd 1
        section .text

        global main
main:
        call	f
        call	g
        call	f
        ret

        global f
f:
        push	ebp
        mov	esp, ebp
        sub	$8, esp
        call	h
        mov	eax, i
        ret
