#include "opt.h"

#include "memp.h"
#include "pbuf.h"
#include "udp.h"
#include "raw.h"
#include "tcp.h"
#include "igmp.h"
#include "api.h"
#include "api_msg.h"
#include "tcpip.h"
#include "sys.h"
#include "stats.h"
#include "etharp.h"
#include "ip_frag.h"
#include "os.h"

#include <string.h>

/*
    i will set
    MEMP_SANITY_CHECK = 0
    MEMP_OVERFLOW_CHECK = 0
    
    so related code will be removed
*/

/**
 * Initialize this module.
 * 
 * Carves out memp_memory into linked lists for each pool-type.
 */
// void memp_init(void){
//     OS_ERR err3;
//     OSMemCreate(&memp, "memp", &memp_memory[0], 20, 1500 + sizeof(void *), &err3);

//     if(err3 != OS_ERR_NONE){
//         APP_TRACE_INFO(("memp_init: OSMemCreate failed %d\n", err3));
//     }
// }

// void * memp_malloc(memp_t type){
//     OS_ERR err3;
//     void* mem = OSMemGet(&memp, &err3);

//     if(err3 != OS_ERR_NONE){
//         APP_TRACE_INFO(("memp_malloc: OSMemGet failed %d\n", err3));
//         return NULL;
//     }
//     return mem;
// }

// void memp_free(memp_t type, void *mem){
//     OS_ERR err3;
//     OSMemPut(&memp, mem, &err3);

//     if(err3 != OS_ERR_NONE){
//         APP_TRACE_INFO(("memp_free: OSMemPut failed %d\n", err3));
//     }
// }