	.file	"asm-Linux-386.S"
	.text

	.type	FPsave,@function
	.global	FPsave
FPsave:
	pushl	%ebp
	movl	%esp, %ebp
	movl	8(%ebp), %eax
	fstenv	(%eax)
	popl	%ebp
	ret

	.type	FPrestore,@function
	.global	FPrestore
FPrestore:
	pushl	%ebp
	movl	%esp, %ebp
	movl	8(%ebp), %eax
	fldenv	(%eax)
	popl	%ebp
	ret

	.type	_tas,@function
	.globl	_tas
_tas:
	movl	$1, %eax
	movl	4(%esp), %ecx
	xchgl	%eax, 0(%ecx)
	ret
