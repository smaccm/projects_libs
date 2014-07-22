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

#define _malloc(...) malloc(__VA_ARGS__)
#define _free(...) free(__VA_ARGS__)

#ifdef CAMKES
#define RESOURCE(o, id) id##_VADDR
#else
#define RESOURCE(o, id) sdhc_map_device(o, id##_PADDR, id##_SIZE)
#endif

static inline void
udelay(long _us){
    volatile long us = _us;
    while(us--){
        volatile long i = 1000;
        while(i--);
    }
}

/**
 * Maps in device memory
 * @param[in] o     A reference to the services provided
 * @param[in] paddr the physical address of the device
 * @param[in] size  the size of the region in bytes
 * @return          the virtual address of the mapping.
 *                  NULL on failure.
 */
static inline void* 
sdhc_map_device(struct ps_io_mapper* o, uintptr_t paddr, int size){
    return ps_io_map(o, (void*)paddr, size, 0, PS_MEM_NORMAL);
}

