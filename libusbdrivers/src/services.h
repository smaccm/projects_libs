/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdlib.h>
#include <platsupport/io.h>
#include <platsupport/delay.h>
#include "debug.h"
#include <assert.h>
#include <utils/util.h>

void otg_irq(void);

#ifdef ARCH_ARM
#define udelay(ms)  ps_udelay(ms)
#else
static inline void udelay(uint32_t us)
{
	volatile uint32_t i;
	for (; us > 0; us--) {
		for (i = 0; i < 1000; i++);
	}
}
#endif
#define msdelay(ms) udelay((ms) * 1000)

#define usb_assert(test)         \
        do{                      \
            assert(test);        \
        } while(0)


#define usb_malloc(...) calloc(1, __VA_ARGS__)
#define usb_free(...) free(__VA_ARGS__)

#define MAP_DEVICE(o, p, s) ps_io_map(&o->io_mapper, p, s, 0, PS_MEM_NORMAL)

#define GET_RESOURCE(ops, id) MAP_DEVICE(ops, id##_PADDR, id##_SIZE)

#ifdef ARCH_ARM
#define dsb() asm volatile("dsb")
#define isb() asm volatile("isb")
#define dmb() asm volatile("dmb")
#else
#define dsb() asm volatile ("" ::: "memory")
#define isb() asm volatile ("" ::: "memory")
#define dmb() asm volatile ("mfence" ::: "memory")
#endif

static inline void*
ps_dma_alloc_pinned(ps_dma_man_t *dma_man, size_t size, int align, int cache,
	       ps_mem_flags_t flags, uintptr_t* paddr)
{
    void* addr;
    assert(dma_man);
    addr = ps_dma_alloc(dma_man, size, align, cache, flags);
    if (addr != NULL) {
        *paddr = ps_dma_pin(dma_man, addr, size);
    }
    return addr;
}

static inline void
ps_dma_free_pinned(ps_dma_man_t *dma_man, void* addr, size_t size)
{
    assert(dma_man);
    ps_dma_unpin(dma_man, addr, size);
    ps_dma_free(dma_man, addr, size);
}


