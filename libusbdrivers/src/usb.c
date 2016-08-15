/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include <usb/usb.h>
#include <usb/usb_host.h>
#include <usb/drivers/usbhub.h>
#include <usb/usb.h>
#include "services.h"
#include <string.h>
#include <utils/util.h>

#define USB_DEBUG

#ifdef USB_DEBUG
#define dprintf(...) printf(__VA_ARGS__)
#else
#define dprintf(...) do{}while(0)
#endif


#define USB_DBG(d, ...)                                 \
        do {                                            \
            usb_dev_t dev = d;                          \
            if(dev && dev->addr){                       \
                dprintf("USB %2d: ", dev->addr);        \
            }else{                                      \
                dprintf("USB   : ");                    \
            }                                           \
            dprintf(__VA_ARGS__);                       \
        }while(0)


#define NUM_DEVICES 32

#define CLASS_RESERVED_STR "<Reserved>"

const char*
usb_class_get_description(enum usb_class usb_class)
{
    if (usb_class >= 0x00 && usb_class < 0x11) {
        static const char* str[] = {
            "Unspecified - See interface classes",
            "Audio",
            "Communication and CDC Control",
            "Human interface device (HID)",
            CLASS_RESERVED_STR,
            "Physical Interface Device (PID)",
            "Image",
            "Printer",
            "Mass storage",
            "USB hub",
            "CDC-Data",
            "Smart Card"
            CLASS_RESERVED_STR,
            "Content security",
            "Video",
            "Personal Healthcare Pulse monitor",
            "Audio/Video (AV)"
        };
        return str[usb_class];
    }
    /* Some other sparse classes */
    switch (usb_class) {
    case 0xDC:
        return "Diagnostic Device";
    case 0xE0:
        return "Wireless Controller";
    case 0xEF:
        return "Miscellaneous";
    case 0xFE:
        return "Application specific";
    case 0xFF:
        return "ventor specific";
    default:
        return CLASS_RESERVED_STR;
    }
}

/*****************
 **** Helpers ****
 *****************/

static inline struct usbreq
__new_desc_req(enum DescriptorType t, int size) {
    struct usbreq r = {
        .bmRequestType = 0b10000000,
        .bRequest      = GET_DESCRIPTOR,
        .wValue        = t << 8,
        .wIndex        = 0,
        .wLength       = size
    };
    return r;
}

static inline struct usbreq
__new_address_req(int addr) {
    struct usbreq r = {
        .bmRequestType = 0b00000000,
        .bRequest      = SET_ADDRESS,
        .wValue        = addr,
        .wIndex        = 0,
        .wLength       = 0
    };
    return r;
}


/**** Device list ****/

static usb_dev_t inactive_devlist = NULL;

/* Initialise the device list */
static void
devlist_init(usb_t* host)
{
    host->devlist = NULL;
    inactive_devlist = NULL;
    host->addrbm = 1;
    host->next_addr = 1;
}

/* Insert a device into the list, return the index at which it was inserted */
static int
devlist_insert(usb_dev_t d)
{
    usb_t* host = d->host;
    int i = host->next_addr;
    /* Early exit */
    if (host->addrbm == (1ULL << NUM_DEVICES) - 1) {
        return -1;
    }
    /* cycle the list searching for a free address */
    while (host->addrbm & (1 << i)) {
        if (i == NUM_DEVICES) {
            /* Remember address 0 is a special case */
            i = 1;
        } else {
            i++;
        }
    }
    /* Add the device to the list */
    d->next = host->devlist;
    host->devlist = d;
    host->addrbm |= (1 << i);

    /* Update the next address for next insertion */
    if ((i + 1) == NUM_DEVICES) {
        host->next_addr = 1;
    } else {
        host->next_addr = i + 1;
    }

    /* return address */
    return i;
}

/* Remove the device from the list */
static void
devlist_remove(usb_dev_t d)
{
    usb_t* host = d->host;
    usb_dev_t* dptr = &host->devlist;
    while (*dptr != d) {
        assert(*dptr != NULL || !"Device not in list");
        dptr = &(*dptr)->next;
    }
    *dptr = d->next;
    d->next = NULL;
    host->addrbm &= ~(1 << d->addr);
    d->addr = -1;
}

