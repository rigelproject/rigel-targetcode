#include <rigel-tasks.h>
#include <rigel.h>
#include <spinlock.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// Number of TQ_TaskDescEntry's to allocate on finding an empty freelist
#define PREALLOCATE_COUNT 256
#undef RigelPrint
#define RigelPrint(x) 

// Lock that protects task queue
SPINLOCK_INIT_OPEN(GLOBAL_swtq_taskqueue_lock);
SPINLOCK_INIT_OPEN(GLOBAL_swtq_freelist_lock);
ATOMICFLAG_INIT_CLEAR(GLOBAL_swtq_init_flag);

// Head pointer for the task queue
volatile TQ_TaskDescEntry *GLOBAL_swtq_head = NULL;
volatile TQ_TaskDescEntry *GLOBAL_swtq_tail = NULL;
// FIXME: Only works for up to 1024 cores
volatile int LOCAL_swtq_sense[1024];
volatile int GLOBAL_swtq_sense;

// Number of cores waiting on the TQ TODO: Replace in SPINLOCK with unified
// counter
volatile int GLOBAL_swtq_waitcount = 0;

// Used by SPINLOCK version
volatile TQ_TaskDescEntry *GLOBAL_swtq_freelist_head = NULL;

//XXX
//XXX	LOCK-FREE SW TASK QUEUES 
//XXX
TQ_RetType 
SW_TaskQueue_Init_SPINLOCK(int qID, int max_size) 
{
	int check, i;
	TQ_TaskDescEntry *new_freelist;
	TQ_TaskDescEntry *tde;
	TQ_TaskDescEntry *tde_block;

	check = atomic_flag_check(&GLOBAL_swtq_init_flag);
	assert(!check && "TQ already initialized!");
	
	RigelPrint(0xaa000001);
	// XXX: ACQUIRE LOCK [[FREE LIST]]
	spin_lock(&GLOBAL_swtq_freelist_lock);
	{
		int count = PREALLOCATE_COUNT;
		// Initialize freelist by grabbing a whole block at once
		tde_block = (TQ_TaskDescEntry *)malloc(count*sizeof(TQ_TaskDescEntry));
		// Link the entries in the block together
		while(--count > 0) {
			tde_block[count].next = &(tde_block[count-1]);
		}
		tde_block[0].next = NULL;
		new_freelist = &(tde_block[PREALLOCATE_COUNT-1]);

		RigelGlobalStore(new_freelist, GLOBAL_swtq_freelist_head);
	}
	// XXX: RELEASE LOCK [[FREE LIST]]
	spin_unlock(&GLOBAL_swtq_freelist_lock);
	
	// XXX: ACQUIRE LOCK [[TASK QUEUE]]
	spin_lock(&GLOBAL_swtq_taskqueue_lock);
	{
		// Initialize task queue
		tde = (TQ_TaskDescEntry *)malloc(sizeof(TQ_TaskDescEntry));

		tde->next = NULL;
		RigelGlobalStore(tde, GLOBAL_swtq_head);
		RigelGlobalStore(tde, GLOBAL_swtq_tail);
		// Set and flush global sense
		RigelGlobalStore(0, GLOBAL_swtq_sense);
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
	}
	// XXX: RELEASE LOCK [[TASK QUEUE]]
	spin_unlock(&GLOBAL_swtq_taskqueue_lock);

	atomic_flag_set(&GLOBAL_swtq_init_flag);

	RigelPrint(0xaa000002);

	return TQ_RET_SUCCESS;
}

