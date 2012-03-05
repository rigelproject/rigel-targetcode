#ifndef _STDDEF_H
#define _STDDEF_H

#define NULL 0

#ifndef __PTRDIFF_TYPE__
#define __PTRDIFF_TYPE__ long int
#endif
typedef __PTRDIFF_TYPE__ ptrdiff_t;

#ifndef __SIZE_TYPE__
#define __SIZE_TYPE__ long unsigned int
#endif
typedef __SIZE_TYPE__ size_t;

//wchar_t/wint_t are builtin types in C++, can't typedef them.
//See http://lists.cs.uiuc.edu/pipermail/cfe-dev/2010-January/007460.html
#ifndef __cplusplus
#ifndef __WCHAR_TYPE__
#define __WCHAR_TYPE__ int
#endif
typedef __WCHAR_TYPE__ wchar_t;

#ifndef __WINT_TYPE__
#define __WINT_TYPE__ unsigned int
#endif
typedef __WINT_TYPE__ wint_t;
#endif

#define offsetof(type, member)  __builtin_offsetof (type, member)

#endif /* #ifndef _STDDEF_H */