static void
devlist_activate(usb_dev_t d)
{
    usb_dev_t* dptr = &inactive_devlist;
    usb_t* host = d->host;
    while (*dptr != d) {
        assert(*dptr != NULL || !"Device not in list");
        dptr = &(*dptr)->next;
    }
    *dptr = d->next;
    d->next = host->devlist;
    host->devlist = d;
    d->connect(d);
}

static usb_dev_t
devlist_find_inactive(usb_dev_t df)
{
    usb_dev_t d = inactive_devlist;
    while (d != NULL) {
        if (df->prod_id == d->prod_id && df->vend_id == d->vend_id) {
            return d;
        }
    }
    return NULL;
}


/* Retrieve a device from the list */
static usb_dev_t
devlist_at(usb_t* host, int addr)
{
    usb_dev_t d;
    assert(addr >= 0 && addr < NUM_DEVICES);
    for (d = host->devlist; d != NULL; d = d->next) {
        if (d->addr == addr) {
            return d;
        }
    }
    return 0;
}

/************************
 **** Debug printing ****
 ************************/

#define PFIELD(d, x) printf("\t0x%-4x : %s\n", (d)->x, #x)
#define PFIELD2(d, x) printf("\t0x%-4x : %-20s | ", (d)->x, #x)
static void
usb_print_descriptor(struct anon_desc * desc, int index)
{
    int type = desc->bDescriptorType;
    switch (desc->bDescriptorType) {
    case DEVICE: {
        struct device_desc d;
        memcpy(&d, desc, desc->bLength);
        printf("Device descriptor:\n");
        PFIELD(&d, bLength);
        PFIELD(&d, bDescriptorType);
        PFIELD(&d, bcdUSB);
        PFIELD2(&d, bDeviceClass);
        printf("%s\n", usb_class_get_description(d.bDeviceClass));
        PFIELD2(&d, bDeviceSubClass);
        printf("%s\n", usb_class_get_description(d.bDeviceSubClass));
        PFIELD(&d, bDeviceProtocol);
        PFIELD(&d, bMaxPacketSize0);
        PFIELD(&d, idVendor);
        PFIELD(&d, idProduct);
        PFIELD(&d, bcdDevice);
        PFIELD(&d, iManufacturer);
        PFIELD(&d, iProduct);
        PFIELD(&d, iSerialNumber);
        PFIELD(&d, bNumConfigurations);
        break;
    }
    case CONFIGURATION: {
        struct config_desc d;
        memcpy(&d, desc, desc->bLength);
        if (index >= 0) {
            printf("Config descriptor %d\n", index);
        } else {
            printf("Config descriptor\n");
        }
        PFIELD(&d, bLength);
        PFIELD(&d, bDescriptorType);
        PFIELD(&d, wTotalLength);
        PFIELD(&d, bNumInterfaces);
        PFIELD(&d, bConfigurationValue);
        PFIELD(&d, iConfigurationIndex);
        PFIELD2(&d, bmAttributes);
        printf("%s ", (d.bmAttributes & (1 << 6)) ?
               "Self powered" : "");
        printf("%s ", (d.bmAttributes & (1 << 5)) ?
               "Remote wakeup" : "");
        printf("%s ", (d.bmAttributes & (1 << 7)) ?
               "" : "Warning: bit 7 should be set");
        printf("%s ", (d.bmAttributes & (1 << 4)) ?
               "Warning: bit 5 should not be set" : "");
        printf("\n");
        PFIELD2(&d, bMaxPower);
        printf("%dmA\n", d.bMaxPower * 2);
        break;
    }
    case INTERFACE: {
        struct iface_desc d;
        memcpy(&d, desc, desc->bLength);
        if (index >= 0) {
            printf("Interface descriptor %d\n", index);
        } else {
            printf("Interface descriptor\n");
        }
        PFIELD(&d, bLength);
        PFIELD(&d, bDescriptorType);
        PFIELD(&d, bInterfaceNumber);
        PFIELD(&d, bAlternateSetting);
        PFIELD(&d, bNumEndpoints);
        PFIELD2(&d, bInterfaceClass);
        printf("%s\n", usb_class_get_description(d.bInterfaceClass));
        PFIELD2(&d, bInterfaceSubClass);
        printf("%s\n", usb_class_get_description(d.bInterfaceSubClass));
        PFIELD(&d, bInterfaceProtocol);
        PFIELD(&d, iInterface);
        break;
    }
    case ENDPOINT: {
        struct endpoint_desc d;
        memcpy(&d, desc, desc->bLength);
        if (index >= 0) {
            printf("Endpoint descriptor %d\n", index);
        } else {
            printf("Endpoint descriptor\n");
        }
        PFIELD(&d, bLength);
        PFIELD(&d, bDescriptorType);
        PFIELD2(&d, bEndpointAddress);
        printf("%d-", d.bEndpointAddress & 0xf);
        if (d.bmAttributes & 0x3) {
            if (d.bEndpointAddress & (1 << 7)) {
                printf("IN");
            } else {
                printf("OUT");
            }
        }
        printf("\n");
        PFIELD2(&d, bmAttributes);
        switch (d.bmAttributes & 0x3) {
        case 0:
            printf("CONTROL");
            break;
        case 1:
            printf("ISOCH");
            break;
        case 2:
            printf("BULK");
            break;
        case 3:
            printf("INT");
            break;
        }
        printf(",");
        switch ((d.bmAttributes >> 2) & 0x3) {
        case 0:
            printf("No synch");
            break;
        case 1:
            printf("Asynchronous");
            break;
        case 2:
            printf("Adaptive");
            break;
        case 3:
            printf("Synchronous");
            break;
        }
        printf(",");
        switch ((d.bmAttributes >> 4) & 0x3) {
        case 0:
            printf("DATA");
            break;
        case 1:
            printf("Feedback");
            break;
        case 2:
            printf("Implicit feedback data");
            break;
        case 3:
            printf("<Reserved>");
            break;
        }
        printf("\n");
        PFIELD2(&d, wMaxPacketSize);
        printf("%d bytes, %d xacts per uFrame\n",
               d.wMaxPacketSize & 0x7ff,
               ((d.wMaxPacketSize >> 11) & 0x3) + 1);
        PFIELD(&d, bInterval);
        break;
    }
    case DEVICE_QUALIFIER:
    case OTHER_SPEED_CONFIGURATION:
    case INTERFACE_POWER:
    case STRING:
        printf("Descriptor type %d not implemented\n", type);
        break;
        /* Class specific types */
    case HID: {
        struct hid_desc d;
        memcpy(&d, desc, desc->bLength);
        printf("HID descriptor\n");
        PFIELD(&d, bLength);
        PFIELD(&d, bDescriptorType);
        PFIELD(&d, bcdHID);
        PFIELD(&d, bCountryCode);
        PFIELD(&d, bNumDescriptors);
        PFIELD(&d, bReportDescriptorType);
        PFIELD(&d, wReportDescriptorLength);
        break;
    }
    default:
        printf("Unknown descriptor type %d\n", type);
    }
}

