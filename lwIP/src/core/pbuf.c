#include "opt.h"

#include "stats.h"
#include "def.h"
#include "mem.h"
#include "memp.h"
#include "pbuf.h"
#include "sys.h"
#include "arch/perf.h"
#include "dhcp.h"  // for include struct dhcp_msg
#if TCP_QUEUE_OOSEQ
#include "tcp.h"
#endif

#include <string.h>

#define SIZEOF_STRUCT_PBUF        LWIP_MEM_ALIGN_SIZE(sizeof(struct pbuf))
/* Since the pool is created in memp, PBUF_POOL_BUFSIZE will be automatically
   aligned there. Therefore, PBUF_POOL_BUFSIZE_ALIGNED can be used here. */
#define PBUF_POOL_BUFSIZE_ALIGNED LWIP_MEM_ALIGN_SIZE(PBUF_POOL_BUFSIZE)

#if TCP_QUEUE_OOSEQ
#define ALLOC_POOL_PBUF(p) do { (p) = alloc_pool_pbuf(); } while (0)
#else
#define ALLOC_POOL_PBUF(p) do { (p) = memp_malloc(MEMP_PBUF_POOL); } while (0)
#endif

#define PbufEntry 3
static OS_MEM pbuf_pool;
static char ram_dhcp_pool[PbufEntry][1500];

void pbuf_init(void){
    OS_ERR err3;
    OSMemCreate((OS_MEM*    ) &pbuf_pool,
                (CPU_CHAR*  ) "pbuf_pool",
                (void*      ) &ram_dhcp_pool,
                (CPU_SIZE_T ) PbufEntry,
                (CPU_SIZE_T ) 1500,
                (OS_ERR*    ) &err3);
    
    if(err3 != OS_ERR_NONE){
        APP_TRACE_INFO(("pbuf_init: OSMemCreate error: %d\n", err3));
    }
    APP_TRACE_INFO(("pbuf_init: pbuf_pool: %d\n", pbuf_pool));
}

struct pbuf *pbuf_alloc(pbuf_layer layer, u16_t length, pbuf_type type){
    struct pbuf *p;
    OS_ERR err3;

    u16_t offset = 0;
    switch (layer) {
        case PBUF_TRANSPORT:
            /* add room for transport (often TCP) layer header */
        case PBUF_IP:
            /* add room for IP layer header */
            offset += PBUF_IP_HLEN;
        case PBUF_LINK:
            /* add room for link layer header */
            offset += PBUF_LINK_HLEN;
            break;
        case PBUF_RAW: break;
        default: return NULL;
    }

    switch(type){
        case PBUF_POOL:
            APP_TRACE_INFO(("pbuf_alloc, PBUF_POOL: not defined yet\n"));
            break;
        case PBUF_RAM:
            p = (struct pbuf*) OSMemGet(&pbuf_pool, &err3);
            if(err3 != OS_ERR_NONE){
                APP_TRACE_INFO(("pbuf_alloc, PBUF_RAM: OSMemGet error: %d\n", err3));
                return NULL;
            }

            p->payload = LWIP_MEM_ALIGN((void *)((u8_t *)p + SIZEOF_STRUCT_PBUF + offset));
            p->len = p->tot_len = length;
            p->next = NULL;
            p->type = type;
            break;
        case PBUF_ROM:
        case PBUF_REF:
            p = (struct pbuf *) OSMemGet(&pbuf_pool, &err3);
            if(err3 != OS_ERR_NONE){
                APP_TRACE_INFO(("pbuf_alloc, PBUF_REF: OSMemGet error: %d\n", err3));
                return NULL;
            }
            p->payload = NULL;
            p->len = p->tot_len = length;
            p->next = NULL;
            p->type = type;
            break;
        default: return NULL;
    }

    p->ref = 1;
    p->flags = 0;
    return p;
}
void pbuf_realloc(struct pbuf *p, u16_t size){} 

