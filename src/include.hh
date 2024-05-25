#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if(SIMD)
#include <immintrin.h>
#endif
#if(WIN)
#include "windows.h"
#elif(LIN)
#include <unistd.h>
#include <fcntl.h>
#endif

#include "basic.hh"
#include "mem.cc"
#include "ds.cc"

#include "report.cc"
#include "lexer.cc"
#include "type.hh"
#include "parser.cc"
#include "checker.cc"

#include "riscvAsm.cc"