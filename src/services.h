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


#define udelay(ms)  ps_udelay(ms)
#define msdelay(ms) ps_mdelay(ms)


#define usb_assert(test)         \
        do{                      \
            assert(test);        \
        } while(0)


#define usb_malloc(...) malloc(__VA_ARGS__)
#define usb_free(...) free(__VA_ARGS__)

#define MAP_DEVICE(o, p, s) ps_io_map(&o->io_mapper, p, s, 0, PS_MEM_NORMAL)

#define GET_RESOURCE(ops, id) MAP_DEVICE(ops, id##_PADDR, id##_SIZE)



#define dsb() asm volatile("dsb")
#define isb() asm volatile("isb")
#define dmb() asm volatile("dmb")


