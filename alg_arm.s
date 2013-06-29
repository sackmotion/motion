.globl	alg_diff_asm
.globl	alg_update_reference_frame_asm

@ r0 - ref image ptr
@ r1 - new image ptr
@ r2 - out image ptr
@ r3 - pixel count
@ noise threshold

alg_diff_asm:
		stmfd	sp!, {r4, r5, r6, r7, r8, r9, r10, r11, r12, lr}	@ save regs here

		mov		r6, r0
		sub		r0, r0
		@calculate noise threshold in parallel into r12
		ldr		r12,[sp, #40]
		add		r12, r12, #1
		orr		r12, r12, lsl #8
		orr		r12, r12, lsl #16
		mov		r9, r0			@ always zeros
		mov		r8, #1			@ four 1 bytes
		orr		r8, r8, lsl #8
		orr		r8, r8, lsl #16

L0:
		ldr		r10, [r6]		@ load original & new pixels 4 at a time		@C1L3
		ldr		r11, [r1]														@C1L3
		add		r6, r6, #4														@C1L1
		add		r1, r1, #4														@C1L1

		uqsub8	r4, r10, r11	@ absolute difference into r3					@C1L2
		uqsub8	r5, r11, r10													@C1L2
		ldr		r7, [r2]														@C1L3
		orr		r4, r4, r5														@C1L1

		usub8	r4, r4, r12		@ compare against threshold, and mix out/new	@C1L1
		sel		r5, r11, r7		@ based on that into r4							@C1L1
		str		r5, [r2]														@C1L1

		sel		r4, r8, r9		@ calculate number of different pixels			@C1L1
		usada8	r0, r4, r9, r0													@C1L3

		subs	r3, r3, #4														@C1L1
		add		r2, r2, #4														@C1L1
		bge		L0																@C5-7

		ldmfd	sp!, {r4, r5, r6, r7, r8, r9, r10, r11, r12, pc}

@ Macro to process a single pixel post absolute differencing & thresholding
@
@ r0 - in: virgin image ptr + 4	(byte*)
@ r1 - in: ref image ptr + 4	(byte*)
@ r2 - in: out image ptr		(byte*)	(will increment)
@ r3 - in: ref_dyn image ptr	(int*)	(will increment)
@ r4 - in: smart mask ptr		(byte*)	(will increment)
@ r5 - in: ? (used as ref_dyn value, virgin pixel, new ref pixel)
@ r6 - in: accept timer
@ r7 - in: ? (used as smart mask pixel, out pixel, ref pixel)
@ r8 - in: 4 x thresholded diff pixels
@ r9 - in: ? (used as updated ref_dyn value)
@ r10- in: ? (unused)
@ r11- in: ? (unused)
@ r12- in: ? (but outer loop is reserving this as a constant)
@
.macro	update_ref_pix	pix_lsb, pix_offset
		ldrb	r7, [r4]				@ smart mask pixel
		add		r4, r4, #1
		cmp		r7, #0
		beq		2f
		uxtb	r9, r8, ror #\pix_lsb	@ diff pixel
		cmp		r9, #0					@ ... if 0, no motion
		beq		2f

		ldr		r5, [r3]				@ if *ref_dyn == 0...
		mov		r9, #1
		cmp		r5, #0
		beq		3f						@ ...*ref_dyn = 1
		cmp		r5, r6					@ if *ref_dyn > accept timer...
		bgt		2f						@ ...*ref_dyn = 0, *ref = *virgin

		ldrb	r7, [r2]				@ if *out
		add		r9, r5, #1
		cmp		r7, #0
		bgt		3f						@ ...(*ref_dyn)++
1:@release_pixel:
		ldrb	r7, [r0, #\pix_offset]	@ *ref = (*ref + *virgin) / 2
		ldrb	r5, [r1, #\pix_offset]
		sub		r9, r9, r9				@ *ref_dyn = 0
		add		r5, r5, r7
		lsr		r5, r5, #1
		strb	r5, [r1, #\pix_offset]
		b		3f
2:@copy_virgin:
		ldrb	r7, [r0, #\pix_offset]	@ ref[0] = virgin[0]
		sub		r9, r9, r9				@ *ref_dyn = 0
		strb	r7, [r1, #\pix_offset]
3:@next_ref:
		str		r9, [r3]
		add		r3, r3, #4
		add		r2, r2, #1
.endm

@ r0 - virgin image ptr		(byte*)
@ r1 - ref image ptr		(byte*)
@ r2 - out image ptr		(byte*)
@ r3 - ref_dyn image ptr	(int*)
@ smart mask							[sp, #40]
@ pixel count							[sp, #44]
@ threshold								[sp, #48]
@ accept timer							[sp, #52]

alg_update_reference_frame_asm:
		stmfd	sp!, {r4, r5, r6, r7, r8, r9, r10, r11, r12, lr}

		ldr		r12,[sp, #48]	@calculate noise threshold in parallel into r12
		add		r12, r12, #1
		orr		r12, r12, lsl #8
		orr		r12, r12, lsl #16

		ldr		r4, [sp, #40]	@ smart mask ptr
		b		L2
L1:
		str		r6, [sp, #44]	@ update pixel count
L2:
		ldr		r10, [r0]		@ load 4 virgin, ref, smartmask pixels 4 		@C1L3
		ldr		r11, [r1]														@C1L3
		add		r0, r0, #4														@C1L1
		add		r1, r1, #4														@C1L1

		uqsub8	r8, r10, r11	@ absolute differences into r8					@C1L2
		uqsub8	r9, r11, r10													@C1L2
		ldr		r6, [sp, #52]	@ accept timer
		orr		r8, r8, r9														@C1L1

		usub8	r8, r8, r12		@ compare against threshold, and zero out 		@C1L1
		sub		r9, r9, r9		@ those pixels lower than the threshold			@C1L1
		sel		r8, r8, r9		@ into r8										@C1L1

		update_ref_pix	0, -4
		update_ref_pix	8, -3
		update_ref_pix	16, -2
		update_ref_pix	24, -1

		ldr		r6, [sp, #44]	@ update pixel count
		subs	r6, r6, #4
		bgt		L1

		ldmfd	sp!, {r4, r5, r6, r7, r8, r9, r10, r11, r12, pc}
