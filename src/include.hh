#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.hh"
#if(SIMD)
#include <immintrin.h>
#endif
#include "basic.hh"
#include "mem.cc"
#if(LINUX)
#include "os/linux.cc"
#endif

#include "ds.cc"
#include "report.cc"

#include "frontend/lexer.cc"