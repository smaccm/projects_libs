/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <platsupport/clock.h>
#include <platsupport/gpio.h>
#include <platsupport/mach/pmic.h>
#include <platsupport/plat/sysreg.h>
#include <usb/usb_host.h>
#include "../../ehci/ehci.h"
#include "../../services.h"
#include "../usb_otg.h"
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <usb/drivers/usb3503_hub.h>


#define USB2_HOST_CTRL_PADDR  0x12130000
#define USB2_HOST_CTRL_SIZE   0x1000
#define USB2_HOST_EHCI_PADDR  0x12110000
#define USB2_HOST_EHCI_SIZE   0x1000
#define USB2_HOST_OHCI_PADDR  0x12120000
#define USB2_HOST_OHCI_SIZE   0x1000
#define USB2_DEV_LINK_PADDR   0x12140000
#define USB2_DEV_LINK_SIZE    0x1000

#define USBPHY_PHYCTRL_OFFSET 0x000

#define HOST_ENABLE        (BIT(29) | BIT(28) | BIT(27) | BIT(26))
#define HOST_FREQ_SEL(x)   ((x) * BIT(16))
#define HOST_UTMI_RESET    BIT(2)
#define HOST_LINK_RESET    BIT(1)
#define HOST_PHY_RESET     BIT(0)
#define HOST_RESET         (HOST_UTMI_RESET | HOST_LINK_RESET | HOST_PHY_RESET)

#define REG32(base, offset) (volatile uint32_t*)((void*)(base) + (offset))

#define NRESET_GPIO     XEINT12
#define HUBCONNECT_GPIO XEINT6
#define NINT_GPIO       XEINT7

static volatile void* _phy_regs = NULL;

static sysreg_t _sysreg;

/* EHCI registers */
static void *_usb_regs = NULL;

/* GPIO subsystem for bit-bangined I2C and HUB control */
static gpio_sys_t gpio_sys;

/* HUB and PMIC on I2C4 */
static struct i2c_bb i2c_bb;
static i2c_bus_t i2c_bus;

/* Eth power control */
static pmic_t pmic;
/* Hub control */
static usb3503_t usb3503_hub;

static int
usb_init_phy(ps_io_ops_t* io_ops)
{
    /* Map phy regs */
    if (_phy_regs == NULL) {
        _phy_regs = GET_RESOURCE(io_ops, USB2_HOST_CTRL);
    }
    exynos5_sysreg_init(io_ops, &_sysreg);
    exynos5_sysreg_usbphy_enable(USBPHY_USB2, &_sysreg);

    /* Reset */
    *REG32(_phy_regs, USBPHY_PHYCTRL_OFFSET) = HOST_FREQ_SEL(5) | HOST_RESET | BIT(20) | BIT(10);
    udelay(10);
    *REG32(_phy_regs, USBPHY_PHYCTRL_OFFSET) &= ~HOST_RESET;
    udelay(20);

    /* Enable */
    *REG32(_phy_regs, USBPHY_PHYCTRL_OFFSET) |= HOST_ENABLE;
    udelay(10);

    return 0;
}



/*******************************************/

static void
hub_pwren(int state)
{
    if (state) {
        usb3503_connect(&usb3503_hub);
    } else {
        usb3503_disconnect(&usb3503_hub);
    }
}

static void
eth_pwren(int state)
{
    if (state) {
        pmic_ldo_cfg(&pmic, LDO_ETH, LDO_ON, 3300);
    } else {
        pmic_ldo_cfg(&pmic, LDO_ETH, LDO_OFF, 3300);
    }
}

static void
board_pwren(int port, int state)
{
    switch (port) {
    case 1:
        /* USB2 */
        break;
    case 2:
        /* HSIC Ethernet */
        eth_pwren(state);
        break;
    case 3:
        /* USB hub */
        hub_pwren(state);
        break;
    default:
        assert(!"Invalid port for power change");
    }
}

static int
usb_plat_gpio_init(ps_io_ops_t* io_ops)
{
    int err;
    err = gpio_sys_init(io_ops, &gpio_sys);
    assert(!err);
    err = i2c_bb_init(&gpio_sys, GPIOID(GPA2, 1), GPIOID(GPA2, 0), &i2c_bb, &i2c_bus);
    assert(!err);

    /* USB hub */
    err = usb3503_init(&i2c_bus, &gpio_sys, NRESET_GPIO, HUBCONNECT_GPIO,
                       NINT_GPIO, &usb3503_hub);
    assert(!err);

    /* PMIC for ethernet power change */
    err = pmic_init(&i2c_bus, PMIC_BUSADDR, &pmic);
    assert(!err);

    /* Turn off the eth chip */
    eth_pwren(0);
    return err;
}

int
usb_host_init(enum usb_host_id id, ps_io_ops_t* io_ops, usb_host_t* hdev)
{
    int err;
    if (id < 0 || id > USB_NHOSTS) {
        return -1;
    }
    assert(io_ops);
    assert(hdev);

    hdev->id = id;
    hdev->dman = &io_ops->dma_manager;

    /* Check device mappings */
    if (_usb_regs == NULL) {
        _usb_regs = GET_RESOURCE(io_ops, USB2_HOST_EHCI);
    }
    if (_usb_regs == NULL) {
        return -1;
    }

    /* Initialise GPIOs */
    if (usb_plat_gpio_init(io_ops)) {
        return -1;
    }

    /* Initialise the phy */
    if (usb_init_phy(io_ops)) {
        assert(0);
        return -1;
    }

    err = ehci_host_init(hdev, (uintptr_t)_usb_regs, &board_pwren);

    return err;
}


int
usb_plat_otg_init(usb_otg_t odev, ps_io_ops_t* io_ops)
{
    return -1;
}


