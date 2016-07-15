/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef USB3503_HUB_H
#define USB3503_HUB_H


/**
 * USB3503 IC from Microchip
 */
#ifdef ARCH_ARM
#include <platsupport/gpio.h>
#include <platsupport/i2c.h>

#define USB3503_I2C_ADDR  0x10

struct usb3503 {
    i2c_slave_t i2c_slave;
    gpio_t o_nreset;
    gpio_t o_hubconnect;
    gpio_t i_nint;
};
typedef struct usb3503 usb3503_t;


int usb3503_init(i2c_bus_t* i2c_bus, gpio_sys_t* gpio_sys, gpio_id_t o_nreset,
                 gpio_id_t o_hubconnect, gpio_id_t i_nint, usb3503_t* hub);

void usb3503_reset(usb3503_t* hub);
void usb3503_hard_reset(usb3503_t* hub);

void usb3503_connect(usb3503_t* hub);
void usb3503_disconnect(usb3503_t* hub);

void usb3503_handle_irq(usb3503_t* hub);

#endif
#endif /* USB3503_HUB_H */