static void
print_string_desc(struct string_desc *desc)
{
	assert(desc);

	for (int i = 0; i < desc->bLength - 2; i++) {
		printf("%c", desc->bString[i]);
	}
	printf("\n");
}

static void
usb_get_string_desc(usb_dev_t udev, int index, struct string_desc *desc)
{
    int err;
    int lang;
    struct xact xact[2];
    struct usbreq *req;
    struct string_desc *sdesc;

    if (index == 0) {
	    return;
    }

    /* XXX: xact allocation relies on the xact.len */
    xact[0].len = sizeof(struct usbreq);
    xact[1].len = sizeof(struct string_desc);

    /* Get memory for the request */
    err = usb_alloc_xact(udev->dman, xact, sizeof(xact) / sizeof(struct xact));
    if (err) {
        USB_DBG(udev, "Not enough DMA memory!\n");
        return;
    }

    /* Fill in the SETUP packet */
    xact[0].type = PID_SETUP;
    xact[1].type = PID_IN;
    req = xact_get_vaddr(&xact[0]);
    sdesc = (struct string_desc*)xact[1].vaddr;

    /* Get language ID */
    *req = __get_descriptor_req(STRING, 0, 0, sizeof(struct string_desc));
    err = usbdev_schedule_xact(udev, udev->ep_ctrl, xact, 2, NULL, NULL);

    if (err < 0) {
       USB_DBG(udev, "USB request failed.\n");
       usb_destroy_xact(udev->dman, xact, 2);
       return;
    }

    lang = sdesc->bString[1] << 8 | sdesc->bString[0];

    /* Get the string descriptor */
    *req = __get_descriptor_req(STRING, index, lang, sizeof(struct string_desc));
    err = usbdev_schedule_xact(udev, udev->ep_ctrl, xact, 2, NULL, NULL);

    if (err < 0) {
       USB_DBG(udev, "USB request failed.\n");
       usb_destroy_xact(udev->dman, xact, 2);
       return;
    }

    if (desc) {
        memcpy(desc, sdesc, sdesc->bLength);
    }
    usb_destroy_xact(udev->dman, xact, 2);
}

