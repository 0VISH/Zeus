#include "basic.hh"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#if(DBG)
#include <stdio.h>
#endif

#define CHUNK_SIZE  (sizeof(u64)*2)
#define CHUNK_COUNT 10000000

namespace allocator{
    void *alloc(u64 size, char *memory, bool *stat){
#if(DBG)
	if(size == 0){
	    printf("\n[MEM]: trying to allocate memory of size 0\n");
	    return nullptr;
	};
#endif
	/*
	  Each allocation remembers the amount of blocks it asked for
	  This decreases cache miss
	*/
	size += sizeof(u32);
	u32 chunkReq = ceil(size/((double)(CHUNK_SIZE)));
	u32 i = 0;
    MEM_FIND_CHUNKS:
	u32 startOff = i;
	u32 chunkFound = 0;
	while(stat[i] == false){
	    chunkFound += 1;
	    if(chunkFound == chunkReq){
		char *ptr = memory+(startOff*CHUNK_SIZE);
		u32 *intPtr = (u32*)ptr;
		*intPtr = chunkReq;
		ptr += sizeof(u32);
		memset(&stat[startOff], true, sizeof(bool) * chunkFound);
		return(void*)ptr;
	    };
	    i += 1;
	};
	if(i+chunkReq >= CHUNK_COUNT){
#if(DBG)
	    printf("\n[MEM]: out of chunks. Please increase CHUNK_COUNT\n");
#endif
	    return nullptr;
	};
	while(stat[i]){i += 1;};
	goto MEM_FIND_CHUNKS;
	return nullptr;
    };
    void free(void *ptr, char *memory, bool *stat){
	char *cptr = (char*)ptr;
	cptr -= sizeof(u32);
	u32 off = (cptr - memory)/CHUNK_SIZE;
	u32 chunks = *((u32*)cptr);
	memset(&stat[off], false, sizeof(bool)*chunks);
#if(DBG)
	memset(cptr, 'A', CHUNK_SIZE*chunks);
#endif
    };
};
namespace mem{
    char *memory;
    bool *stat;
#if(DBG)
    u32 allocCount;
#endif
    
    void init(){
	allocCount = 0;
	memory = (char*)malloc(CHUNK_SIZE   * CHUNK_COUNT);
	const u64 statSize = sizeof(bool) * (CHUNK_COUNT + 1);   //NOTE: +1 for padding
	stat   = (bool*)malloc(statSize);
	memset(stat, false, statSize);
	stat[CHUNK_COUNT] = true;
#if(DBG)
	memset(memory, 'A', CHUNK_SIZE * CHUNK_COUNT);
#endif
    };
    void uninit(){
	::free(memory);
	::free(stat);
    };
    void *alloc(u64 size){
#if(DBG)
	allocCount += 1;
#endif
	return allocator::alloc(size, memory, stat);
    };
    void *calloc(u64 size){
	void *ptr = alloc(size);
	memset(ptr, 0, size);
	return ptr;
    };
    void free(void *ptr){
#if(DBG)
	if(allocCount == 0){
	    printf("[MEM]: allocCount is 0. Trying to free another pointer\n");
	};
	allocCount -= 1;
#endif
	return allocator::free(ptr, memory, stat);
    };
};