#include "opt.h"

// #if !mem_malloc /* don't build if not configured for use in lwipopts.h */

#include "def.h"
#include "mem.h"
#include "sys.h"
#include "stats.h"
#include "memp.h"

#include <string.h>

// #if MEM_USE_POOLS
// #else /* MEM_USE_POOLS */
void mem_init(){
    APP_TRACE_INFO(("mem_init called\n"));
    OS_ERR err3;

    OSSemCreate(&memSem, "memSem", 1, &err3);
    if(err3 != OS_ERR_NONE){
        APP_TRACE_INFO(("mem_init: OSSemCreate failed %d\n", err3));
        return;
    }

    bMemPoolAddr = &bMemPoolArray[0][0];
    OSMemCreate(&bMemPool, "bMemPool", &bMemPoolArray[0], 3, 1500 + sizeof(void *), &err3);
    if(err3 != OS_ERR_NONE){
        APP_TRACE_INFO(("memp_init: OSMemCreate(bMemPool) failed %d\n", err3));
        return;
    }

    sMemPoolAddr = &sMemPoolArray[0][0];
    OSMemCreate(&sMemPool, "sMemPool", &sMemPoolArray[0], 10, 160 + sizeof(void *), &err3);
    if(err3 != OS_ERR_NONE){
        APP_TRACE_INFO(("memp_init: OSMemCreate(sMemPool) failed %d\n", err3));
        return;
    }
    // OSMemCreate(&timeoutMemp, "timeoutMemp", &timeoutMemp_memory[0], 5, sizeof(struct sys_timeo) + sizeof(void *), &err3);
    // if(err3 != OS_ERR_NONE){
    //     APP_TRACE_INFO(("memp_init: OSMemCreate(timeoutMemp) failed %d\n", err3));
    //     return;
    // }
    APP_TRACE_INFO(("bMemPoolAddr = %p  | sMemPoolAddr = %p\n\r", bMemPoolAddr, sMemPoolAddr));
    APP_TRACE_INFO(("mem_init done\n"));
}

void mem_free(void* mem){
    OS_ERR err3;

    // bMemPoolArray is defined first
    // so address is lower than sMemPoolArray
    if((char*)mem < sMemPoolAddr) OSMemPut(&bMemPool, mem, &err3);
    else OSMemPut(&sMemPool, mem, &err3);

    if(err3 != OS_ERR_NONE){
        APP_TRACE_INFO(("mem_free: OSMemPut failed %d\n", err3));
    }
}

void* mem_realloc(void *mem, mem_size_t newsize){
    return mem;
}

void * mem_malloc(mem_size_t size){
    OS_ERR err3;
    
    void* mem = (size <= 160) ? OSMemGet(&sMemPool, &err3) : OSMemGet(&bMemPool, &err3);
    if(err3 != OS_ERR_NONE){
        APP_TRACE_INFO(("mem_malloc: OSMemGet failed %d\n", err3));
        return NULL;
    }
    return mem;
}

// #endif /* MEM_USE_POOLS */

void *mem_calloc(mem_size_t count, mem_size_t size){
    return mem_malloc(count * size);
}