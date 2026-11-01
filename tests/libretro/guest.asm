; Test guest: a boot sector that switches to VGA mode 13h (320x200) and
; paints the top half red and the bottom half green, then halts.
; Build: nasm -f bin -o guest.img guest.asm
bits 16
org 0x7c00

	mov ax, 0x0013		; set mode 13h
	int 0x10

	mov dx, 0x3c8		; DAC write index
	mov al, 1
	out dx, al
	inc dx			; DAC data: entry 1 = red, entry 2 = green
	mov al, 63
	out dx, al
	xor al, al
	out dx, al
	out dx, al
	out dx, al
	mov al, 63
	out dx, al
	xor al, al
	out dx, al

	mov ax, 0xa000
	mov es, ax
	xor di, di
	mov al, 1
	mov cx, 320 * 100
	rep stosb
	mov al, 2
	mov cx, 320 * 100
	rep stosb

halt:
	hlt
	jmp halt

	times 510 - ($ - $$) db 0
	dw 0xaa55