static void
print_dev(usb_dev_t d)
{
    if (d) {
        printf("USB@%02d: 0x%04x:0x%04x | %-40s\n",
               d->addr, d->vend_id, d->prod_id,
               usb_class_get_description(d->class));
    }
}

static int
usb_config_print_cb(void* token, int cfg, int iface,
                    struct anon_desc* d)
{
    uint32_t *cnt = (uint32_t*)token;
    int v;
    if (d == NULL) {
        printf("\n");
        return 0;
    } else {
        switch (d->bDescriptorType) {
        case CONFIGURATION:
            cnt[0]++;
            cnt[1] = 0;
            cnt[2] = 0;
            v = cnt[0];
            break;
        case INTERFACE:
            cnt[1]++;
            cnt[2] = 0;
            v = cnt[1];
            break;
        case ENDPOINT:
            cnt[2]++;
            v = cnt[2];
            break;
        default:
            v = -1;
            break;
        }
        usb_print_descriptor(d, v);
        return 0;
    }
}

static void
usbdev_config_print(usb_dev_t udev)
{
    struct anon_desc* desc;
    struct usbreq *req;
    struct xact xact[2];
    int ret;
    uint32_t cnt[3] = {0, 0, 0};

    /* Device descriptor */
    xact[0].type = PID_SETUP;
    xact[0].len = sizeof(*req);
    xact[1].type = PID_IN;
    xact[1].len = sizeof(struct device_desc);
    ret = usb_alloc_xact(udev->dman, xact, 2);
    if (ret) {
        assert(0);
    }
    req = xact_get_vaddr(&xact[0]);
    *req = __get_descriptor_req(DEVICE, 0, 0, xact[1].len);
    ret = usbdev_schedule_xact(udev, udev->ep_ctrl, xact, 2, NULL, NULL);
    if (ret < 0) {
        assert(ret >= 0);
        return;
    }
    desc = (struct anon_desc*)xact_get_vaddr(&xact[1]);
    usb_print_descriptor(desc, -1);
    usb_destroy_xact(udev->dman, xact, sizeof(xact) / sizeof(*xact));
    /* Print config descriptors */
    usbdev_parse_config(udev, usb_config_print_cb, cnt);
}

static void
print_dev_graph(usb_t* host, usb_dev_t d, int depth)
{
    int i;
    if (d != NULL) {
        for (i = 0; i < depth; i++) {
            printf("    ");
        }
        print_dev(d);
    }
    /* Search for connected devices */
    for (i = 1; i < NUM_DEVICES; i++) {
        usb_dev_t d2;
        d2 = devlist_at(host, i);
        if (d2 && d2->hub == d) {
            print_dev_graph(host, d2, depth + 1);
        }
    }
}

static int
parse_config(usb_dev_t udev, struct anon_desc *d, int tot_len,
             usb_config_cb cb, void* t)
{
    int cfg = -1;
    int iface = -1;
    struct anon_desc *usrd = NULL;
    struct endpoint_desc *edsc;
    struct endpoint *ep;
    int buf_len = 0;
    int cur_len = 0;
    int err = 0;
    int cnt = 0;

    /*
     * FIXME: Not all devices report the total length of its descriptors
     * correctly. We should always check the details of each descriptor.
     */
    while (cur_len < tot_len) {
        if (!d->bLength) {
            break;
        }
        /* Copy in for the sake of alignment */
        if (buf_len < d->bLength) {
            if (usrd) {
                usb_free(usrd);
            }
            usrd = usb_malloc(d->bLength);
            if (usrd == NULL) {
                assert(usrd);
                err = 1;
                break;
            }
            buf_len = d->bLength;
        }
        memcpy(usrd, d, d->bLength);
        /* Update our config/iface */
        switch (d->bDescriptorType) {
        case CONFIGURATION:
            cfg = ((struct config_desc*)usrd)->bConfigurationValue;
            iface = -1;
            break;
        case INTERFACE:
            iface = ((struct iface_desc*)usrd)->bInterfaceNumber;
            break;
	case ENDPOINT:
	    edsc = (struct endpoint_desc*)usrd;
	    ep = usb_malloc(sizeof(struct endpoint));

	    /* Fill in the endpoint structure, USB standard(9.6.6) */
	    ep->type = edsc->bmAttributes & 0x3;
	    ep->dir = edsc->bEndpointAddress >> 0x7;
	    ep->num = edsc->bEndpointAddress & 0xF;
	    ep->max_pkt = edsc->wMaxPacketSize;
	    ep->interval = edsc->bInterval;

	    udev->ep[cnt++] = ep;
	    break;
        default:
            break;
        }
        /* Send to caller */
        if (cb(t, cfg, iface, usrd)) {
            break;
        }
        /* Next */
        cur_len += d->bLength;
        d = (struct anon_desc*)((uintptr_t)d + d->bLength);
    }

    /* Report end of list */
    if (cur_len == tot_len) {
        cb(t, cfg, iface, NULL);
    }
    /* Clean up */
    if (usrd == NULL) {
        usb_free(usrd);
    }
    return err;
}

