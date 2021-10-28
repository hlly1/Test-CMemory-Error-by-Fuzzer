#ifndef _DEBUG_H_
#define _DEBUG_H_

#ifdef DEBUG_NO_PRINTF
#define printf(...)
#endif

#ifndef NDEBUG
#include <stdio.h>
#define debug_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define debug_printf(...)
#endif

#endif
