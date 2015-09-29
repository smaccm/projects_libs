/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef SDIO_H
#define SDIO_H

/* NOT to be confused with SDHC. This API is universal */

#include <sdhc/plat/sdio.h>

/* TODO turn this into sdio_cmd */
struct mmc_cmd;
struct sdio_host_dev;
typedef void (*sdio_cb)(struct sdio_host_dev* sdio, struct mmc_cmd* cmd, void* token);

struct sdio_host_dev {
    int (*reset)(struct sdio_host_dev* sdio);
    int (*send_command)(struct sdio_host_dev* sdio, struct mmc_cmd *cmd, sdio_cb cb, void* token);
    int (*handle_irq)(struct sdio_host_dev* sdio, int irq);
    int (*is_voltage_compatible)(struct sdio_host_dev* sdio, int mv);
    void* priv;
};
typedef struct sdio_host_dev sdio_host_dev_t;


static inline int
sdio_send_command(sdio_host_dev_t* sdio, struct mmc_cmd *cmd, sdio_cb cb, void* token)
{
    return sdio->send_command(sdio, cmd, cb, token);
}

static inline int
sdio_handle_irq(sdio_host_dev_t* sdio, int irq)
{
    return sdio->handle_irq(sdio, irq);
}

static inline int
sdio_is_voltage_compatible(sdio_host_dev_t* sdio, int mv)
{
    return sdio->is_voltage_compatible(sdio, mv);
}

static inline int
sdio_reset(sdio_host_dev_t* sdio)
{
    return sdio->reset(sdio);
}

/**
 * Returns the ID of the default SDIO device for the current platform
 * @return  The default SDIO device for the platform
 */
enum sdio_id sdio_default_id(void);

/**
 * Initialises a platform sdio device
 * @param[in]  id     The ID of the sdio device to initialise
 * @param[in]  io_ops OS services to use during initialisation
 * @param[out] dev    An sdio structure to populate.
 * @return            0 on success
 */
int sdio_init(enum sdio_id id, ps_io_ops_t *io_ops, sdio_host_dev_t* dev);

#endif /* SDIO_H */
