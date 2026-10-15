#pragma once

/* MSVC has no fcntl.h. The binary mode flag lives in io.h. */
#include <io.h>

#ifndef _O_BINARY
 #define _O_BINARY 0x8000
#endif
