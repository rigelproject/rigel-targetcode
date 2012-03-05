#include <rigel-tasks.h>
#include <rigel.h>
#include <spinlock.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// Number of TQ_TaskDescEntry's in queue.  Order is the log2 of the size.  The
// size of the queue must be greater than 2x the number of cores to work
// properly as well FIXME: Right now we set this as a fixed size
#define TASK_QUEUE_SIZE_ORDER 14
#define TASK_QUEUE_SIZE (1 << TASK_QUEUE_SIZE_ORDER)

// Conversion routine for indices
#define TQ_IDX(idx) ((idx) % TASK_QUEUE_SIZE)
// How many entries to insert into the queue in one shot
#define ENQUEUE_BLOCK_SIZE 32

// Flag that protects initialization
ATOMICFLAG_INIT_CLEAR(GLOBAL_fixedtq_init_flag);
// Lock protecting tail of queue for inserts
SPINLOCK_INIT_OPEN(GLOBAL_fixedtq_enqueue_lock);
// Lock protecting two cores from updating waitcount at a barrier simultaneously
SPINLOCK_INIT_OPEN(GLOBAL_fixedtq_barrier_lock);

// Head and tail pointer for the task queue
volatile unsigned int GLOBAL_fixedtq_head_index = 0;
volatile unsigned int GLOBAL_fixedtq_tail_index = 0;
// The number of tasks currently enqueued
volatile int GLOBAL_fixedtq_count = 0;

// Set in init to reduce number of calls to RigelGetNumCores
int GLOBAL_swtq_numcores;

// Task queue itself.  TODO: Dynamically allocate?  
TQ_TaskDescEntry GLOBAL_swtq_taskqueue[TASK_QUEUE_SIZE];

// FIXME: Only works for up to 1024 cores
volatile int LOCAL_fixedtq_sense[1024];
volatile int GLOBAL_fixedtq_sense;
volatile int GLOBAL_fixedtq_barrier_complete_in_progress = 0;

// Number of cores waiting on the TQ
volatile int GLOBAL_fixedtq_waitcount = 0;

TQ_RetType 
SW_TaskQueue_Init_FIXED(int qID, int max_size) 
{
	int check, num_cores;
	int i;

	check = atomic_flag_check(&GLOBAL_fixedtq_init_flag);
	assert(!check && "TQ already initialized!");

	num_cores = RigelGetNumCores();
	RigelGlobalStore(num_cores, GLOBAL_swtq_numcores);
	RigelGlobalStore(0, GLOBAL_fixedtq_head_index);
	RigelGlobalStore(0, GLOBAL_fixedtq_tail_index);
	RigelGlobalStore(0, GLOBAL_fixedtq_waitcount);
	RigelGlobalStore(0, GLOBAL_fixedtq_sense);

	for (i = 0; i < num_cores; i++) {
		LOCAL_fixedtq_sense[i] = 0;
	}

	atomic_flag_set(&GLOBAL_fixedtq_init_flag);

	return TQ_RET_SUCCESS;
}

TQ_RetType 
SW_TaskQueue_End_FIXED(int qID) 
{
	int check;

	check = atomic_flag_check(&GLOBAL_fixedtq_init_flag);
	assert(check && "TQ not initialized but END called!");

	atomic_flag_clear(&GLOBAL_fixedtq_init_flag);

	return TQ_RET_SUCCESS;
}

TQ_RetType 
SW_TaskQueue_Enqueue_FIXED(int qID, const TQ_TaskDesc *tdesc) 
{
	int tail_idx, old_idx;

	do {
		// Find the next index in the queue
		RigelGlobalLoad(tail_idx, GLOBAL_fixedtq_tail_index);
		old_idx = tail_idx;
		tail_idx = TQ_IDX(tail_idx);

		// At this point we own an entry, update that entry
		GLOBAL_swtq_taskqueue[tail_idx].task_desc = *tdesc;
		// Flush the task descriptor.
		RigelFlushLine(&GLOBAL_swtq_taskqueue[tail_idx]);

		// Actually add the entry so that waiting dequeuers can take it
		tail_idx = old_idx;
		tail_idx++;
		RigelAtomicCAS(old_idx, tail_idx, GLOBAL_fixedtq_tail_index);
	} while (old_idx != tail_idx);

	return TQ_RET_SUCCESS;
}