/**
 * Adjusts the payload pointer to hide or reveal headers in the payload.
 *
 * Adjusts the ->payload pointer so that space for a header
 * (dis)appears in the pbuf payload.
 *
 * The ->payload, ->tot_len and ->len fields are adjusted.
 *
 * @param p pbuf to change the header size.
 * @param header_size_increment Number of bytes to increment header size which
 * increases the size of the pbuf. New space is on the front.
 * (Using a negative value decreases the header size.)
 * If hdr_size_inc is 0, this function does nothing and returns succesful.
 *
 * PBUF_ROM and PBUF_REF type buffers cannot have their sizes increased, so
 * the call will fail. A check is made that the increase in header size does
 * not move the payload pointer in front of the start of the buffer.
 * @return non-zero on failure, zero on success.
 *
 */
u8_t pbuf_header(struct pbuf *p, s16_t header_size_increment){
    u16_t type;
    void *payload;
    u16_t increment_magnitude;

    LWIP_ASSERT("p != NULL", p != NULL);
    if ((header_size_increment == 0) || (p == NULL))
        return 0;

    if (header_size_increment < 0){
        increment_magnitude = -header_size_increment;
        /* Check that we aren't going to move off the end of the pbuf */
        LWIP_ERROR("increment_magnitude <= p->len", (increment_magnitude <= p->len), return 1;);
    } 
    else increment_magnitude = header_size_increment;

    type = p->type;
    /* remember current payload pointer */
    payload = p->payload;

    /* pbuf types containing payloads? */
    if(type == PBUF_RAM || type == PBUF_POOL){
        /* set new payload pointer */
        p->payload = (u8_t *)p->payload - header_size_increment;
        /* boundary check fails? */
        if ((u8_t *)p->payload < (u8_t *)p + SIZEOF_STRUCT_PBUF) {
            LWIP_DEBUGF( PBUF_DEBUG | 2, ("pbuf_header: failed as %p < %p (not enough space for new header size)\n",
            (void *)p->payload,
            (void *)(p + 1)));\
            /* restore old payload pointer */
            p->payload = payload;
            /* bail out unsuccesfully */
            return 1;
        }
    /* pbuf types refering to external payloads? */
    }
    else if(type == PBUF_REF || type == PBUF_ROM){
        /* hide a header in the payload? */
        if ((header_size_increment < 0) && (increment_magnitude <= p->len)) {
            /* increase payload pointer */
            p->payload = (u8_t *)p->payload - header_size_increment;
        }
        else return 1;
    }
    else return 1;

    /* modify pbuf length fields */
    p->len += header_size_increment;
    p->tot_len += header_size_increment;

    return 0;
}

u8_t pbuf_free(struct pbuf *p){
    u8_t count = 0;
    struct pbuf *q;
    OS_ERR err3;

    if(p == NULL){
        APP_TRACE_INFO(("pbuf_free: p is NULL\n"));
        return 0;
    }
    while(p != NULL){
        u16_t ref = --(p->ref);
        if(ref == 0){
            q = p->next;

            OSMemPut(&pbuf_pool, p, &err3);
            if(err3 != OS_ERR_NONE){
                APP_TRACE_INFO(("pbuf_free: OSMemPut error: %d\n", err3));
                return NULL;
            }
            count++;
            p = q;
        }
        else break;
    }

    return count;
}

/**
 * Count number of pbufs in a chain
 *
 * @param p first pbuf of chain
 * @return the number of pbufs in a chain
 */
u8_t pbuf_clen(struct pbuf *p){
    u8_t len = 0;
    while(p != NULL){
        len++;
        p = p->next;
    }
    return len;
}

/**
 * Increment the reference count of the pbuf.
 *
 * @param p pbuf to increase reference counter of
 *
 */
void pbuf_ref(struct pbuf *p){
    if(p != NULL) (p->ref)++;
}

/**
 * Concatenate two pbufs (each may be a pbuf chain) and take over
 * the caller's reference of the tail pbuf.
 * 
 * @note The caller MAY NOT reference the tail pbuf afterwards.
 * Use pbuf_chain() for that purpose.
 * 
 * @see pbuf_chain()
 */
void pbuf_cat(struct pbuf *head, struct pbuf *tail){
    struct pbuf *p;

    for(p=head; p->next != NULL; p=p->next){
        p->tot_len += tail->tot_len;
    }

    p->tot_len += tail->tot_len;
    p->next = tail;
}

