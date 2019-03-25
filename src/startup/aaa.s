.syntax unified
	.section .text.CHECK_PA0
	.type CHECK_PA0, %function
	.global CHECK_PA0

CHECK_PA0:
	mov R0, PC
	cmp R0, #0x20000000

	//IT HI
	blo SKIP_BX
	bx LR

SKIP_BX:
	
	LDR R0, =0x40023830 // RCC->GPIOA
	LDR R1, [R0]
	ORR R1, #1
	STR R1, [R0]
	
	LDR R0, =0x40020010 // GPIOA->IDR
	LDR R1, [R0]
	TST R1, #1
	BNE Flashprogramrun
	
	LDR R1, =(0xE000ED08) // NVIC load
	STR R0, [R1]
	LDR R0, =(0x20000000)
	LDR SP, [R0]
	LDR R0, =(0x20000004)
	LDR PC,[R0]
	
Flashprogramrun:
	LDR R1, [R0] // GPIOA->IDR
	TST R1, #1
	BNE Flashprogramrun
	
	BX LR

