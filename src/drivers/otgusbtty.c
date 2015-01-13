/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "otgusbtty.h"
#include <usb/usb.h>
#include "../services.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

struct otg_usbtty {
    usb_otg_t otg;
    struct dma_allocator* dalloc;
    dma_mem_t dma_desc;
};

static struct device_desc otg_usbtty_device_desc = {
    .bLength                = 0x12,
    .bDescriptorType        = 0x1,
    .bcdUSB                 = 0x110,
    .bDeviceClass           = 0x0,
    .bDeviceSubClass        = 0x0,
    .bDeviceProtocol        = 0x0,
    .bMaxPacketSize0        = 0x40,
    .idVendor               = 0x067b,
    .idProduct              = 0x2303,
    .bcdDevice              = 0x300,
    .iManufacturer          = 0x0 /* 0x1 */,
    .iProduct               = 0x0 /* 0x2 */,
    .iSerialNumber          = 0x0,
    .bNumConfigurations     = 0x1
};

static struct config_desc otg_usbtty_config_desc = {
    .bLength                = 0x9,
    .bDescriptorType        = 0x2,
    .wTotalLength           = 0x27,
    .bNumInterfaces         = 0x1,
    .bConfigurationValue    = 0x1,
    .iConfigurationIndex    = 0x0,
    .bmAttributes           = 0x80,
    .bMaxPower              = 0x32
};

static struct iface_desc otg_usbtty_iface_desc = {
    .bLength                = 0x9,
    .bDescriptorType        = 0x4,
    .bInterfaceNumber       = 0x0,
    .bAlternateSetting      = 0x0,
    .bNumEndpoints          = 0x3,
    .bInterfaceClass        = 0xff,
    .bInterfaceSubClass     = 0x0,
    .bInterfaceProtocol     = 0x0,
    .iInterface             = 0x0
};

static struct endpoint_desc otg_usbtty_ep1_desc = {
    .bLength                = 0x7,
    .bDescriptorType        = 0x5,
    .bEndpointAddress       = 0x81,
    .bmAttributes           = 0x3,
    .wMaxPacketSize         = 0xa,
    .bInterval              = 0x1
};

static struct endpoint_desc otg_usbtty_ep2_desc = {
    .bLength                = 0x7,
    .bDescriptorType        = 0x5,
    .bEndpointAddress       = 0x2,
    .bmAttributes           = 0x2,
    .wMaxPacketSize         = 0x40,
    .bInterval              = 0x0
};

static struct endpoint_desc otg_usbtty_ep3_desc = {
    .bLength                = 0x7,
    .bDescriptorType        = 0x5,
    .bEndpointAddress       = 0x83,
    .bmAttributes           = 0x2,
    .wMaxPacketSize         = 0x40,
    .bInterval              = 0x0
};


static void
freebuf_cb(usb_otg_t otg, void* token,
           enum usb_xact_status stat)
{
    (void)otg;
    assert(stat == XACTSTAT_SUCCESS);
    dma_free((dma_mem_t)token);
}

static void
send_desc(otg_usbtty_t tty, enum DescriptorType type, int index,
          int maxlen)
{
    struct anon_desc* d = NULL;
    /* Not handling index yet... */
    assert(index == 0);
    /* Find the descriptor */
    switch (type) {
    case DEVICE:
        d = (struct anon_desc*)&otg_usbtty_device_desc;
        printf("device descriptor read/");
        if (maxlen >= d->bLength) {
            printf("all\n");
        } else {
            printf("%d\n", maxlen);
        }
        break;
    case CONFIGURATION:
        printf("config\n");
        break;
    case STRING:
        printf("string\n");
        break;
    case INTERFACE:
        printf("interface\n");
        break;
    case ENDPOINT:
        printf("endpoint\n");
        break;
    case DEVICE_QUALIFIER:
        printf("device qualifier\n");
        break;
    case OTHER_SPEED_CONFIGURATION:
        printf("other speed\n");
        break;
    case INTERFACE_POWER:
        printf("interface power\n");
        break;
    case HID:
        printf("hid\n");
        break;
    case HUB:
        printf("Hub\n");
        break;
    default:
        printf("Unhandled descriptor request\n");
    }
    /* Send the descriptor */
    if (d != NULL) {
        dma_mem_t dma_buf;
        void* buf;
        int err;
        int size = d->bLength;
        /* limit size to prevent babble */
        if (maxlen < size) {
            size = maxlen;
        }
        /* Copy in */
        buf = dma_alloc(tty->dalloc, size, 32, DMAF_HRW,
                        &dma_buf);
        if (buf == NULL) {
            assert(0);
            return;
        }
        memcpy(buf, d, size);
        /* Send the packet */
        err = otg_prime(tty->otg, 0, PID_IN, dma_buf, size,
                        freebuf_cb, dma_buf);
        if (err) {
            assert(0);
            dma_free(dma_buf);
            return;
        }
        /* Status phase */
        err = otg_prime(tty->otg, 0, PID_OUT, NULL, 0, freebuf_cb, dma_buf);
        if (err) {
            assert(0);
            dma_free(dma_buf);
        }
    }
}

static void
usbtty_setup_cb(usb_otg_t otg, void* token, struct usbreq* req)
{
    otg_usbtty_t tty = (otg_usbtty_t)token;
    (void)otg_usbtty_config_desc;
    (void)otg_usbtty_iface_desc;
    (void)otg_usbtty_ep1_desc;
    (void)otg_usbtty_ep2_desc;
    (void)otg_usbtty_ep3_desc;
    switch (req->bRequest) {
    case GET_DESCRIPTOR:
        send_desc(tty, req->wValue >> 8, req->wValue & 0xff,
                  req->wLength);
        break;
    case GET_CONFIGURATION:
        printf("get conf\n");
        break;
    case GET_STATUS:
        printf("get stat\n");
        break;
    case CLR_FEATURE:
        printf("Clear feat\n");
        break;
    case SET_FEATURE:
        printf("Set feature\n");
        break;
    case SET_ADDRESS:
        printf("Set address\n");
        break;
    case SET_DESCRIPTOR:
        printf("Set descriptor\n");
        break;
    case SET_CONFIGURATION:
        printf("Set config\n");
        break;
    case GET_INTERFACE:
        printf("Get interface\n");
        break;
    case SET_INTERFACE:
        printf("Set interface\n");
        break;
    default:
        printf("Unhandles request %d\n", req->bRequest);
    }
}

int
otg_usbtty_init(usb_otg_t otg, struct dma_allocator* dalloc,
                otg_usbtty_t* usbtty)
{
    otg_usbtty_t tty;
    int err;

    assert(dalloc);
    assert(usbtty);
    assert(otg);

    /* Allocate memory */
    tty = usb_malloc(sizeof(*tty));
    if (tty == NULL) {
        assert(0);
        return -1;
    }
    tty->dalloc = dalloc;
    tty->otg = otg;
    /* Initialise the control endpoint */
    err = otg_ep0_setup(otg, usbtty_setup_cb, tty);
    if (err) {
        assert(0);
        usb_free(tty);
        return -1;
    }
    *usbtty = tty;
    return 0;
}

