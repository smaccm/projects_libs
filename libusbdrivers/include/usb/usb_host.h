/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/*
 * These functions need to be implemented by platform
 * specific code.
 */

#ifndef __USB_USB_HOST_H_
#define __USB_USB_HOST_H_

#include <platsupport/io.h>
#include <usb/plat/usb.h>

enum usb_speed {
/// 1.5Mbps connection
    USBSPEED_LOW  = 0,
/// 12Mbps connection
    USBSPEED_FULL = 1,
/// 480Mbps connection
    USBSPEED_HIGH = 2
};

enum usb_xact_type {
/// Input PID
    PID_IN,
/// Output PID
    PID_OUT,
/// Setup PID
    PID_SETUP,
/// Interrupt PID
    PID_INT,
};

enum usb_xact_status {
/// The transaction completed successfully
    XACTSTAT_SUCCESS,
/// The transaction has not been processed
    XACTSTAT_PENDING,
/// The transaction was cancelled due to disconnect, etc
    XACTSTAT_CANCELLED,
/// There was an error in processing the transaction
    XACTSTAT_ERROR,
/// The host exibited a failure during the transaction.
    XACTSTAT_HOSTERROR
};

struct xact {
/// Transfer type
    enum usb_xact_type type;
/// DMA buffer to exchange
    void* vaddr;
    uintptr_t paddr;
/// The length of @ref{buf}
    int len;
};

static inline void* xact_get_vaddr(struct xact* xact)
{
    return xact->vaddr;
}

static inline uintptr_t xact_get_paddr(struct xact* xact)
{
    return xact->paddr;
}

/** Callback type for asynchronous USB transactions
 * @param[in] token  An unmodified opaque token as passed to
 *                   the associated transacton request.
 * @param[in] stat   The status of the transaction.
 * @param[in] rbytes The number of bytes remaining in the transaction.
 *                   This value is generally 0 on successful transmission
 *                   unless a short read or write occurs.
 * @return           1 if the transaction should be rescheduled,
 *                   otherwise, 0.
 */
typedef int (*usb_cb_t)(void* token, enum usb_xact_status stat, int rbytes);


struct usb_host;
typedef struct usb_host usb_host_t;

struct usb_host {
    /// Device ID
    enum usb_host_id id;
    /// Number of ports provided by this host controller
    int nports;

    /// DMA allocator
    ps_dma_man_t* dman;

    /// Submit a transaction for transfer.
    int (*schedule_xact)(usb_host_t* hdev, uint8_t addr, int8_t hub_addr, uint8_t hub_port,
                         enum usb_speed speed, int ep, int max_pkt, int rate_ms, int dt,
                         struct xact* xact, int nxact, usb_cb_t cb, void* t);
    /// Cancel all transactions for a given device address
    int (*cancel_xact)(usb_host_t* hdev, void* token);
    /// Handle an IRQ
    void (*handle_irq)(usb_host_t* hdev);

    /// IRQ numbers tied to this device
    const int* irqs;
    /// Host private data
    struct usb_hc_data* pdata;
};


/**
 * Schedules a USB transaction
 * @param[in] hdev     The host controller that should be used for the transfer
 * @param[in] addr     The destination USB device address
 * @param[in] hub_addr The USB device address of the hub at which the destination
 *                     device is connected. -1 must be used if the device is not
 *                     connected to a hub (i.e. when it is the root hub).
 *                     0 may be used if the device is a SPEED_FULL device.
 * @param[in] hub_port The port at which the destination device is connected to
 *                     its parent hub.
 *                     0 may be used if the device is a SPEED_FULL device.
 * @param[in] speed    The USB speed of the device.
 * @param[in] ep       The destination endpoint of the destination device.
 * @param[in] max_pkt  The maximum packet size supported by the provided endpoint.
 * @param[in] rate_ms  The interval at which the packet should be scheduled.
 *                     (0 if this packet should only be scheduled once.
 * @param[in] dt       Data toggle bit. First packet will be sent to DATA<dt>
 *                     where dt may be either 0 or 1. This field is ignored for
 *                     SETUP transactions.
 * @param[in] xact     An array of packet descriptors.
 * @param[in] nxact    The number of packet descriptors in the array.
 * @param[in] cb       A callback function to call on completion.
 *                     NULL will result in blocking operation.
 * @param[in] t        A token to pass, unmodified, to the provided callback
 *                     function on completion.
 * @return             Negative values represent failure, otherwise, the
 *                     number of bytes remaining to be transfered is returned.
 */
static inline int
usb_hcd_schedule(usb_host_t* hdev, uint8_t addr, uint8_t hub_addr, uint8_t hub_port,
                 enum usb_speed speed, int ep, int max_pkt, int rate_ms, int dt,
                 struct xact* xact, int nxact, usb_cb_t cb, void* t)
{
    return hdev->schedule_xact(hdev, addr, hub_addr, hub_port, speed, ep, max_pkt,
                               rate_ms, dt, xact, nxact, cb, t);
}

/**
 * Cancels a pending USB transaction.
 * It is assumed that a transaction can be uniquely identified by the token
 * that was to be passed to the relevant callback function. For this reason,
 * this function cancels only a single pending transaction where the provided
 * token and the registered token match.
 * @param[in] hdev  A handle to the host controller on which the transaction was
 *                  scheduled.
 * @param[in] token The token which was provided as a callback function argument
 *                  to the transaction (NULL is not supported)
 * @return          0 on success
 */
static inline int
usb_hcd_cancel(usb_host_t* hdev, void* token)
{
    return hdev->cancel_xact(hdev, token);
}

static inline void
usb_hcd_handle_irq(usb_host_t* hdev)
{
    hdev->handle_irq(hdev);
}

static inline int
usb_hcd_count_ports(usb_host_t* hdev)
{
    return hdev->nports;
}

/**
 * Initialise USB host controller.
 * This function should only be called if you wish to use a raw API for the usb host controller, otherwise,
 * this function will be called by usb_init and the appropriate book keeping for device management
 * will be created and maintained.
 * @param[in]  id     The id of the host controller to initialise
 * @param[in]  ioops  a list of io operation functions.
 *                    of the initialised host controller
 * @param[out] hdev   A host structure to populate. This must
 *                    already be filled with a DMA allocator.
 *                    and the device ID.
 * @return            0 on success
 */
int usb_host_init(enum usb_host_id id, ps_io_ops_t* ioops, usb_host_t* hdev);

/** Return a list of IRQ numbers handled by the provided host
 * @param[in]  host   A handle to the USB host device in question
 * @param[out] nirqs  The number of IRQs handled by this host.
 * @return            A NULL terminated list of IRQs
 */
const int* usb_host_irqs(usb_host_t* host, int* nirqs);


#endif /* __USB_USB_HOST_H_ */

