#include <rigel-tasks.h>
#include <rigel.h>
#include <spinlock.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// Number of TQ_TaskDescEntry's to allocate on finding an empty freelist
#define PREALLOCATE_COUNT 1024

// Lock that protects task queue
ATOMICFLAG_INIT_CLEAR(GLOBAL_swtq_init_flag);

// Head pointer for the task queue
volatile TQ_TaskDescEntry *GLOBAL_swtq_head = NULL;
volatile TQ_TaskDescEntry *GLOBAL_swtq_tail = NULL;
// FIXME: Only works for up to 1024 cores
volatile int LOCAL_swtq_sense[1024];
volatile int GLOBAL_swtq_sense;

// Number of cores waiting on the TQ
volatile int GLOBAL_swtq_waitcount = 0;

// Used by LOCK-FREE version
volatile TQ_TaskDescEntry *GLOBAL_swtq_freelist_head_0 = NULL;
volatile TQ_TaskDescEntry *GLOBAL_swtq_freelist_head_1 = NULL;

TQ_RetType 
SW_TaskQueue_Init_LOCKFREE(int qID, int max_size) 
{
	int check, i;
	int count = PREALLOCATE_COUNT;
	TQ_TaskDescEntry *new_freelist;
	TQ_TaskDescEntry *tde;
	TQ_TaskDescEntry *tde_block;

	check = atomic_flag_check(&GLOBAL_swtq_init_flag);
	assert(!check && "TQ already initialized!");
	
	// Initialize freelist by grabbing a whole block at once
	tde_block = (TQ_TaskDescEntry *)malloc(count*sizeof(TQ_TaskDescEntry));
	// Link the entries in the block together
	while(--count > 0) {
		tde_block[count].next = &(tde_block[count-1]);
	}
	tde_block[0].next = NULL;
	new_freelist = &(tde_block[PREALLOCATE_COUNT-1]);
	// Push descriptors out to global cache
	RigelGlobalStore(new_freelist, GLOBAL_swtq_freelist_head_0);
	// Initialize task queue
	tde = (TQ_TaskDescEntry *)malloc(sizeof(TQ_TaskDescEntry));

	tde->next = NULL;
	RigelGlobalStore(tde, GLOBAL_swtq_head);
	RigelGlobalStore(tde, GLOBAL_swtq_tail);
	RigelGlobalStore(0, GLOBAL_swtq_sense);
	// Flush global sense
	RigelWritebackLine(&GLOBAL_swtq_sense);
	RigelInvalidateLine(&GLOBAL_swtq_sense);

	// Push task descriptors all out into global memory
	for (i = 0; i < PREALLOCATE_COUNT; i++) {
		RigelWritebackLine(&(tde_block[i]))
		RigelInvalidateLine(&(tde_block[i]))
	}
	RigelWritebackLine((tde->next));
	RigelInvalidateLine((tde->next));
	RigelWritebackLine((tde));
	RigelInvalidateLine((tde));
	RigelMemoryBarrier();

	atomic_flag_set(&GLOBAL_swtq_init_flag);

	return TQ_RET_SUCCESS;
}

TQ_RetType 
SW_TaskQueue_End_LOCKFREE(int qID) 
{
	int check;

	check = atomic_flag_check(&GLOBAL_swtq_init_flag);
	assert(check && "TQ not initialized but END called!");

	if (GLOBAL_swtq_head) {
		fprintf(stderr, "!!WARNING!! Found remaining tasks in TQ at END call\n");
	}
	// Should we free remaining tasks?
	GLOBAL_swtq_head = NULL;
	GLOBAL_swtq_tail = NULL;
	atomic_flag_clear(&GLOBAL_swtq_init_flag);

	return TQ_RET_SUCCESS;
}

TQ_RetType 
SW_TaskQueue_Enqueue_LOCKFREE(int qID, const TQ_TaskDesc *tdesc) 
{
	assert(0 && "Not yet implemented");
	abort();

	return TQ_RET_SUCCESS;
}