static int
usb_new_device_with_host(usb_dev_t hub, usb_t* host, int port, enum usb_speed speed, usb_dev_t* d)
{
    usb_dev_t udev = NULL;
    struct usbreq *req;
    struct device_desc* d_desc;
    struct string_desc s_desc;
    struct xact xact[2];
    int addr = 0;
    int err;

    USB_DBG(udev, "New USB device!\n");
    udev = (usb_dev_t)usb_malloc(sizeof(*udev));
    if (!udev) {
        USB_DBG(udev, "No heap memory for new USB device\n");
        assert(0);
        return -1;
    }
    udev->addr = 0;
    udev->connect = NULL;
    udev->disconnect = NULL;
    udev->dev_data = NULL;
    udev->hub = hub;
    udev->port = port;
    udev->speed = speed;
    udev->host = host;
    udev->dman = host->hdev.dman;

    /*
     * Allocate control endpoint
     * Every device should have one control endpoint, the endpoint number is
     * always zero and we initialize the maximum packet size to 8 for sending
     * the very first request.
     */
    udev->ep_ctrl = (struct endpoint*)usb_malloc(sizeof(struct endpoint));
    if (!udev->ep_ctrl) {
        USB_DBG(udev, "No heap memory for control endpoint\n");
	usb_free(udev);
        assert(0);
        return -1;
    }
    udev->ep_ctrl->type = EP_CONTROL;
    udev->ep_ctrl->num = 0;
    udev->ep_ctrl->max_pkt = 8;

    xact[0].type = PID_SETUP;
    xact[0].len = sizeof(*req);
    xact[1].type = PID_IN;
    xact[1].len = sizeof(*d_desc);
    err = usb_alloc_xact(udev->dman, xact, 2);
    if (err) {
        USB_DBG(udev, "No DMA memory for new USB device\n");
        assert(0);
        free(udev);
        return -1;
    }
    req = xact_get_vaddr(&xact[0]);
    d_desc = xact_get_vaddr(&xact[1]);


    /* USB transactions are O(n) when trying to bind a driver.
     * This is a good time to at least cache
     * a) Max packet size for EP 0
     * b) product and vendor ID
     * c) device class
     */
    USB_DBG(udev, "Determining maximum packet size on the control endpoint\n");
    /*
     * We need the value of bMaxPacketSize in order to request
     * the bMaxPacketSize. A work around to this circular
     * dependency is to set the maximum packet size to 8 and
     * limit the size of our packets to prevent splitting until
     * we know what the correct value is NOTE: High speed
     * devices must always have a MaxPacketSize of 64 on the
     * control endpoint (see USB spec) but we do not consider
     * special cases.
     */
    xact[1].len = 8;
    *req = __new_desc_req(DEVICE, 8);
    err = usbdev_schedule_xact(udev, udev->ep_ctrl, xact, 2, NULL, NULL);
    if (err < 0) {
        usb_destroy_xact(udev->dman, xact, 2);
        free(udev);
        return -1;
    }

    assert(err >= 0);
    udev->ep_ctrl->max_pkt = d_desc->bMaxPacketSize0;
    
    /* Find the next available address */
    addr = devlist_insert(udev);
    if (addr < 0) {
        USB_DBG(dev, "Too many devices\n");
        assert(0);
        usb_destroy_xact(udev->dman, xact, sizeof(xact) / sizeof(*xact));
        usb_free(udev);
        return -1;
    }

    /* Set the address */
    *req = __new_address_req(addr);
    USB_DBG(udev, "Setting address to %d\n", addr);
    err = usbdev_schedule_xact(udev, udev->ep_ctrl, xact, 1, NULL, NULL);
    assert(err >= 0);
    /* Device has 2ms to start responding to new address */
    msdelay(2);
    udev->addr = addr;

    /* All settled, start processing standard USB descriptors */
    USB_DBG(udev, "Retrieving device descriptor\n");
    xact[1].len = sizeof(*d_desc);
    *req = __new_desc_req(DEVICE, sizeof(*d_desc));
    err = usbdev_schedule_xact(udev, udev->ep_ctrl, xact, 2, NULL, NULL);
    assert(err >= 0);
    udev->prod_id = d_desc->idProduct;
    udev->vend_id = d_desc->idVendor;
    udev->class   = d_desc->bDeviceClass;
    USB_DBG(udev, "idVendor  0x%04x | ", udev->vend_id);
    if (d_desc->iManufacturer) {
        usb_get_string_desc(udev, d_desc->iManufacturer, &s_desc);
        print_string_desc(&s_desc);
    } else {
        printf("\n");
    }
    USB_DBG(udev, "idProduct 0x%04x | ", udev->prod_id);
    if (d_desc->iProduct) {
        usb_get_string_desc(udev, d_desc->iProduct, &s_desc);
        print_string_desc(&s_desc);
    } else {
        printf("\n");
    }

    *d = udev;
    usb_destroy_xact(udev->dman, xact, sizeof(xact) / sizeof(*xact));

    /* Search through disconnected drivers for reconnection */
    udev = devlist_find_inactive(*d);
    if (udev) {
        USB_DBG(*d, "Reconnecting existing driver\n");
        udev->addr = (*d)->addr;
        devlist_remove(*d);
        devlist_activate(udev);
        usb_free(*d);
        *d = udev;
    }

    return 0;
}