/**
 * Chain two pbufs (or pbuf chains) together.
 * 
 * The caller MUST call pbuf_free(t) once it has stopped
 * using it. Use pbuf_cat() instead if you no longer use t.
 * 
 * @param h head pbuf (chain)
 * @param t tail pbuf (chain)
 * @note The pbufs MUST belong to the same packet.
 * @note MAY NOT be called on a packet queue.
 *
 * The ->tot_len fields of all pbufs of the head chain are adjusted.
 * The ->next field of the last pbuf of the head chain is adjusted.
 * The ->ref field of the first pbuf of the tail chain is adjusted.
 *
 */
void pbuf_chain(struct pbuf *head, struct pbuf *tail){
    pbuf_cat(head, tail);
    pbuf_ref(tail);
}

/**
 * Dechains the first pbuf from its succeeding pbufs in the chain.
 *
 * Makes p->tot_len field equal to p->len.
 * @param p pbuf to dechain
 * @return remainder of the pbuf chain, or NULL if it was de-allocated.
 * @note May not be called on a packet queue.
 */
struct pbuf *pbuf_dechain(struct pbuf *p){
    struct pbuf *q = p->next;
    u8_t tail_gone = 1;

    if(q != NULL){
        q->tot_len = p->tot_len - p->len;
        p->next = NULL;
        p->tot_len = p->len;

        tail_gone = pbuf_free(q);
        if(tail_gone > 0){
            APP_TRACE_INFO(("pbuf_dechain: tail_gone > 0\n"));
        }
    }

    return ((tail_gone > 0) ? NULL : q);
}

/**
 *
 * Create PBUF_RAM copies of pbufs.
 *
 * Used to queue packets on behalf of the lwIP stack, such as
 * ARP based queueing.
 *
 * @note You MUST explicitly use p = pbuf_take(p);
 *
 * @note Only one packet is copied, no packet queue!
 *
 * @param p_to pbuf destination of the copy
 * @param p_from pbuf source of the copy
 *
 * @return ERR_OK if pbuf was copied
 *         ERR_ARG if one of the pbufs is NULL or p_to is not big
 *                 enough to hold p_from
 */
err_t pbuf_copy(struct pbuf *p_to, struct pbuf *p_from){
    u16_t offset_to=0, offset_from=0, len;

    do{
        if ((p_to->len - offset_to) >= (p_from->len - offset_from)) {
            /* complete current p_from fits into current p_to */
            len = p_from->len - offset_from;
        } else {
            /* current p_from does not fit into current p_to */
            len = p_to->len - offset_to;
        }
        
        MEMCPY((u8_t*)p_to->payload + offset_to, (u8_t*)p_from->payload + offset_from, len);
        offset_to += len;
        offset_from += len;
        
        if(offset_to <= p_to->len) APP_TRACE_INFO(("offset_to <= p_to->len\n"));
        if(offset_to == p_to->len) {
            /* on to next p_to (if any) */
            offset_to = 0;
            p_to = p_to->next;
        }
        if(offset_from <= p_from->len) APP_TRACE_INFO(("offset_from <= p_from->len\n"));
        if (offset_from >= p_from->len) {
            /* on to next p_from (if any) */
            offset_from = 0;
            p_from = p_from->next;
        }

        if((p_from != NULL) && (p_from->len == p_from->tot_len)) {
            /* don't copy more than one packet! */
            LWIP_ERROR("pbuf_copy() does not allow packet queues!\n",
                        (p_from->next == NULL), return ERR_VAL;);
        }
        if((p_to != NULL) && (p_to->len == p_to->tot_len)) {
            /* don't copy more than one packet! */
            LWIP_ERROR("pbuf_copy() does not allow packet queues!\n",
                        (p_to->next == NULL), return ERR_VAL;);
        }
    }while(p_from);

    return ERR_OK;
}

/**
 * Copy (part of) the contents of a packet buffer
 * to an application supplied buffer.
 *
 * @param buf the pbuf from which to copy data
 * @param dataptr the application supplied buffer
 * @param len length of data to copy (dataptr must be big enough). No more 
 * than buf->tot_len will be copied, irrespective of len
 * @param offset offset into the packet buffer from where to begin copying len bytes
 * @return the number of bytes copied, or 0 on failure
 */
