#include <pthread.h>
#include <rigel.h>

//http://pubs.opengroup.org/onlinepubs/009695399/functions/pthread_once.html

//TODO I'd rather not have these all over the place, I'd like them all in one place.
//Having them in rigel.h means (I think) that you can't have them as static inline functions
//(or can you?), but macros are totally the wrong way to do it if you can inline the function.
//Another way to do it might be to make them LLVM intrinsics.
static inline int compare_and_swap(const void *ptr, const void *compareval, void *swapval)
{
	  __asm__ __volatile__ ("atom.cas %0, %2, %1" \
				                  : "+&r" (swapval)     \
										      : "r"   (ptr),        \
										        "r"   (compareval)  \
										      : "memory");
		  return (swapval == compareval);
}

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
  if(compare_and_swap((void *)once_control, (void *)0, (void *)1))
		init_routine();
  //TODO: "The pthread_once() function is not a cancellation point.
	//However, if init_routine is a cancellation point and is canceled,
	//the effect on once_control shall be as if pthread_once() was never called."
	//TODO: "The pthread_once() function may fail if:
	//[EINVAL]
	//	If either once_control or init_routine is invalid. (Question: what does 'invalid' mean?)

	return 0;
}