TQ_RetType 
SW_TaskQueue_End_SPINLOCK(int qID) 
{
	int check;

	check = atomic_flag_check(&GLOBAL_swtq_init_flag);
	assert(check && "TQ not initialized but END called!");
	// XXX: ACQUIRE LOCK [[TASK QUEUE]]
	spin_lock(&GLOBAL_swtq_taskqueue_lock);

	if (GLOBAL_swtq_head) {
		fprintf(stderr, "!!WARNING!! Found remaining tasks in TQ at END call\n");
	}
	// Should we free remaining tasks?
	GLOBAL_swtq_head = NULL;
	GLOBAL_swtq_tail = NULL;

	// XXX: RELEASE LOCK [[TASK QUEUE]]
	spin_unlock(&GLOBAL_swtq_taskqueue_lock);

	atomic_flag_clear(&GLOBAL_swtq_init_flag);

	return TQ_RET_SUCCESS;
}

TQ_RetType 
SW_TaskQueue_Enqueue_SPINLOCK(int qID, const TQ_TaskDesc *tdesc) 
{
	assert(0 && "Not yet implemented");
	abort();

	return TQ_RET_SUCCESS;
}

TQ_RetType 
SW_TaskQueue_Loop_SPINLOCK(int qID, int tot_iters, int iters_per_task, 
	const TQ_TaskDesc *tdesc) 
{
	int start = 0;
	int core_num = RigelGetCoreNum();
	TQ_TaskDescEntry *next_node, *new_node, *tail_node;
	volatile TQ_TaskDescEntry **CURR_freelist_head;

RigelPrint(0xbb000001);
	while (start < tot_iters) {
RigelPrint(0xbb000002);
		// ###### 1. Get a new entry #####
		
		// XXX: ACQUIRE LOCK [[FREE LIST]]
		spin_lock(&GLOBAL_swtq_freelist_lock);
		{
RigelPrint(0xbb000003);
			// Get the head of the current list of free descriptors
			RigelGlobalLoad(new_node, GLOBAL_swtq_freelist_head);
RigelPrint(new_node);

			// Increase number of available task descriptors if we run out
			if (NULL == new_node) {
RigelPrint(0xbb000004);
				int count = PREALLOCATE_COUNT;
				// Grab a whole block at once
				TQ_TaskDescEntry *tde_block = 
					(TQ_TaskDescEntry *)malloc(count*sizeof(TQ_TaskDescEntry));
				// Link the entries in the block together
				while(--count > 0) {
					tde_block[count].next = &(tde_block[count-1]);
				}
				tde_block[0].next = NULL;
				// Insert the new entries into the free list
				new_node = &(tde_block[PREALLOCATE_COUNT-1]);
				RigelGlobalStore(new_node, GLOBAL_swtq_freelist_head);
				// Get the updated freelist for use in this insert
RigelPrint(0xbb000005);
			}
RigelPrint(0xbb000006);
			// Unlink the entry
			RigelGlobalLoad(next_node, new_node->next);
			RigelGlobalStore(next_node, GLOBAL_swtq_freelist_head);
RigelPrint(0xbb000007);
		}
		// XXX: RELEASE LOCK [[FREE LIST]]
		spin_unlock(&GLOBAL_swtq_freelist_lock);
RigelPrint(0xbb000008);
RigelPrint(new_node);

		// ###### 2. Setup the descriptor #####
		new_node->task_desc.v1 = tdesc->v1;
		new_node->task_desc.v2 = tdesc->v2;
		new_node->task_desc.begin = start;
		start += iters_per_task;
		// Deal with corner case
		if (start > tot_iters) { start = tot_iters; }
		new_node->task_desc.end = start;
		new_node->next = NULL;
		// Flush the descriptor back out to global memory TODO: Do en masse at the end
//		RigelWritebackLine(new_node);
//		RigelInvalidateLine(new_node);
		
RigelPrint(0xbb000009);
RigelPrint((void *)&GLOBAL_swtq_taskqueue_lock);
		// ###### 3. Insert the descriptor #####
		// XXX: ACQUIRE LOCK [[TASK QUEUE]]
		spin_lock(&GLOBAL_swtq_taskqueue_lock);
		{
			// Get the value we want to insert again
			TQ_TaskDescEntry *new_tail = new_node;
RigelPrint(0xbb00000a);

			// Grab the current tail poiner
			RigelGlobalLoad(tail_node, GLOBAL_swtq_tail);
			// Insert the new_tail
			RigelGlobalStore(new_tail, (tail_node->next));
			// Update global tail pointer
			RigelGlobalStore(new_tail, GLOBAL_swtq_tail);
		}
		// XXX: RELEASE LOCK [[TASK QUEUE]]
		spin_unlock(&GLOBAL_swtq_taskqueue_lock);
RigelPrint(0xbb00000b);
	}

RigelPrint(0xbb00000c);
	return TQ_RET_SUCCESS;
}