u16_t pbuf_copy_partial(struct pbuf *buf, void *dataptr, u16_t len, u16_t offset){
    struct pbuf *p;
    u16_t left = 0;
    u16_t buf_copy_len;
    u16_t copied_total = 0;

    if(buf != NULL) APP_TRACE_INFO(("pbuf_copy_partial: invalid buf\n"));
    if(dataptr != NULL) APP_TRACE_INFO(("pbuf_copy_partial: invalid dataptr\n"));

    if((buf == NULL) || (dataptr == NULL)) return 0;

    /* Note some systems use byte copy if dataptr or one of the pbuf payload pointers are unaligned. */
    for(p = buf; len != 0 && p != NULL; p = p->next) {
        if ((offset != 0) && (offset >= p->len)) {
            /* don't copy from this buffer -> on to the next */
            offset -= p->len;
        } else {
            /* copy from this buffer. maybe only partially. */
            buf_copy_len = p->len - offset;
            if (buf_copy_len > len)
                buf_copy_len = len;
            /* copy the necessary parts of the buffer */
    
            MEMCPY(&((char*)dataptr)[left], &((char*)p->payload)[offset], buf_copy_len);
            copied_total += buf_copy_len;
            left += buf_copy_len;
            len -= buf_copy_len;
            offset = 0;
        }
    }
    return copied_total;
}

/**
 * Copy application supplied data into a pbuf.
 * This function can only be used to copy the equivalent of buf->tot_len data.
 *
 * @param buf pbuf to fill with data
 * @param dataptr application supplied data buffer
 * @param len length of the application supplied data buffer
 *
 * @return ERR_OK if successful, ERR_MEM if the pbuf is not big enough
 */
err_t
pbuf_take(struct pbuf *buf, const void *dataptr, u16_t len)
{
  struct pbuf *p;
  u16_t buf_copy_len;
  u16_t total_copy_len = len;
  u16_t copied_total = 0;

  LWIP_ERROR("pbuf_take: invalid buf", (buf != NULL), return 0;);
  LWIP_ERROR("pbuf_take: invalid dataptr", (dataptr != NULL), return 0;);

  if ((buf == NULL) || (dataptr == NULL) || (buf->tot_len < len)) {
    return ERR_ARG;
  }

  /* Note some systems use byte copy if dataptr or one of the pbuf payload pointers are unaligned. */
  for(p = buf; total_copy_len != 0; p = p->next) {
    LWIP_ASSERT("pbuf_take: invalid pbuf", p != NULL);
    buf_copy_len = total_copy_len;
    if (buf_copy_len > p->len) {
      /* this pbuf cannot hold all remaining data */
      buf_copy_len = p->len;
    }
    /* copy the necessary parts of the buffer */
    MEMCPY(p->payload, &((char*)dataptr)[copied_total], buf_copy_len);
    total_copy_len -= buf_copy_len;
    copied_total += buf_copy_len;
  }
  LWIP_ASSERT("did not copy all data", total_copy_len == 0 && copied_total == len);
  return ERR_OK;
}

/**
 * Creates a single pbuf out of a queue of pbufs.
 *
 * @remark: The source pbuf 'p' is not freed by this function because that can
 *          be illegal in some places!
 *
 * @param p the source pbuf
 * @param layer pbuf_layer of the new pbuf
 *
 * @return a new, single pbuf (p->next is NULL)
 *         or the old pbuf if allocation fails
 */
struct pbuf*
pbuf_coalesce(struct pbuf *p, pbuf_layer layer)
{
  struct pbuf *q;
  err_t err;
  if (p->next == NULL) {
    return p;
  }
  q = pbuf_alloc(layer, p->tot_len, PBUF_RAM);
  if (q == NULL) {
    /* @todo: what do we do now? */
    return p;
  }
  err = pbuf_copy(q, p);
  LWIP_ASSERT("pbuf_copy failed", err == ERR_OK);
  pbuf_free(p);
  return q;
}