#include "rigel-tasks.h"



TQ_RetType 
TaskQueue_Init(int qID, int max_size) {
	TQ_RetType retval;

	asm ( 
				"addi	$7, $25, 0; \n\t"
				"addi	$8, $26, 0; \n\t"
				"addi	$9, $27, 0; \n\t"
				"addi	$10, $28, 0; \n\t"
				"addi	$11, $24, 0; \n\t"
				"or $25, %1, $0; \n\t"
				"or $26, %2, $0; \n\t"
				"tq.init;\n\t" 
				"or	%0, $24, $0;"
				"addi	$25, $7, 0; \n\t"
				"addi	$26, $8, 0; \n\t"
				"addi	$27, $9, 0; \n\t"
				"addi	$28, $10, 0; \n\t"
				"addi	$24, $11, 0; \n\t"
				: "=r"(retval)
				:"r"(qID), "r"(max_size)
				: "24", "25", "26"
			);

	return retval;
}

TQ_RetType 
TaskQueue_End(int qID) {
	TQ_RetType retval = TQ_RET_SUCCESS;

	asm ( 
				"addi	$7, $25, 0; \n\t"
				"addi	$8, $26, 0; \n\t"
				"addi	$9, $27, 0; \n\t"
				"addi	$10, $28, 0; \n\t"
				"addi	$11, $24, 0; \n\t"
				"tq.end;\n\t" 
				"or	%0, $24, $0;"
				"addi	$25, $7, 0; \n\t"
				"addi	$26, $8, 0; \n\t"
				"addi	$27, $9, 0; \n\t"
				"addi	$28, $10, 0; \n\t"
				"addi	$24, $11, 0; \n\t"
				: "=r"(retval)
				: 
				: "24"
			);

	return retval;
}

TQ_RetType 
TaskQueue_Enqueue(int qID, const TQ_TaskDesc *tdesc) {
	TQ_RetType retval = TQ_RET_SUCCESS;

	asm ( 
				"addi	$7, $25, 0; \n\t"
				"addi	$8, $26, 0; \n\t"
				"addi	$9, $27, 0; \n\t"
				"addi	$10, $28, 0; \n\t"
				"addi	$11, $24, 0; \n\t"
				"or $25, %1, $0; \n\t"
				"or $26, %2, $0; \n\t"
				"or $27, %3, $0; \n\t"
				"or $28, %4, $0; \n\t"
				"tq.enq;\n\t" 
				"or	%0, $24, $0;"
				"addi	$25, $7, 0; \n\t"
				"addi	$26, $8, 0; \n\t"
				"addi	$27, $9, 0; \n\t"
				"addi	$28, $10, 0; \n\t"
				"addi	$24, $11, 0; \n\t"
				: "=r"(retval)
				: "r"(tdesc->v1), "r"(tdesc->v2), "r"(tdesc->v3), "r"(tdesc->v4)
				: "24", "25", "26", "27", "28"
			);

	return retval;
}

TQ_RetType 
TaskQueue_Loop(int qID, int tot_iters, int iters_per_task, const TQ_TaskDesc *tdesc) {
	TQ_RetType retval = TQ_RET_SUCCESS;

	// IGNORED: tdesc->begin and tdesc->end
	
	asm ( 
				"addi	$7, $25, 0; \n\t"
				"addi	$8, $26, 0; \n\t"
				"addi	$9, $27, 0; \n\t"
				"addi	$10, $28, 0; \n\t"
				"addi	$11, $24, 0; \n\t"
				"or $25, %1, $0; \n\t"
				"or $26, %2, $0; \n\t"
				"or $27, %3, $0; \n\t"
				"or $28, %4, $0; \n\t"
				"tq.loop;\n\t" 
				"or	%0, $24, $0; \n\t"
				"addi	$25, $7, 0; \n\t"
				"addi	$26, $8, 0; \n\t"
				"addi	$27, $9, 0; \n\t"
				"addi	$28, $10, 0; \n\t"
				"addi	$24, $11, 0; \n\t"
				: "=r"(retval)
				: "r"(tdesc->ip), "r"(tdesc->data), "r"(tot_iters), "r"(iters_per_task)
				: "24", "25", "26", "27", "28"
			);

	return retval;
}

TQ_RetType 
TaskQueue_Dequeue(int qID, TQ_TaskDesc *tdesc) {
	TQ_RetType retval = TQ_RET_SUCCESS;

	asm ( 
				"addi	$7, $25, 0; \n\t"
				"addi	$8, $26, 0; \n\t"
				"addi	$9, $27, 0; \n\t"
				"addi	$10, $28, 0; \n\t"
				"addi	$11, $24, 0; \n\t"
				"tq.deq;\n\t" 
				"or	%0, $24, $0; \n\t"
				"or %1, $25, $0; \n\t"
				"or %2, $26, $0; \n\t"
				"or %3, $27, $0; \n\t"
				"or %4, $28, $0;"
				"or $28, $24, $0;\n"
				"addi	$25, $7, 0; \n\t"
				"addi	$26, $8, 0; \n\t"
				"addi	$27, $9, 0; \n\t"
				"addi	$28, $10, 0; \n\t"
				"addi	$24, $11, 0; \n\t"
				: "=r"(retval), "=r"(tdesc->v1), "=r"(tdesc->v2), "=r"(tdesc->v3), "=r"(tdesc->v4)
				: 
				: "24", "25", "26", "27", "28"
			);

	return retval;
}