TQ_RetType 
SW_TaskQueue_Loop_LOCKFREE(int qID, int tot_iters, int iters_per_task, 
	const TQ_TaskDesc *tdesc) 
{
	int start = 0;
	int core_num = RigelGetCoreNum();

	while (start < tot_iters) {
		TQ_TaskDescEntry *new_freelist, *curr_freelist;
		TQ_TaskDescEntry *curr_tail, *new_tail;
		TQ_TaskDescEntry *next_node;
		volatile TQ_TaskDescEntry **CURR_freelist_head;

try_remove_freelist:
		CURR_freelist_head = (0 == LOCAL_swtq_sense[core_num]) ?  
			&GLOBAL_swtq_freelist_head_0 : 
			&GLOBAL_swtq_freelist_head_1;

		// Get the head of the current list of free descriptors
		RigelGlobalLoad(curr_freelist, *CURR_freelist_head);

		// Increase number of available task descriptors if we run out
		if (NULL == curr_freelist) {
			int count = PREALLOCATE_COUNT;
			// Grab a whole block at once
			TQ_TaskDescEntry *tde_block = 
				(TQ_TaskDescEntry *)malloc(count*sizeof(TQ_TaskDescEntry));
			// Link the entries in the block together
			new_freelist = &(tde_block[count-1]);
			while(--count > 0) {
				tde_block[count].next = &(tde_block[count-1]);
			}
			tde_block[0].next = NULL;
			// We are the only one adding so this should never fail
			RigelAtomicCAS(curr_freelist, new_freelist, (*CURR_freelist_head));
	
			// If we end up colliding while increasing the number of tdescs, back off
			// and try again.  FIXME: We could just merge the results...
			if (curr_freelist != new_freelist) {
				free(tde_block);
				tde_block = NULL;
				goto try_remove_freelist;
			}
			RigelGlobalLoad(curr_freelist, *CURR_freelist_head);
		}
			// Unlink the entry
		RigelGlobalLoad(new_freelist, curr_freelist->next);
		RigelAtomicCAS(curr_freelist, new_freelist, (*CURR_freelist_head));
		if (new_freelist != curr_freelist) { goto try_remove_freelist; }

		// Set up task descriptor
		curr_freelist->task_desc.v1 = tdesc->v1;
		curr_freelist->task_desc.v2 = tdesc->v2;
		curr_freelist->task_desc.begin = start;
		start += iters_per_task;
		// Deal with corner case
		if (start > tot_iters) { start = tot_iters; }
		curr_freelist->task_desc.end = start;
		curr_freelist->next = NULL;
		// Flush the descriptor back out to global memory TODO: Do en masse at the // end
		RigelWritebackLine(curr_freelist);
		RigelInvalidateLine(curr_freelist);
		// Insert the new task descriptor.  GLOBAL_swtq_tail may already be
		// deallocated, but it will not be reused yet since 
try_enqueue:		
		{
			TQ_TaskDescEntry *test_tde;
	
			// Get the value we want to insert again
			new_tail = curr_freelist;

			// Grab the current tail poiner
			RigelGlobalLoad(curr_tail, GLOBAL_swtq_tail);
			RigelGlobalLoad(next_node, (curr_tail->next));

			// Check to make sure the tail is what we expect
			RigelGlobalLoad(test_tde, GLOBAL_swtq_tail);
			if (test_tde != curr_tail) goto try_enqueue;

			if (NULL == next_node) {
				// This is the correct tail
				RigelAtomicCAS(next_node, new_tail, (curr_tail->next));
				if (next_node != new_tail) { goto try_enqueue; }
			} else {
				// The tail pointer is trailing...update
				RigelAtomicCAS(curr_tail, next_node, GLOBAL_swtq_tail);
				goto try_enqueue;
			}
			RigelAtomicCAS(curr_tail, curr_freelist, GLOBAL_swtq_tail);
		}
	}

	return TQ_RET_SUCCESS;
}

// Put a TaskDescEntry back on the free list
static inline void
swtq_freelist_add(TQ_TaskDescEntry *freed_tqe)
{	
	int core_num = RigelGetCoreNum();
	volatile TQ_TaskDescEntry *next_node, *curr_node, *test_node;

	// Set the proper double-buffered list
	volatile TQ_TaskDescEntry *global_tde_head = (1 == LOCAL_swtq_sense[core_num]) ?
			GLOBAL_swtq_freelist_head_0 : GLOBAL_swtq_freelist_head_1;
	// Return curr_tail to the freelist.  Not that this assumes there are two
	// free lists that are double-buffered.  The choice of free list is based on
	// the barrier sense.  We take them off of the current sense and add them to
	// the opposite.
	do {
		RigelGlobalLoad(curr_node, *global_tde_head);
		RigelGlobalLoad(next_node, curr_node->next);

		freed_tqe->next = next_node; 
		test_node = freed_tqe;
		RigelAtomicCAS(curr_node, test_node, *global_tde_head);
	} while (test_node != freed_tqe);
	RigelWritebackLine(freed_tqe);
	RigelInvalidateLine(freed_tqe);
}

