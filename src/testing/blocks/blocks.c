#ifndef LLVM28
#error Need LLVM 2.8 for C Blocks support
#endif

#include <stdio.h>
#include <Block.h>

typedef int (^IntBlock)();
typedef struct {
	IntBlock forward;
	IntBlock backward;
} Counter;

Counter MakeCounter(int start, int increment) {
	Counter counter;

	__block int i = start;

	counter.forward = Block_copy( ^ {
		i += increment;
		return i;
	});
	counter.backward = Block_copy( ^ {
		i -= increment;
		return i;
	});

	return counter;

}

int main() {
	Counter counter = MakeCounter(5, 2);
	printf("Forward one: %d\n", counter.forward());
	printf("Forward one more: %d\n", counter.forward());
	printf("Backward one: %d\n", counter.backward());

	Block_release(counter.forward);
	Block_release(counter.backward);

	return 0;
}
/* Outputs:
Forward one: 7
Forward one more: 9
Backward one: 7
*/
