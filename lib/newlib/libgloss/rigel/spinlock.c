#include <spinlock.h>

int
atomic_flag_check(volatile atomic_flag_t *f) {
	volatile unsigned int *flag_addr = &f->flag_val;
	return (1 == *flag_addr);
}

void 
atomic_flag_set(volatile atomic_flag_t *f) {
	volatile unsigned int *flag_addr = &f->flag_val;
	*flag_addr = 1;
}

void 
atomic_flag_clear(volatile atomic_flag_t *f) {
	volatile unsigned int *flag_addr = &f->flag_val;
	*flag_addr = 0;
}

void 
atomic_flag_spin_until_clear(volatile atomic_flag_t *f) {
	volatile unsigned int *flag_addr = &f->flag_val;
	while(*flag_addr != 0);
}

		
void 
atomic_flag_spin_until_set(volatile atomic_flag_t *f) {
	volatile unsigned int *flag_addr = &f->flag_val;
	while(*flag_addr == 0);
}
