/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _OTG_USBTTY_H_
#define _OTG_USBTTY_H_

#include <usb/otg.h>

struct otg_usbtty;
typedef struct otg_usbtty* otg_usbtty_t;

int otg_usbtty_init(usb_otg_t otg, ps_dma_man_t* dman, otg_usbtty_t* usbtty);

#endif /* _OTG_USBTTY_H_ */