/****************************
 **** Exported functions ****
 ****************************/

int
usb_init(enum usb_host_id id, ps_io_ops_t* ioops, usb_t* host)
{
    usb_hub_t  hub;
    usb_dev_t  udev;
    int err;

    /* Prefill the host structure */
    devlist_init(host);

    err = usb_host_init(id, ioops, &host->hdev);
    if (err) {
        assert(!err);
        return -1;
    }
    err = usb_new_device_with_host(NULL, host, 1, 0, &udev);

    assert(!err);

    err = usb_hub_driver_bind(udev, &hub);
    assert(!err);
    return 0;
}

int
usb_new_device(usb_dev_t hub, int port, enum usb_speed speed,
               usb_dev_t* d)
{
    return usb_new_device_with_host(hub, hub->host, port, speed, d);
}


usb_dev_t
usb_get_device(usb_t* host, int addr)
{
    if (addr <= 0 || addr >= NUM_DEVICES) {
        return NULL;
    } else {
        return devlist_at(host, addr);
    }
}

/*
 * Parsing standard USB descriptors.
 * We don't support multiple configurations, if the device has more than one
 * configurations, the first one will be activated.
 */
int
usbdev_parse_config(usb_dev_t udev, usb_config_cb cb, void* t)
{
    struct xact xact[2];
    struct usbreq* req;
    struct config_desc *cd;
    struct anon_desc *d;
    int tot_len;
    int err;

    /* First read to find the size of the descriptor table */
    xact[0].len = sizeof(*req);
    xact[0].type = PID_SETUP;
    xact[1].len = sizeof(*cd);
    xact[1].type = PID_IN;
    err = usb_alloc_xact(udev->dman, xact, 2);
    if (err) {
        assert(0);
        return -1;
    }
    req = xact_get_vaddr(&xact[0]);
    cd = xact_get_vaddr(&xact[1]);
    *req = __get_descriptor_req(CONFIGURATION, 0, 0, xact[1].len);
    err = usbdev_schedule_xact(udev, udev->ep_ctrl, xact, 2, NULL, NULL);
    if (err < 0) {
        usb_destroy_xact(udev->dman, xact, 2);
        assert(0);
        return -1;
    }
    tot_len = cd->wTotalLength;
    /* Next read for the entire descriptor table */
    xact[0].len = sizeof(*req);
    xact[0].type = PID_SETUP;
    xact[1].len = tot_len;
    xact[1].type = PID_IN;
    err = usb_alloc_xact(udev->dman, xact, 2);
    if (err) {
        assert(0);
        return -1;
    }
    req = xact_get_vaddr(&xact[0]);
    d = xact_get_vaddr(&xact[1]);
    *req = __get_descriptor_req(CONFIGURATION, 0, 0, tot_len);
    err = usbdev_schedule_xact(udev, udev->ep_ctrl, xact, 2, NULL, NULL);
    if (err < 0) {
        usb_destroy_xact(udev->dman, xact, sizeof(xact) / sizeof(*xact));
        return -1;
    }
    /* Now loop through descriptors */
    err = parse_config(udev, d, tot_len, cb, t);
    usb_destroy_xact(udev->dman, xact, sizeof(xact) / sizeof(*xact));
    return err;
}

