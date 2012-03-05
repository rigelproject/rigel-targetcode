# setjmp and longjmp for the ece412 simulator
# on entry reg $4 holds pointer to 16 word jmpbuf

	.text
	.align	4
	.globl	setjmp
	.ent	setjmp
        .type   setjmp, @function
setjmp:
        .set    noreorder
        stw $18, $4, 0
        stw $19, $4, 4
        stw $20, $4, 8
        stw $21, $4, 12
        stw $22, $4, 16
        stw $23, $4, 20
        stw $24, $4, 24
        stw $25, $4, 28
        stw $26, $4, 32
        stw $27, $4, 36
        stw $gp, $4, 40
        stw $sp, $4, 44
        stw $fp, $4, 48
        stw $ra, $4, 52
        xor $2, $2, $2
        #jal 0
        #stw $ra, $4, 56 #This PC
        #ldw $ra, $4, 52
        jmpr $ra
	.end	setjmp

	.text
	.align	4
	.globl	longjmp
	.ent	longjmp
        .type   longjmp, @function
longjmp:
        .set    noreorder
        ldw $18, $4, 0
        ldw $19, $4, 4
        ldw $20, $4, 8
        ldw $21, $4, 12
        ldw $22, $4, 16
        ldw $23, $4, 20
        ldw $24, $4, 24
        ldw $25, $4, 28
        ldw $26, $4, 32
        ldw $27, $4, 36
        ldw $gp, $4, 40
        ldw $sp, $4, 44
        ldw $fp, $4, 48
        ldw $ra, $4, 52
        bne $5, $zero, 1f
        addi $2, $zero, 1
        jmp 2f 
1:
        add $2, $zero, $5
2:
        jmpr $ra
    .end   longjmp

