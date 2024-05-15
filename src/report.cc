#define MAX_ERRORS 10
#define MAX_WARNINGS 10

#ifdef _MSC_VER
#if SIMD
#define __builtin_popcount __popcnt
#endif
#endif

namespace report{

    struct Report {
        u64   off;
        char *fileName;
        char *msg;
        char *fileContent;
    };

    Report errors[MAX_ERRORS];
    u8 errorOff = 0;
    char reportBuff[1024];
    u32 reportBuffTop = 0;

    char *getLineAndOff(char *mem, u64 offset, u32 &line, u32 &off) {
#if(SIMD)
	__m128i tocmp =  _mm_set1_epi8('\n');
	u32 x = 0;
	while (true) {
	    if (x + 16 > offset){
		while (x<offset) {
		    if (mem[x] == '\n') { line += 1; };
		    x += 1;
		};
		break;
	    };
	    __m128i chunk = _mm_loadu_si128 ((__m128i const*)(mem+x));
	    __m128i results =  _mm_cmpeq_epi8(chunk, tocmp);
	    s32 mask = _mm_movemask_epi8(results);
	    line += __builtin_popcount(mask);
	    x += 16;
	}

#else
	for (u64 i = 0; i < offset; i += 1) {
	    if (mem[i] == '\n') { line += 1; };
	};
#endif
	while (mem[offset-off] != '\n') {
	    off += 1;
	};
	return mem + offset-off+1;
    };
	
    void flushReports() {
		os::setPrintColorToWhite();
		if(errorOff != 0){
			//printf in FILO so that the usr can see first report first in the terminal
			for (u8 i = errorOff; i != 0;) {
				i -= 1;
				Report &rep = errors[i];
				u32 line = 1;
				u32 off = 1;
				char *beg = getLineAndOff(rep.fileContent, rep.off, line, off);
				u32 x = 0;
				while (beg[x] != '\n' && beg[x] != '\0') {
				if (beg[x] == '\t') { beg[x] = ' '; }; //replace tabs with spaces for ez reporting and reading
				x += 1;
				};
				beg[x] = '\0';
				bool printDots = false;
				while(off > 40){
				off -= 40;
				beg += 40;
				printDots = true;
				}
				printf("\n%s: ", rep.fileName);
				os::printErrorInRed();
				printf(" %s\n", rep.msg);
				printf("  %d| ", line);
				if(printDots){
				printf("...");
				};
				printf("%s\n____", beg);
				if(printDots){
				printf("___");
				}
				u32 n = line;
				while (n > 0) {
				printf("_");
				n = n / 10;
				}
				while (off != 1) {
				printf("_");
				off -= 1;
				};
				printf("^\n");
			};
			printf("\nerror: %d\n", errorOff);
		};
    };
}