void
usbdev_disconnect(usb_dev_t udev)
{
    UNUSED int err;
    usb_host_t* hdev;
    USB_DBG(udev, "Disconnecting\n");
    assert(udev);
    assert(udev->host);
    hdev = &udev->host->hdev;
    if (udev->disconnect) {
        printf("calling device disconnect 0x%x\n", (uint32_t)udev->disconnect);
        udev->disconnect(udev);
    }
//    err = hdev->cancel_xact(hdev, udev->addr);
//    assert(!err);
    (void)hdev;
    devlist_remove(udev);
    if (udev->dev_data) {
        /* Stow this device if a driver is attached */
        udev->next = inactive_devlist;
        inactive_devlist = udev;
    } else {
        /* destroy it */
        usb_free(udev->ep_ctrl);
	for (int i = 0; i < USB_MAX_EPS; i++) {
		if (udev->ep[i]) {
			usb_free(udev->ep[i]);
		}
	}
        usb_free(udev);
    }
}

void
usb_handle_irq(usb_t* host)
{
    usb_host_t* hdev;
    assert(host);
    hdev = &host->hdev;
    hdev->handle_irq(hdev);
}

int
usbdev_schedule_xact(usb_dev_t udev, struct endpoint *ep, struct xact* xact,
                     int nxact, usb_cb_t cb, void* token)
{
    int err;
    usb_host_t* hdev;
    uint8_t hub_addr;
    assert(udev);
    assert(udev->host);
    assert(udev->host->hdev.schedule_xact);
    hdev = &udev->host->hdev;
    if (udev->hub) {
        hub_addr = udev->hub->addr;
    } else {
        hub_addr = -1;
    }
    err = usb_hcd_schedule(hdev, udev->addr, hub_addr, udev->port, udev->speed,
                           ep, xact, nxact, cb, token);
    return err;
}

void
usb_lsusb(usb_t* host, int v)
{
    int i;
    printf("\n");
    if (v == 0) {
        /* Print a simple list */
        for (i = 1; i < NUM_DEVICES; i++) {
            usb_dev_t d = devlist_at(host, i);
            print_dev(d);
        }
    } else {
        /* Print by connection */
        print_dev_graph(host, NULL, -1);
    }
    /* Print out all the configs */
    if (v > 1) {
        printf("\n");
        for (i = 1; i < NUM_DEVICES; i++) {
            usb_dev_t d = devlist_at(host, i);
            if (d) {
                print_dev(d);
                usbdev_config_print(d);
            }
        }
    }
    printf("\n");
}

void
usb_probe_device(usb_dev_t dev)
{
    usbdev_config_print(dev);
}

int
usb_alloc_xact(ps_dma_man_t* dman, struct xact* xact, int nxact)
{
    int i;
    for (i = 0; i < nxact; i++) {
        if (xact[i].len) {
            xact[i].vaddr = ps_dma_alloc_pinned(dman, xact[i].len, 32, 0, PS_MEM_NORMAL, &xact[i].paddr);
            if (xact[i].vaddr == NULL) {
                usb_destroy_xact(dman, xact, i);
                return -1;
            }
        } else {
            xact[i].vaddr = NULL;
        }
    }
    return 0;
}

void
usb_destroy_xact(ps_dma_man_t* dman, struct xact* xact, int nxact)
{
    while (nxact-- > 0) {
        if (xact[nxact].vaddr) {
            ps_dma_free_pinned(dman, xact[nxact].vaddr, xact[nxact].len);
        }
    }
}


enum usb_class
usbdev_get_class(usb_dev_t dev)
{
    return dev->class;
}
