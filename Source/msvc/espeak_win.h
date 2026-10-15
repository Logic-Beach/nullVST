#pragma once

#include <sys/stat.h>

/* The dictionary tool checks for a pipe. MSVC's stat has the mode bit only. */
#ifndef S_ISFIFO
 #define S_ISFIFO(mode) (((mode) & _S_IFMT) == _S_IFIFO)
#endif