TQ_RetType 
SW_TaskQueue_Loop_FIXED(int qID, int tot_iters, int iters_per_task, 
	const TQ_TaskDesc *tdesc) 
{
	int start = 0;

	// Do a block enqueue
	while (start < tot_iters) {
		// XXX: ACQUIRE LOCK [[ENQUEUE]]
		spin_lock(&GLOBAL_fixedtq_enqueue_lock);
		{
			unsigned int tail_idx;
			int task_count;
			TQ_TaskDesc t;
			t.v1 = tdesc->v1;
			t.v2 = tdesc->v2;

			// Find the next index in the queue
			RigelGlobalLoad(tail_idx, GLOBAL_fixedtq_tail_index);
	
			for (	task_count = 0; 
						(task_count < ENQUEUE_BLOCK_SIZE) && (start < tot_iters); 
						task_count++, tail_idx++)
			{
				t.begin = start;
				start += iters_per_task;
				if (start > tot_iters) { start = tot_iters; }
				t.end = start;

				// At this point we own an entry, update that entry
				GLOBAL_swtq_taskqueue[TQ_IDX(tail_idx)].task_desc = *tdesc;
				// Flush the task descriptor.
				RigelFlushLine(&GLOBAL_swtq_taskqueue[TQ_IDX(tail_idx)]);
			}
			// Adjust the tail point to reflect the number of tasks just added
			RigelGlobalStore(tail_idx, GLOBAL_fixedtq_tail_index);
		}
		// XXX: RELEASE LOCK [[ENQUEUE]]
		spin_unlock(&GLOBAL_fixedtq_enqueue_lock);
	}

	return TQ_RET_SUCCESS;
}

TQ_RetType 
SW_TaskQueue_Dequeue_FIXED(int qID, TQ_TaskDesc *tdesc) 
{
	// Once a reservation is made, we own this index in the queue
	unsigned int head_idx;
	// (head_idx - 1) is our reservation since atom.inc returns new (incremented)
	// value.
	RigelAtomicINC(head_idx, GLOBAL_fixedtq_head_index);
	head_idx--;

try_dequeue:
	{
		unsigned int _head_idx, _tail_idx, tail_idx;

		// Grab the tail and check if we grabbed an entry that is valid
		RigelGlobalLoad(tail_idx, GLOBAL_fixedtq_tail_index);

		_head_idx = head_idx;
		_tail_idx = tail_idx;
		// Check for wraparound
		if (head_idx > (tail_idx + TASK_QUEUE_SIZE)) {
			_tail_idx = tail_idx + TASK_QUEUE_SIZE;
			_head_idx = TQ_IDX(head_idx);
		} 

		// Entry inserted, dequeue it
		if (_head_idx < _tail_idx) {
			// Get descriptor from task queue
			*tdesc = GLOBAL_swtq_taskqueue[TQ_IDX(head_idx)].task_desc;
	
			return TQ_RET_SUCCESS;
		}
	}

	// Task queue looks to be empty, enter the barrier provisionally
enter_barrier:
	{
		int core_num = RigelGetCoreNum();
		// Flip Local Sense
		LOCAL_fixedtq_sense[core_num] = (0 == LOCAL_fixedtq_sense[core_num]) ? 1 : 0;

		while (1) {
			unsigned int _head_idx, _tail_idx, tail_idx;
			int sense;

			// Check the sense to see if we have already left the barrier
			RigelGlobalLoad(sense, GLOBAL_fixedtq_sense);
			if (sense == LOCAL_fixedtq_sense[core_num]) {
				return TQ_RET_SYNC;
			}

			RigelGlobalLoad(tail_idx, GLOBAL_fixedtq_tail_index);

			_head_idx = head_idx;
			_tail_idx = tail_idx;
			// Check for wraparound
			if (head_idx > (tail_idx+TASK_QUEUE_SIZE)) {
				_head_idx = TQ_IDX(head_idx);
				_tail_idx = tail_idx + TASK_QUEUE_SIZE;
			}

			// The task queue is not empty!
			if (_head_idx < _tail_idx) {
				// Barrier is no longer in progress.  Back out.
				LOCAL_fixedtq_sense[core_num] = (0 == LOCAL_fixedtq_sense[core_num]) ? 1 : 0;

				// Get descriptor from task queue
				*tdesc = GLOBAL_swtq_taskqueue[TQ_IDX(_head_idx)].task_desc;
		
				return TQ_RET_SUCCESS;

			} else if (_head_idx == (_tail_idx+GLOBAL_swtq_numcores-1)) {
				// Actual barrier reached.  No turning back...Reset head/tail indices and
				// then notify other cores by setting sense.  
				RigelGlobalStore(tail_idx, GLOBAL_fixedtq_head_index);
				RigelGlobalStore(LOCAL_fixedtq_sense[core_num], GLOBAL_fixedtq_sense);
	
				return TQ_RET_SYNC;
			}
		}
	}
}

