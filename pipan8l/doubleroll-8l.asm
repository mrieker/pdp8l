
; roll AC / PC lights
; reverse direction every SR'th steps

; pdp8v/asm/assemble doubleroll-8l.asm doubleroll-8l.obj > doubleroll-8l.lis
; pdp8v/asm/link -o doubleroll-8l.oct doubleroll-8l.obj > doubleroll-8l.map
; pdp8v/asm/octtobin < doubleroll-8l.oct > doubleroll-8l.bin

; pipan8l> loadbin doubleroll-8l.bin
; pipan8l> setsw sr 060
; pipan8l> startat 02000

	. = 00000
countlo: .word	0
counthi: .word	-4

	. = 00003
h0003:	isz	countlo
	jmp	.-1
	jmp	finish3

	. = 00006
h0006:	isz	countlo
	jmp	.-1
	jmp	finish6

	. = 00014
h0014:	isz	countlo
	jmp	.-1
	isz	counthi
	jmp	.-3
	jmsi	_step		; PC=0014; AC=1400
	.word	h0030,00600
	.word	h0006,03000
	.word	h0014,01400

	. = 00030
h0030:	isz	countlo
	jmp	.-1
	isz	counthi
	jmp	.-3
	jmsi	_step		; PC=0030; AC=0600
	.word	h0060,00300
	.word	h0014,01400
	.word	h0030,00600

	. = 00060
h0060:	isz	countlo
	jmp	.-1
	isz	counthi
	jmp	.-3
	jmsi	_step		; PC=0060; AC=0300
	.word	h0140,00140
	.word	h0030,00600
	.word	h0060,00300

	. = 00140
h0140:	isz	countlo
	jmp	.-1
	isz	counthi
	jmp	.-3
	jmsi	_step		; PC=0140; AC=0140
	.word	h0300,00060
	.word	h0060,00300
	.word	h0140,00140

finish3:
	isz	counthi
	jmp	h0003
	jmsi	_step		; PC=0003; AC=6000
	.word	h0006,03000
	.word	h4001,04001
	.word	h0003,06000

finish6:
	isz	counthi
	jmp	h0006
	jmsi	_step		; PC=0006; AC=3000
	.word	h0014,01400	; up -> 0014; 1400
	.word	h0003,06000	; dn -> 0003; 6000
	.word	h0006,03000	; hold

_step:	.word	step

	. = 00300
h0300:	isz	countlo
	jmp	.-1
	isz	counthi
	jmp	.-3
	jmsi	_step		; PC=0300; AC=0060
	.word	h0600,00030
	.word	h0140,00140
	.word	h0300,00060

	. = 00600
h0600:	isz	countlo
	jmp	.-1
	isz	counthi
	jmp	.-3
	jmsi	_step		; PC=0600; AC=0030
	.word	h1400,00014
	.word	h0300,00060
	.word	h0600,00030

	. = 01400
h1400:	isz	countlo
	jmp	.-1
	isz	counthi
	jmp	.-3
	jmsi	_step		; PC=1400; AC=0014
	.word	h3000,00006
	.word	h0600,00030
	.word	h1400,00014

	. = 03000
h3000:	isz	countlo
	jmp	.-1
	isz	counthi
	jmp	.-3
	jmsi	_step		; PC=3000; AC=0006
	.word	h6000,00003
	.word	h1400,00014
	.word	h3000,00006

	. = 06000
h6000:	isz	countlo
	jmp	.-1
	isz	counthi
	jmp	.-3
	jmsi	_step		; PC=6000; AC=0003
	.word	h4001,04001
	.word	h3000,00006
	.word	h6000,00003

	. = 04001
h4001:	isz	countlo
	jmp	.-1
	isz	counthi
	jmp	.-3
	jmsi	_step		; PC=4001; AC=4001
	.word	h0003,06000
	.word	h6000,00003
	.word	h4001,04001

	. = 02000
	.global	__boot
__boot:
	cla cll
	tad	_4001		; put 4001 in AC
	jmpi	_4001		; put 4001 in PC

_4001:	.word	04001

; step PC and AC
;  jmsi   _step
;  .word  pcup,acup
;  .word  pcdn,acdn
;  .word  pchl,achl
step:	.word	.-.
	cla cll			; reset timing loop for next iteration
	tad	minus4
	dca	counthi
	cla osr			; read switches
	cll cma iac		; ...negative
	sna
	jmp	stephold	; - zero: hold in place
	tad	count		; compute count-switches
	snl cla
	jmp	stepinc
	tad	direc		; count >= SR, flip direction
	cma
	dca	direc
	dca	count		; reset count
stepinc:
	isz	count		; count < SR, increment
	cla
	tad	direc		; get direction
	sma cla
	jmp	stepup		; pos - pc goes up
stepdn:
	isz	step		; neg - pc goes down
	isz	step
	nop
	jmp	stepnext
stepup:
	cla osr			; read switches
	cll cma iac		; ...negative
	sna
	jmp	stephold	; - zero: hold in place
stepnext:
	cla
	tadi	step
	dca	steppc
	isz	step
	tadi	step
	jmpi	steppc
stephold:
	isz	step		; SR=0: hold in place
	isz	step
	jmp	stepdn

count:	.word	.-.
direc:	.word	.-.

steppc:	.word	.-.
minus4:	.word	-4

_0021:	.word	00021

