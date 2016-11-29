/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <usb/usb.h>
#include <autoconf.h>

#define LAN9730_PID  0x9730
#define LAN9730_VID  0x0424

static inline int is_eth(usb_dev_t dev)
{
    return dev->prod_id == LAN9730_PID && dev->vend_id == LAN9730_VID;
}


#if 0 //CONFIG_LIB_LWIP
#include <lwip/netif.h>
struct netif* lan9730_driver_bind(usb_dev_t udev);

int lan9730_input(struct netif *netif);

int lan9730_poll_status(struct netif *netif);
#endif /* CONFIG_LIB_LWIP */
