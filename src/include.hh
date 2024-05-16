#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if(SIMD)
#include <immintrin.h>
#endif
#if(WIN)
#include "windows.h"
#endif

#include "basic.hh"
#include "mem.cc"
#include "ds.cc"
#include "threadPool.cc"

#include "report.cc"
#include "frontend/lexer.cc"
#include "frontend/type.hh"
#include "frontend/parser.cc"
#include "frontend/checker.cc"