// Put a TaskDescEntry back on the free list
static void
swtq_freelist_add(TQ_TaskDescEntry *freed_tqe)
{	
	int core_num = RigelGetCoreNum();
	TQ_TaskDescEntry *head_node;
	
	freed_tqe->next = NULL;

RigelPrint(0xcc000001);

	// XXX: ACQUIRE LOCK [[FREE LIST]]
	spin_lock(&GLOBAL_swtq_freelist_lock);
	{
RigelPrint(0xcc000002);
		RigelGlobalLoad(head_node, GLOBAL_swtq_freelist_head);

		freed_tqe->next = head_node;
		RigelGlobalStore(freed_tqe, GLOBAL_swtq_freelist_head);
	}
	// XXX: RELEASE LOCK [[FREE LIST]]
	spin_unlock(&GLOBAL_swtq_freelist_lock);

	// It is safe to writeback late since nothing is reused until after the
	// barrier has been reached.
	RigelWritebackLine(freed_tqe);
	RigelInvalidateLine(freed_tqe);
RigelPrint(0xcc000003);
}

TQ_RetType 
SW_TaskQueue_Dequeue_SPINLOCK(int qID, TQ_TaskDesc *tdesc) 
{
	TQ_TaskDescEntry *curr_head, *curr_tail, *new_node;
	volatile int sense, wc;
	// Only needed if try_dequeue fails
	int core_num, pauses;

RigelPrint(0xdd000001);
try_dequeue:

	// XXX: ACQUIRE LOCK [[TASK QUEUE]]
	spin_lock(&GLOBAL_swtq_taskqueue_lock);
	{
RigelPrint(0xdd000002);
		// Grab the current head
		RigelGlobalLoad(curr_head, GLOBAL_swtq_head);
		RigelGlobalLoad(curr_tail, GLOBAL_swtq_tail);

		// If HEAD == TAIL, queue is empty
		if (curr_head == curr_tail) { 
RigelPrint(0xdd000003);
			// XXX: RELEASE LOCK [[TASK QUEUE]]
			spin_unlock(&GLOBAL_swtq_taskqueue_lock);
			
			goto enter_barrier; 
		}

		// Unlink an entry
RigelPrint(0xdd000111);
		RigelGlobalLoad(new_node, (curr_head->next));
RigelPrint(0xdd000112);
		RigelGlobalStore(new_node, GLOBAL_swtq_head);
RigelPrint(0xdd000113);

		// Provisional descriptor for us.  TODO: Can we defer this/use local access?
		*tdesc = curr_head->task_desc;
RigelPrint(0xdd000004);
	}
	// XXX: RELEASE LOCK [[TASK QUEUE]]
	spin_unlock(&GLOBAL_swtq_taskqueue_lock);

	// Return descriptor to free list
RigelPrint(0xdd000005);
	swtq_freelist_add(curr_head);
RigelPrint(0xdd000006);

	return TQ_RET_SUCCESS;

enter_barrier:
	core_num = RigelGetCoreNum();
RigelPrint(0xdd000007);
	// Barrier Reached
	// XXX: ACQUIRE LOCK [[TASK QUEUE]]
	spin_lock(&GLOBAL_swtq_taskqueue_lock);
	{
		int num_cores = RigelGetNumCores();
		TQ_TaskDescEntry *tde;

RigelPrint(0xdd000008);
		// Flip Local Sense
		LOCAL_swtq_sense[core_num] = (0 == LOCAL_swtq_sense[core_num]) ? 1 : 0;
		// Add core to the barrier
		RigelAtomicINC(wc, GLOBAL_swtq_waitcount);

		if (wc == num_cores) {
RigelPrint(0xdd000009);
			RigelGlobalLoad(curr_head, GLOBAL_swtq_head);
			RigelGlobalLoad(curr_tail, GLOBAL_swtq_tail);
			if (curr_head != curr_tail) {
RigelPrint(0xdd00000a);
				//  Queue is not empty, leave the barrier
				RigelAtomicDEC(wc, GLOBAL_swtq_waitcount);
				// Flip Local Sense
				LOCAL_swtq_sense[core_num] = (0 == LOCAL_swtq_sense[core_num]) ? 1 : 0;

				// XXX: RELEASE LOCK [[TASK QUEUE]]
				spin_unlock(&GLOBAL_swtq_taskqueue_lock);

				goto try_dequeue;
			}

RigelPrint(0xdd00000b);
			// Actual barrier reached.  No turning back...
			RigelGlobalStore(0, GLOBAL_swtq_waitcount);
			RigelGlobalStore(LOCAL_swtq_sense[core_num], GLOBAL_swtq_sense);
	
			// XXX: RELEASE LOCK [[TASK QUEUE]]
			spin_unlock(&GLOBAL_swtq_taskqueue_lock);

			return TQ_RET_SYNC;
		}
	}
	// XXX: RELEASE LOCK [[TASK QUEUE]]
	spin_unlock(&GLOBAL_swtq_taskqueue_lock);
RigelPrint(0xdd00000c);

continue_barrier:
	// Reset pauses
	pauses = 1;
	// Spin waiting for barrier, lock held on entrance
	do {
		// XXX: ACQUIRE LOCK [[TASK QUEUE]]
		spin_lock(&GLOBAL_swtq_taskqueue_lock);
		{
RigelPrint(0xdd00000d);
			// No one has left the barrier.  Do ACTUAL check to see if tasks exist
			RigelGlobalLoad(curr_head, GLOBAL_swtq_head);
			RigelGlobalLoad(curr_tail, GLOBAL_swtq_tail);
			if (curr_head != curr_tail) {
RigelPrint(0xdd00000e);
				// QUEUE IS NOT EMPTY! 
				RigelGlobalLoad(sense, GLOBAL_swtq_sense);
				if (sense == LOCAL_swtq_sense[core_num]) { 
					// The barrier was reached in the meantime.
					
					// XXX: RELEASE LOCK [[TASK QUEUE]]
					spin_unlock(&GLOBAL_swtq_taskqueue_lock);
	
					return TQ_RET_SYNC;
				}

RigelPrint(0xdd00000f);
				// Leave the barrier and try again.
				RigelAtomicDEC(wc, GLOBAL_swtq_waitcount);
				// Flip Local Sense
				LOCAL_swtq_sense[core_num] = (0 == LOCAL_swtq_sense[core_num]) ? 1 : 0;
				// XXX: RELEASE LOCK [[TASK QUEUE]]
				spin_unlock(&GLOBAL_swtq_taskqueue_lock);
	
				goto try_dequeue;
			}
		}
RigelPrint(0xdd000010);
		// XXX: RELEASE LOCK [[TASK QUEUE]]
		spin_unlock(&GLOBAL_swtq_taskqueue_lock);

		{
			// Pause for a while to reduce global traffic (linear backoff)
			int pcount = pauses++;
			while(pcount-- >= 0) { asm volatile ("nop; nop; nop; nop;"); }
		}

RigelPrint(0xdd000011);
		// Grab the current waitcounter and try again
		RigelGlobalLoad(sense, GLOBAL_swtq_sense);
	} while (sense != LOCAL_swtq_sense[core_num]);
RigelPrint(0xdd000012);
	return TQ_RET_SYNC;
}