TQ_RetType 
SW_TaskQueue_Dequeue_LOCKFREE(int qID, TQ_TaskDesc *tdesc) 
{
try_dequeue:
	{
	TQ_TaskDescEntry *curr_head, *new_head;
	TQ_TaskDescEntry *curr_tail, *new_tail;
	TQ_TaskDescEntry *test_tde;

		// Get current ends of the queue
		RigelGlobalLoad(curr_tail, GLOBAL_swtq_tail);
		RigelGlobalLoad(curr_head, GLOBAL_swtq_head);
		// Try to unlink one
		RigelGlobalLoad(new_head, (curr_head->next));

		// End of queue reached?
		if (curr_head == curr_tail) {
			// Queue is empty, try and enter the barrier
			if (NULL == new_head) { goto enter_barrier; }

			// Update trailing tail if needed and try again
			RigelAtomicCAS(curr_tail, new_head, GLOBAL_swtq_tail);
			goto try_dequeue;
		} else {
			// Provisional descriptor for us.  TODO: Can we defer this/use local access?
			*tdesc = new_head->task_desc;
			// Advance the head of the queue
			RigelAtomicCAS(curr_head, new_head, GLOBAL_swtq_head);
			if (curr_head != new_head) { goto try_dequeue; }
		}
		// Return descriptor to free list
		swtq_freelist_add(curr_head);

		return TQ_RET_SUCCESS;
	}

enter_barrier:
	// Barrier Reached
	{
		int num_cores = RigelGetNumCores();
		int core_num = RigelGetCoreNum();
		int pause_count = 0, pauses = 0;
		volatile int wc = 0, sense;
		TQ_TaskDescEntry *tde;
		TQ_TaskDescEntry *curr_head, *new_head;
		TQ_TaskDescEntry *curr_tail, *new_tail;

		// Flip Local Sense
		LOCAL_swtq_sense[core_num] = (0 == LOCAL_swtq_sense[core_num]) ? 1 : 0;
		// Add core to the barrier
		RigelAtomicINC(wc, GLOBAL_swtq_waitcount);

		if (wc == num_cores) {
			// No one has left the barrier.  Do ACTUAL check to see if tasks exist.
			// Note that we do not have to do second check to make sure curr_head has
			// not changed after we grab curr_head->next since no other cores are
			// able to access it (they are all in the barrier)
			RigelGlobalLoad(curr_head, GLOBAL_swtq_head);
			RigelGlobalLoad(curr_tail, GLOBAL_swtq_tail);
			RigelGlobalLoad(new_head, (curr_head->next)); // TODO: Opti: Push this down
			if (curr_head == curr_tail) {
				// Queue is empty, the barrier is valid
				if (NULL != new_head) {
					// QUEUE IS NOT EMPTY!  Leave the barrier
					RigelAtomicDEC(wc, GLOBAL_swtq_waitcount);
					// Flip Local Sense
					LOCAL_swtq_sense[core_num] = (0 == LOCAL_swtq_sense[core_num]) ? 1 : 0;

					goto try_dequeue;
				}
			}
			// The queues are definitely empty so no one in the barrier can wakeup at
			// this point.  We need to check one last time to make sure no one woke up
			// in the meantime and then we commit the barrier.
			RigelGlobalLoad(wc, GLOBAL_swtq_waitcount);
			if (wc != num_cores) {
				// Someone has left the barrier in the meantime, so should we.
				RigelAtomicDEC(wc, GLOBAL_swtq_waitcount);
				// Flip Local Sense
				LOCAL_swtq_sense[core_num] = (0 == LOCAL_swtq_sense[core_num]) ? 1 : 0;

				goto try_dequeue;
			}
			// Actual barrier reached.  No turning back...
			RigelGlobalStore(0, GLOBAL_swtq_waitcount);
			RigelGlobalStore(LOCAL_swtq_sense[core_num], GLOBAL_swtq_sense);

			return TQ_RET_SYNC;
		}

continue_barrier:
		// Spin waiting for barrier
		do {
			// Pause for a while to reduce global traffic (linear backoff)
			pauses = pause_count++;
			while(pauses-- > 0) { asm volatile ("nop; nop; nop; nop;"); }
			// No one has left the barrier.  Do ACTUAL check to see if tasks exist
			RigelGlobalLoad(curr_head, GLOBAL_swtq_head);
			RigelGlobalLoad(curr_tail, GLOBAL_swtq_tail);
			RigelGlobalLoad(new_head, (curr_head->next));// TODO - Opti: Push this down
			if (curr_head != curr_tail) {
				// QUEUE IS NOT EMPTY! 
				// Check to see if the barrier was reached in the meantime
				RigelGlobalLoad(sense, GLOBAL_swtq_sense);
				if (sense == LOCAL_swtq_sense[core_num]) { break; }
				
				// Try to leave the barrier
				RigelAtomicDEC(wc, GLOBAL_swtq_waitcount);
				if (wc == num_cores-1) {
					// Subtle race opens between an ATOM.INC moving wc to num_cores and
					// teh time when that core gets the lock. To avoid a problem, if we
					// find that wc was equal to num_cores, we back off and try again.
					// The core just entering the barrier that incremented wc should
					// reset the barrier (if appropriate.
					RigelAtomicINC(wc, GLOBAL_swtq_waitcount);
					continue;
				} 
				// Leave the barrier
				LOCAL_swtq_sense[core_num] = (0 == LOCAL_swtq_sense[core_num]) ? 1 : 0;
				goto try_dequeue;
			}

			// Grab the current waitcounter and try again
			RigelGlobalLoad(sense, GLOBAL_swtq_sense);
		} while (sense != LOCAL_swtq_sense[core_num]);
	}

	return TQ_RET_SYNC;
}
